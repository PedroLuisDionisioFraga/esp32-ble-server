/**
 * @file ble-gap.h
 * @author Pedro Luis Dionísio Fraga (pedrodfraga@hotmail.com)
 * @brief
 * @version 0.1
 * @date 2025-06-19
 *
 * @copyright Copyright (c) 2025
 *
 */

#ifndef BLE_GAP_H
#define BLE_GAP_H

#include <esp_err.h>

esp_err_t ble_gap_init(const char *device_name);

esp_err_t ble_gap_start_adv();

esp_err_t ble_gap_update_connection_params(uint8_t *bda, uint16_t min_interval, uint16_t max_interval, uint16_t latency,
                                           uint16_t timeout);

#endif  // BLE_GAP_H
