/**
 * @file ble.c
 * @author Pedro Luis Dionísio Fraga (pedrodfraga@hotmail.com)
 * @brief Creates a BLE GATT server with a custom service and characteristic.
 * @version 0.1
 * @date 2025-06-19
 *
 * @copyright Copyright (c) 2025
 *
 */

// Implements BT controller and VHCI configuration procedures from the host side.
#include <esp_bt.h>
#include <esp_bt_device.h>
// Implements initialization and enabling of the Bluedroid stack.
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>  // Implements GAP configuration such as advertising and connection parameters.
#include <esp_gatt_common_api.h>
#include <esp_gatt_defs.h>  // Implements GATT server API definitions.
#include <esp_log.h>
#include <string.h>

#include "ble-gap.h"
#include "ble-gatt.h"
#include "ble-gatts.h"
#include "ble.h"
#include "nvm_driver.h"

static const char *TAG = "BLE";

void ble_init()
{
  esp_err_t ret;

  // Initialize NVS
  nvm_init();

  ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

  // Initialize Bluetooth controller
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  ret = esp_bt_controller_init(&bt_cfg);
  if (ret)
  {
    ESP_LOGE(TAG, "Bluetooth controller initialization failed: %s", esp_err_to_name(ret));
    return;
  }

  // Enable Bluetooth controller
  ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
  if (ret)
  {
    ESP_LOGE(TAG, "Bluetooth controller enable failed: %s", esp_err_to_name(ret));
    return;
  }

  // Initialize Bluedroid stack
  ret = esp_bluedroid_init();
  if (ret)
  {
    ESP_LOGE(TAG, "Bluedroid initialization failed: %s", esp_err_to_name(ret));
    return;
  }

  // Enable Bluedroid stack
  ret = esp_bluedroid_enable();
  if (ret)
  {
    ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
    return;
  }

  // Separate GAP, GATT, and GATTS initialization
  if (ble_gatts_init() != ESP_OK)
    return;
  if (ble_gap_init("AIR-FRYER") != ESP_OK)
    return;
  if (ble_gatt_init() != ESP_OK)
    return;

  ESP_LOGI(TAG, "BLE GATT server initialized successfully");
}
