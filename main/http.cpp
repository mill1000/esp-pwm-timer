#include "esp_err.h"
#include "esp_log.h"

#include <string>
#include <vector>

#include "http.h"
#include "mongoose.h"
#include "json.h"

#define TAG "HTTP"

extern const unsigned char index_html_start[] asm("_binary_index_html_start");
extern const unsigned char index_html_end[]   asm("_binary_index_html_end");

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
  switch(ev)
  {
    case MG_EV_HTTP_REQUEST:
    {
      struct http_message *hm = (struct http_message *) ev_data;
      
      char action[4];
      if (mg_get_http_var(&hm->query_string, "action", action, sizeof(action)) == -1)
      {
        uint32_t length = (uint32_t) (index_html_end - index_html_start);
        // No action requested. Serve the page
        mg_send_head(nc, 200, length, "Content-Type: text/html");
        mg_send(nc, (void*) index_html_start, length);
        nc->flags |= MG_F_SEND_AND_CLOSE;    
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

    default:
      break;
  }
}

/**
  @brief  Main task function of the HTTP server
  
  @param  pvParameters
  @retval none
*/
void httpTask(void * pvParameters)
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

  // Loop waiting for events
  while(1)
    mg_mgr_poll(&manager, 1000);

  // Free the manager if we ever exit
  mg_mgr_free(&manager);

  vTaskDelete(NULL);
}