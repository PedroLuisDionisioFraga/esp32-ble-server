/**
 * @file ble-gatt.h
 * @author Pedro Luis Dionísio Fraga (pedrodfraga@hotmail.com)
 * @brief
 * @version 0.1
 * @date 2025-06-19
 *
 * @copyright Copyright (c) 2025
 *
 */

#ifndef BLE_GATT_H
#define BLE_GATT_H

#include <esp_bt_defs.h>
#include <esp_gatt_defs.h>
#include <esp_gatts_api.h>  // Implements GATT Server configuration such as creating services and characteristics.
#include <stdint.h>

typedef struct
{
  esp_gatts_cb_t gatts_cb;
  esp_gatt_if_t gatts_if;
  uint16_t app_id;
  uint16_t connection_id;
  esp_gatt_srvc_id_t service_id;
  uint16_t service_handle;
  esp_bt_uuid_t characteristic_uuid;
  uint16_t characteristic_handle;
  esp_gatt_perm_t perm;
  esp_gatt_char_prop_t property;
  uint16_t descr_handle;
  esp_bt_uuid_t descr_uuid;
  char *profile_name;
  // esp_gatts_attr_db_t *gatt_db;
} ble_gatts_profile_t;

esp_err_t ble_gatt_init();
esp_err_t ble_gatts_init();

#endif  // BLE_GATT_H
