#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "lwip/netdb.h"
#include "cJSON.h"

#include "esp_netif_sntp.h"

#include "wifi_sta.h"

#define RX_BUF_SIZE 1024

#define WEB_HOST "api.open-meteo.com"
#define WEB_PORT "80"
#define WEB_PATH "/v1/forecast?latitude=41.67&longitude=-86.25&current_weather=true"
#define WEB_FAMILY AF_UNSPEC

//HTTP GET request
static const char *REQUEST = "GET " WEB_PATH " HTTP/1.0\r\n"
  "Host: " WEB_HOST ":" WEB_PORT "\r\n"
  "User-Agent: esp-idf/1.0 esp32\r\n"
  "\r\n";

//Set timeouts
#define CONNECTION_TIMEOUT_SEC 10
#define SOCKET_TIMEOUT_SEC 5


static const char *TAG = "Wifi Com";

static esp_err_t esp_ret;
static int ret;
static struct addrinfo *dns_res;
static int sock;
static char recv_buf[RX_BUF_SIZE];
static uint32_t recv_total;
static ssize_t recv_len;
static EventGroupHandle_t network_event_group;
static EventBits_t network_event_bits;

static struct addrinfo hints;
static struct timeval sock_timeout;

static esp_sntp_config_t config;

int parse_weather_json(const char *json_str) {
  cJSON *root = cJSON_Parse(json_str);
  if (root == NULL) {
    ESP_LOGE(TAG, "Failed to parse JSON");
    return -100;
  }

  cJSON *current_weather = cJSON_GetObjectItem(root, "current_weather");
  if (current_weather != NULL) {
    cJSON *temp_item = cJSON_GetObjectItem(current_weather, "temperature");
    if (cJSON_IsNumber(temp_item)) {
      ESP_LOGI(TAG, "Outside Temperature: %.1f°C", temp_item->valuedouble);
      cJSON_Delete(root); 
      return (int)round(temp_item->valuedouble);
    }
  }
  return -100;
  
}

void wifi_com_init(){


  //Hints for DNS lookup
  hints = (struct addrinfo){
    .ai_family = WEB_FAMILY,
    .ai_socktype = SOCK_STREAM,
  };

  // Socket timeout
  sock_timeout = (struct timeval){
    .tv_sec = SOCKET_TIMEOUT_SEC,
    .tv_usec = 0
  };

  //Welcome message
  ESP_LOGI(TAG, "Starting Wifi Com");

  //Initialize event group
  network_event_group = xEventGroupCreate();

  //Initialize NVS: ESP32 WiFi driver uses NVS to store WiFi settings
  //Erase NVS partition if it's out of free space or new version
  esp_ret = nvs_flash_init();
  if((esp_ret == ESP_ERR_NVS_NO_FREE_PAGES) ||
     (esp_ret == ESP_ERR_NVS_NEW_VERSION_FOUND)){
    ESP_ERROR_CHECK(nvs_flash_erase());
    esp_ret = nvs_flash_init();
  }
  if(esp_ret != ESP_OK){
    ESP_LOGE(TAG, "Error (%d): Could not initialize NVS", esp_ret);
    abort();
  } 

    // Initialize TCP/IP network interface (only call once in application)
    // Must be called prior to initializing the network driver!
    esp_ret = esp_netif_init();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%d): Failed to initialize network interface", esp_ret);
        abort();
    }

  // Create default event loop that runs in the background
  // Must be running prior to initializing the network driver!
  esp_ret = esp_event_loop_create_default();
  if (esp_ret != ESP_OK) {
      ESP_LOGE(TAG, "Error (%d): Failed to create default event loop", esp_ret);
      abort();
  }

  // Initialize network connection
  esp_ret = wifi_sta_init(network_event_group);
  if (esp_ret != ESP_OK) {
      ESP_LOGE(TAG, "Error (%d): Failed to initialize WiFi", esp_ret);
      abort();
  }

  config = (esp_sntp_config_t)ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
  esp_netif_sntp_init(&config);

  //set timezone to south bend time
  setenv("TZ","EST5EDT,M3.2.0/2,M11.1.0/2", 1);
  tzset();
}


bool wait_for_connection(){
  ESP_LOGI(TAG, "Waiting for network to connect...");
  network_event_bits = xEventGroupWaitBits(network_event_group, 
                                            WIFI_STA_CONNECTED_BIT, 
                                            pdFALSE, 
                                            pdTRUE, 
                                            pdMS_TO_TICKS(CONNECTION_TIMEOUT_SEC*1000));
  if (network_event_bits & WIFI_STA_CONNECTED_BIT) {
      ESP_LOGI(TAG, "Connected to WiFi network");
  } else {
      ESP_LOGE(TAG, "Failed to connect to network");
      return false;
  }

  // Wait for IP address
  ESP_LOGI(TAG, "Waiting for IP address...");
  network_event_bits = xEventGroupWaitBits(network_event_group, 
                                            WIFI_STA_IPV4_OBTAINED_BIT | 
                                            WIFI_STA_IPV6_OBTAINED_BIT, 
                                            pdFALSE, 
                                            pdFALSE, 
                                            pdMS_TO_TICKS(CONNECTION_TIMEOUT_SEC*1000));
  if (network_event_bits & WIFI_STA_IPV4_OBTAINED_BIT) {
    ESP_LOGI(TAG, "Connected to IPv4 network");
  } else if (network_event_bits & WIFI_STA_IPV6_OBTAINED_BIT) {
    ESP_LOGI(TAG, "Connected to IPv6 network");
  } else {
    ESP_LOGE(TAG, "Failed to obtain IP address");
    return false;
  }
  return true;
}

int wifi_get_temp(){
  network_event_bits = xEventGroupGetBits(network_event_group);
  if (!(network_event_bits & WIFI_STA_CONNECTED_BIT) ||
    !((network_event_bits & WIFI_STA_IPV4_OBTAINED_BIT) ||
    (network_event_bits & WIFI_STA_IPV6_OBTAINED_BIT))) {
    ESP_LOGI(TAG, "Network connection not established yet.");
    if (!wait_for_connection(network_event_group, 
                             CONNECTION_TIMEOUT_SEC)) {
      ESP_LOGE(TAG, "Failed to connect to WiFi. Reconnecting...");
      esp_ret = wifi_sta_reconnect();
      if (esp_ret != ESP_OK) {
          ESP_LOGE(TAG, "Failed to reconnect WiFi (%d)", esp_ret);
      }
      return -100;
    }
  }

  // DNS lookup
  ret = getaddrinfo(WEB_HOST, WEB_PORT, &hints, &dns_res);
  if(ret != 0 || dns_res == NULL){
    ESP_LOGE(TAG, "Error (%d): DNS lookup failed", ret);
    return -100;
  }

  // Print resolved IP addresses
  ESP_LOGI(TAG, "DNS lookup succeeded. IP addresses:");
  for (struct addrinfo *addr = dns_res; addr != NULL; addr = addr->ai_next){
    
    if(addr->ai_family == AF_INET){
      struct in_addr *ip = &((struct sockaddr_in *) addr->ai_addr)->sin_addr;
      inet_ntop(AF_INET, ip, recv_buf, INET_ADDRSTRLEN);
      ESP_LOGI(TAG, " IPv4: %s", recv_buf);
    
    }else if (addr->ai_family == AF_INET6){
      struct in6_addr *ip = &((struct sockaddr_in6 *)addr->ai_addr)->sin6_addr;
      inet_ntop(AF_INET6, ip, recv_buf, INET6_ADDRSTRLEN);
      ESP_LOGI(TAG, " IPv6: %s", recv_buf);
    }
  }
    
  //Create a socket
  sock = socket(dns_res->ai_family, dns_res->ai_socktype, dns_res->ai_protocol);
  if(sock<0){
    ESP_LOGE(TAG, "Error (%d): Failed to create socket %s", errno, strerror(errno));
    return -100;
  }
    
  //Set socket send timeout
  ret = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &sock_timeout, sizeof(sock_timeout));
  if(ret < 0) {
    ESP_LOGE(TAG, "Error (%d): Failed to set socket send timeout: %s", errno, strerror(errno));
    close(sock);
    return -100;
  }

  //Set socket recieve timeout
  ret = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &sock_timeout, sizeof(sock_timeout));
  if(ret < 0) {
    ESP_LOGE(TAG, "Error (%d): Failed to set socket recieve timeout: %s", errno, strerror(errno));
    close(sock);
    return -100;
  }
    

  //Connect to server
  ret = connect(sock, dns_res->ai_addr, dns_res->ai_addrlen);
  if(ret < 0){
    ESP_LOGE(TAG, "Error (%d): Failed to connect to server: %s", errno, strerror(errno));
    close(sock);
    return -100;
  }

  //Delete the address info (prevents memory leaks)
  freeaddrinfo(dns_res);

  //Send HTTP GET
  ESP_LOGI(TAG, "Sending HTTP GET request...");
  ret = send(sock, REQUEST, strlen(REQUEST), 0 );
  if (ret < 0){
    ESP_LOGE(TAG, "Error (%d): Failed to send HTTP GET request: %s", errno, strerror(errno));
    close(sock);
    return -100;
  }

  //Print HTTP response
  ESP_LOGI(TAG, "HTTP response:");
  recv_total = 0;
  while(1){
    // Recieve data from the socket
    recv_len = recv(sock, recv_buf, sizeof(recv_buf)-1, 0); // -1 for null char room

    //Check for errors
    if(recv_len < 0){
      ESP_LOGE(TAG, "Error (%d): Failed to recieve data: %s", errno, strerror(errno));
      break;
    }

    //Check for end of data
    if(recv_len == 0){
      break;
    }
    
    //Null terminate the recieved data 
    recv_buf[recv_len] = '\0';

    recv_total += (uint32_t)recv_len;
  }
  //Close the socket
  close(sock);

  //Print total bytes recieved
  printf("Total bytes recieved: %lu\n", recv_total);
  printf("\n");

  char *json_body = strstr(recv_buf, "\r\n\r\n");
  int temp_result = 0;
  if (json_body != NULL) {
    json_body += 4;  // Skip past \r\n\r\n
    temp_result = parse_weather_json(json_body);
  }


  if(temp_result == -100){
    return -100;
  }
  return temp_result;
  
}
