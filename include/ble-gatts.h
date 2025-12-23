/**
 * @file ble-gatts.h
 * @author Pedro Luis Dionísio Fraga (pedrodfraga@hotmail.com)
 * @brief GATT Server internal API - manages characteristics and events
 * @version 0.2
 * @date 2025-07-20
 *
 * @copyright Copyright (c) 2025
 *
 * This is an internal header. Users should use ble.h instead.
 */

#ifndef BLE_GATTS_H
#define BLE_GATTS_H

#include <esp_err.h>
#include <stddef.h>
#include <stdint.h>

#include "ble.h"

/**
 * @brief Initialize GATTS with user-defined characteristics
 *
 * @param chars Array of characteristic definitions
 * @param count Number of characteristics
 * @param service_uuid 16-bit service UUID
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ble_gatts_init(ble_characteristic_t *chars, size_t count, uint16_t service_uuid);

/**
 * @brief Deinitialize GATTS and free resources
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ble_gatts_deinit();

/**
 * @brief Check if a BLE client is currently connected
 *
 * @return true if connected, false otherwise
 */
bool ble_gatts_is_connected(void);

#endif  // BLE_GATTS_H
