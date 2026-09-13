#include <atomic>
#include <chrono>
#include <optional>
#include <thread>

#include "sdkconfig.h"

#include "asio.hpp"
#include "asio/connect.hpp"
#include "logger.hpp"
#include "wifi.hpp"

using asio::ip::tcp;

using namespace std::chrono_literals;

// Connect to a Wi-Fi router and return ip address
std::optional<esp_netif_ip_info_t> connectWifiRouter()
{
  espp::Logger logger{{.tag = "wifi", .level = espp::Logger::Verbosity::INFO}};

  logger.info("Connecting to Wi-Fi router");
  auto& wifi = espp::Wifi::get();
  wifi.set_log_level(espp::Logger::Verbosity::DEBUG);

  // Initialize the WiFi stack
  if (!wifi.init()) {
    logger.error("Failed to initialize WiFi stack");
    return std::nullopt;
  }

  esp_netif_ip_info_t ip_info;
  std::atomic_bool gotIp = false;
  wifi.register_sta("home",
                    {.ssid = CONFIG_ESP_WIFI_SSID,         // use whatever was saved to NVS (if any)
                     .password = CONFIG_ESP_WIFI_PASSWORD, // use whatever was saved to NVS (if any)
                     .num_connect_retries = CONFIG_ESP_MAXIMUM_RETRY,
                     .auto_connect = true,
                     .on_got_ip =
                       [&](ip_event_got_ip_t* eventdata)
                     {
                       ip_info = eventdata->ip_info;
                       gotIp = true;
                     },
                     .log_level = espp::Logger::Verbosity::INFO});

  wifi.switch_to_sta("home");

  // Wait for getting IP address for up to 15000ms
  int waitCount = 0;
  while (!gotIp && waitCount < 150) {
    std::this_thread::sleep_for(100ms);
    ++waitCount;
  }
  if (gotIp) {
    return ip_info;
  }
  return std::nullopt;
}

// send a message to a server
void ping(std::string_view server_ip, const unsigned short server_port)
{
  espp::Logger logger{{.tag = "tcp", .level = espp::Logger::Verbosity::INFO}};
  try {
    asio::io_context io_context;
    tcp::endpoint endpoint(asio::ip::make_address(server_ip), server_port);
    tcp::socket socket(io_context);
    socket.connect(endpoint);
    logger.info("Connected to {}:{}", server_ip, server_port);

    asio::write(socket, asio::buffer("ping!\n"));
    asio::error_code ec;
    socket.shutdown(tcp::socket::shutdown_both, ec);
    socket.close();
    logger.info("Connection closed");
  } catch (const std::exception& erre) {
    logger.error(erre.what());
  }
}

extern "C" void app_main(void)
{
  espp::Logger logger{{.tag = "main", .level = espp::Logger::Verbosity::INFO}};

  std::optional<esp_netif_ip_info_t> ip_info_o = connectWifiRouter();
  if (!ip_info_o) {
    logger.info("Failed to connect to Wi-Fi AP");
    vTaskDelete(NULL);
  }

  esp_netif_ip_info_t ip_info = ip_info_o.value();
  logger.info("ip: {}.{}.{}.{}", IP2STR(&ip_info.ip));
  logger.info("netmask: {}.{}.{}.{}", IP2STR(&ip_info.netmask));
  logger.info("gw: {}.{}.{}.{}", IP2STR(&ip_info.gw));

  ping(CONFIG_SERVER_IP_ADDRESS, atoi(CONFIG_SERVER_PORT));

  while (true) {
    std::this_thread::sleep_for(1s);
  }
}
