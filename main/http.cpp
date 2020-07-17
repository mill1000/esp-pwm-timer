#include "esp_err.h"
#include "esp_log.h"

#include <string>
#include <vector>
#include <queue>
#include <unordered_set>

#include "http.h"
#include "mongoose.h"
#include "json.h"
#include "ota_interface.h"

#define TAG "HTTP"

/**
  @brief  Sends HTTP responses on the provided connection
  
  @param  c Mongoose connection
  @param  code HTTP event code to send
  @param  error HTTP response body to send
  @retval none
*/
static void httpSendResponse(struct mg_connection* nc, uint16_t code, const char* error)
{
  mg_send_head(nc, code, strlen(error), "Content-Type: text/html");
  mg_printf(nc, error);
  nc->flags |= MG_F_SEND_AND_CLOSE;
}

/**
  @brief  Generic Mongoose event handler for the HTTP server
  
  @param  nc Mongoose connection
  @param  ev Mongoose event calling the function
  @param  ev_data Event data pointer
  @retval none
*/
static void httpEventHandler(struct mg_connection* nc, int ev, void* ev_data)
{
  static std::unordered_set<mg_connection*> ws_clients;
  constexpr double ws_interval = .5f; // 500 ms

  switch(ev)
  {
    case MG_EV_HTTP_REQUEST:
    {
      struct http_message *hm = (struct http_message *) ev_data;
      
      char action[4];
      if (mg_get_http_var(&hm->query_string, "action", action, sizeof(action)) == -1)
      {
        struct mg_serve_http_opts opts;

        memset(&opts, 0, sizeof(opts));
        opts.document_root = "/spiffs";
        opts.enable_directory_listing = "no";

        mg_serve_http(nc, (struct http_message *) ev_data, opts);
        break;
      }
      else if (strcmp(action, "get") == 0) // Get JSON values
      {
        std::string settings = JSON::get_settings();

        ESP_LOGI(TAG, "Get = %s", settings.c_str());

        mg_send_head(nc, 200, settings.length(), "Content-Type: application/json");
        mg_send(nc, settings.c_str(), settings.length());
        nc->flags |= MG_F_SEND_AND_CLOSE;   
    
        break;
      }
      else if (strcmp(action, "set") == 0) // Set JSON values
      {
        // Move JSON data into null terminated buffer
        std::string buffer(hm->body.p, hm->body.p + hm->body.len);

        ESP_LOGI(TAG, "Set = %s", buffer.c_str());

        // Parse settings JSON and reset buffer
        bool success = JSON::parse_settings(buffer);

        const char* errorString = success ? "Update succesful." : "JSON parse failed.";
        uint16_t returnCode = success ? 200 : 400;

        httpSendResponse(nc, returnCode, errorString);
      }
      else
      {
        mg_http_send_redirect(nc, 302, hm->uri, mg_mk_str(NULL));
        nc->flags |= MG_F_SEND_AND_CLOSE;   
      }
      break;
    }

    case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
    {
      char addr[32];
      mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
      ESP_LOGI(TAG, "Websocket connect from %s", addr);

      if (ws_clients.empty())
        mg_set_timer(nc, mg_time() + ws_interval);
      
      ws_clients.insert(nc);

      break;
    }

    case MG_EV_CLOSE:
    {
      if (nc->flags & MG_F_IS_WEBSOCKET)
      {
        char addr[32];
        mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
        ESP_LOGI(TAG, "Websocket disconnect from %s", addr);

        ws_clients.erase(nc);

        if (!ws_clients.empty() && nc->ev_timer_time != 0)
        {
          // Move the event timer to another connection if this connection was handling interval timing
          mg_set_timer(*ws_clients.begin(), nc->ev_timer_time);
        }
      }
      break;
    }

    case MG_EV_TIMER:
    {
      // Reset time to trigger
      double eventTime = *((double*) ev_data);
      mg_set_timer(nc, eventTime + ws_interval);

      std::string status = JSON::get_status();

      for (const auto &c : ws_clients)
      {
        if ((c->flags & MG_F_IS_WEBSOCKET) && (c->send_mbuf.len == 0))   
          mg_send_websocket_frame(c, WEBSOCKET_OP_TEXT, status.c_str(), status.length());
      }

      break;
    }

    default:
      break;
  }
}

/**
  @brief  Mongoose event handler for the OTA firmware update
  
  @param  nc Mongoose connection
  @param  ev Mongoose event calling the function
  @param  ev_data Event data pointer
  @retval none
*/
static void otaEventHandler(struct mg_connection* nc, int ev, void* ev_data)
{
  constexpr uint32_t MG_F_OTA_FAILED = MG_F_USER_1;
  constexpr uint32_t MG_F_OTA_COMPLETE = MG_F_USER_2;

  static std::queue<OTA::end_callback_t> callbacks;
  static std::string response;

  switch(ev)
  {
    case MG_EV_HTTP_REQUEST:
    {
      // Serve the page from the SPIFFS
      mg_http_serve_file(nc, (struct http_message *) ev_data, "/spiffs/ota.html", mg_mk_str("text/html"), mg_mk_str(""));
      break;
    }

    case MG_EV_HTTP_MULTIPART_REQUEST:
    {
      // Reset response string
      response.clear();
      break;
    }

    case MG_EV_HTTP_PART_BEGIN:
    {
      struct mg_http_multipart_part* multipart = (struct mg_http_multipart_part*) ev_data;

      //ESP_LOGI(TAG, "Filename %s, Varname %s", multipart->file_name, multipart->var_name);
      if (multipart->user_data != nullptr)
      {
        ESP_LOGE(TAG, "Non-null OTA handle. OTA already in progress?");

        // Mark OTA as failed and append reason
        response += "OTA update already in progress.";
        nc->flags |= MG_F_OTA_FAILED;
        return;
      }

      // Construct an OTA object based on the incoming file "target"
      std::string ota_target = std::string(multipart->var_name);
      OTA::Handle* ota = OTA::construct_handle(ota_target);
      if (ota == nullptr)
      {
        ESP_LOGE(TAG, "Failed to construct OTA handle for target '%s'.", ota_target.c_str());

        // Mark OTA as failed and append reason
        response += "OTA update init failed.";
        nc->flags |= MG_F_OTA_FAILED;
        return;
      }

      ESP_LOGI(TAG, "Starting %s OTA...", ota_target.c_str());
      esp_err_t result = ota->start();
      if (result != ESP_OK)
      {
        ESP_LOGE(TAG, "Failed to start OTA. Error: %s", esp_err_to_name(result));

        // Mark OTA as failed and append reason
        response += "OTA update init failed.";
        nc->flags |= MG_F_OTA_FAILED;

        // Kill the handle
        delete ota;
        multipart->user_data = nullptr;
        return;
      }

      // Save the handle for reference in future calls
      multipart->user_data = (void*) ota;
      break;
    }

    case MG_EV_HTTP_PART_DATA:
    {
      struct mg_http_multipart_part* multipart = (struct mg_http_multipart_part*) ev_data;

      // Something went wrong so ignore the data
      if (nc->flags & MG_F_OTA_FAILED)
        return;

      // Fetch handle from user_data
      OTA::Handle* ota = (OTA::Handle*) multipart->user_data;
      if (ota == nullptr)
        return;

      if (ota->write((uint8_t*) multipart->data.p, multipart->data.len) != ESP_OK)
      {
        // Mark OTA as failed and append reason
        response += "OTA write failed.";
        nc->flags |= MG_F_OTA_FAILED;
      }
      break;
    }

    case MG_EV_HTTP_PART_END:
    {
      struct mg_http_multipart_part* multipart = (struct mg_http_multipart_part*) ev_data;
      
      // Fetch handle from user_data
      OTA::Handle* ota = (OTA::Handle*) multipart->user_data;
      if (ota == nullptr)
        return;

      // Even if MG_F_OTA_FAILED is set we should let the OTA try to clean up

      ESP_LOGI(TAG, "Ending OTA...");
      OTA::end_result_t result = ota->end();

      // Save callback
      callbacks.push(result.callback);

      // Update response if error
      if (result.status != ESP_OK)
      {
        response += "OTA end failed.";
        nc->flags |= MG_F_OTA_FAILED;
      }

      // Free the handle object and reference
      delete ota;
      multipart->user_data = nullptr;
      break;
    }

    case MG_EV_HTTP_MULTIPART_REQUEST_END:
    {
      // Send the appropriate response status 
      if (nc->flags & MG_F_OTA_FAILED)
        httpSendResponse(nc, 500, response.c_str());
      else
        httpSendResponse(nc, 200, "OTA update successful.");

      // Signal OTA is complete
      nc->flags |= MG_F_OTA_COMPLETE;

      response.clear();
      break;
    }

    case MG_EV_CLOSE:
    {
      // Ignore close events that aren't after an OTA
      if ((nc->flags & MG_F_OTA_COMPLETE) != MG_F_OTA_COMPLETE)
       return;
      
      // Fire callbacks once OTA connection has closed
      while (callbacks.empty() == false)
      {
        OTA::end_callback_t callback = callbacks.front();
        if (callback)
          callback();

        callbacks.pop();
      }
    
     break; 
    }
  }
}

/**
  @brief  Main task function of the HTTP server
  
  @param  pvParameters
  @retval none
*/
void HTTP::task(void* pvParameters)
{
  ESP_LOGI(TAG, "Starting HTTP server...");

  // Create and init the event manager
  struct mg_mgr manager;
  mg_mgr_init(&manager, NULL);

  // Connect bind to an address and specify the event handler
  struct mg_connection* connection = mg_bind(&manager, "80", httpEventHandler);
  if (connection == NULL)
  {
    ESP_LOGE(TAG, "Failed to bind port.");
    mg_mgr_free(&manager);
    vTaskDelete(NULL);
    return;
  }

  // Enable HTTP on the connection
  mg_set_protocol_http_websocket(connection);

  // Special handler for OTA page
  mg_register_http_endpoint(connection, "/ota", otaEventHandler);

  // Loop waiting for events
  while(1)
    mg_mgr_poll(&manager, 1000);

  // Free the manager if we ever exit
  mg_mgr_free(&manager);

  vTaskDelete(NULL);
}