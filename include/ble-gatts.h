#ifndef BLE_GATTS_H
#define BLE_GATTS_H

#include <esp_bt_defs.h>
#include <esp_gatt_defs.h>
#include <esp_gatts_api.h>
#include <stdint.h>

#include "ble-gatt.h"

esp_err_t ble_gatts_init();
esp_err_t ble_gatts_add_profile(ble_gatts_profile_t *profile);

#endif  // BLE_GATTS_H
