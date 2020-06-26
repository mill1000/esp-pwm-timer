#include "esp_err.h"
#include "esp_log.h"

#include "http.h"
#include "mongoose.h"

#define TAG "WiFi"

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
  httpSendResponse(nc, 200, "OK it works");
  return;
}

void httpTask()
{
  ESP_LOGI(TAG, "Configuring Mongoose...");

  // Create and init the event manager
  struct mg_mgr manager;
  mg_mgr_init(&manager, NULL);

  // Connect bind to an address and specify the event handler
  struct mg_connection* connection = mg_bind(&manager, "80", httpEventHandler);
  if (connection == NULL)
  {
      ESP_LOGE(TAG, "Failed to bind to port 80.");
      mg_mgr_free(&manager);
      return;
  }

  // Enable WebSockets on the connection
  mg_set_protocol_http_websocket(connection);

  // Loop waiting for events
  while(1)
    mg_mgr_poll(&manager, 1000);

  // Free the manager if we ever exit
  mg_mgr_free(&manager);
}