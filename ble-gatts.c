/**
 * @file ble-gatts.c
 * @author Pedro Luis Dionísio Fraga (pedrodfraga@hotmail.com)
 * @brief
 * @version 0.1
 * @date 2025-07-20
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "ble-gatts.h"

#include <esp_bt_main.h>
#include <esp_err.h>
#include <esp_gatt_common_api.h>
#include <esp_gatt_defs.h>
#include <esp_gatts_api.h>
#include <esp_log.h>
#include <string.h>

#include "ble-gap.h"

#define MAX_MTU_SIZE      500
#define GATTS_NUM_HANDLES 8
#define MAX_PROFILES      5
// First profile constants
#define APP_ID                                    0
#define APP_SERVICE_UUID                          0x00FF  // Custom service UUID (avoid 0x1800-0x181F reserved UUIDs)
#define APP_CHARACTERISTIC_UUID                   0xFF01  // Custom characteristic UUID
#define APP_CHARACTERISTIC_PERMISSION             (esp_gatt_perm_t)(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE)
#define APP_CHARACTERISTIC_DESCRIPTION_UUID       0xFF02  // Custom characteristic description UUID
#define APP_CHARACTERISTIC_DESCRIPTION_PERMISSION (esp_gatt_perm_t)(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE)
#define APP_CHARACTERISTIC_PROPERTY                                                                                  \
  (esp_gatt_char_prop_t)(ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY)
#define APP_DESCRIPTION_UUID 0x2901  // Characteristic User Description (standard UUID)
#define APP_HANDLE           0x1

static const char *GATTS_TAG = "BLE_GATTS";

static uint8_t char_value[5] = {'P', 'E', 'D', 'R', 'O'};
static const char attribute_name[] = "Atribute name test";
static esp_attr_value_t user_desc_value = {
  .attr_max_len = sizeof(attribute_name),
  .attr_len = sizeof(attribute_name),
  .attr_value = (uint8_t *)attribute_name,
};

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param);

static uint8_t s_num_profiles = 0;
static ble_gatts_profile_t s_profiles[MAX_PROFILES];

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
  if (event == ESP_GATTS_REG_EVT)
  {
    if (param->reg.status == ESP_GATT_OK)
    {
      ESP_LOGI(GATTS_TAG, "Register app success, app_id %d, gatts_if %d", param->reg.app_id, gatts_if);
      s_profiles[param->reg.app_id].gatts_if = gatts_if;
    }
    else
    {
      ESP_LOGE(GATTS_TAG, "Reg app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
      return;
    }
  }

  for (int idx = 0; idx < s_num_profiles; idx++)
  {
    // ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function
    if (gatts_if == ESP_GATT_IF_NONE || gatts_if == s_profiles[idx].gatts_if)
    {
      if (s_profiles[idx].gatts_cb)
      {
        ESP_LOGI(GATTS_TAG, "Calling profile '%s' callback for event %d", s_profiles[idx].profile_name, event);
        s_profiles[idx].gatts_cb(event, gatts_if, param);
      }
      else
        ESP_LOGW(GATTS_TAG, "No callback registered for profile '%s'", s_profiles[idx].profile_name);

      continue;
    }
    ESP_LOGW(GATTS_TAG, "Skipping profile for event %d, gatts_if %d does not match", event, gatts_if);
  }
}

esp_err_t ble_gatts_add_profile(ble_gatts_profile_t *profile)
{
  ESP_LOGI(GATTS_TAG, "Adding profile '%s'", profile->profile_name);

  if (s_num_profiles >= MAX_PROFILES)
    return ESP_ERR_NO_MEM;

  profile->app_id = APP_ID + s_num_profiles;
  profile->gatts_if = ESP_GATT_IF_NONE;

  s_profiles[s_num_profiles] = *profile;
  s_num_profiles++;  // Increment BEFORE registering so the callback can find this profile

  if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED)
  {
    s_num_profiles--;  // Rollback if not enabled
    return ESP_ERR_INVALID_STATE;
  }

  esp_err_t ret = esp_ble_gatts_app_register(profile->app_id);
  if (ret != ESP_OK)
  {
    ESP_LOGE(GATTS_TAG, "gatts app register failed, error code = %x", ret);
    s_num_profiles--;  // Rollback on failure
    return ret;
  }

  ESP_LOGI(GATTS_TAG, "Added profile '%s', app_id: %d", profile->profile_name, profile->app_id);

  return ESP_OK;
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param)
{
  ESP_LOGI(GATTS_TAG, "gatts_profile_event_handler: event=%d, gatts_if=%d", event, gatts_if);
  switch (event)
  {
    case ESP_GATTS_CONNECT_EVT:
    {
      ESP_LOGI(GATTS_TAG,
               "GATT_CONNECT_EVT, connection_id %d, remote " ESP_BD_ADDR_STR "",
               param->connect.conn_id,
               ESP_BD_ADDR_HEX(param->connect.remote_bda));

      ble_gap_update_connection_params(param->connect.remote_bda, 0x20, 0x40, 0, 400);
      s_profiles[APP_ID].connection_id = param->connect.conn_id;
      break;
    }
    case ESP_GATTS_REG_EVT:
    {
      ESP_LOGI(GATTS_TAG, "GATTS registered, app_id: %d, gatts_if: %d", param->reg.app_id, gatts_if);

      s_profiles[APP_ID].service_id.is_primary = true;
      s_profiles[APP_ID].service_id.id.inst_id = 0x00;
      s_profiles[APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_16;
      s_profiles[APP_ID].service_id.id.uuid.uuid.uuid16 = APP_SERVICE_UUID;

      esp_ble_gatts_create_service(gatts_if, &s_profiles[APP_ID].service_id, GATTS_NUM_HANDLES);

      ESP_LOGI(GATTS_TAG, "Service created with UUID: 0x%04x", APP_SERVICE_UUID);
      break;
    }
    case ESP_GATTS_CREATE_EVT:
    {
      ESP_LOGI(GATTS_TAG,
               "Service create, status %d, service_handle %d",
               param->create.status,
               param->create.service_handle);

      s_profiles[APP_ID].service_handle = param->create.service_handle;
      s_profiles[APP_ID].characteristic_uuid.len = ESP_UUID_LEN_16;
      s_profiles[APP_ID].characteristic_uuid.uuid.uuid16 = APP_CHARACTERISTIC_UUID;

      esp_ble_gatts_start_service(s_profiles[APP_ID].service_handle);

      esp_err_t add_char_ret = esp_ble_gatts_add_char(s_profiles[APP_ID].service_handle,
                                                      &s_profiles[APP_ID].characteristic_uuid,
                                                      APP_CHARACTERISTIC_PERMISSION,
                                                      APP_CHARACTERISTIC_PROPERTY,
                                                      NULL,
                                                      NULL);
      if (add_char_ret != ESP_OK)
      {
        ESP_LOGE(GATTS_TAG, "add char failed, error code =%x", add_char_ret);
        return;
      }

      break;
    }
    case ESP_GATTS_ADD_CHAR_EVT:
    {
      ESP_LOGI(GATTS_TAG,
               "Characteristic add, status %d, attr_handle %d, service_handle %d",
               param->add_char.status,
               param->add_char.attr_handle,
               param->add_char.service_handle);

      s_profiles[APP_ID].characteristic_handle = param->add_char.attr_handle;
      // s_profiles[APP_ID].characteristic_uuid.len = ESP_UUID_LEN_16;
      // s_profiles[APP_ID].characteristic_uuid.uuid.uuid16 = APP_CHARACTERISTIC_DESCRIPTION_UUID;
      // UUID do descritor de User Description (0x2901 é o padrão BLE)
      esp_bt_uuid_t descr_uuid = {
        .len = ESP_UUID_LEN_16,
        .uuid.uuid16 = APP_DESCRIPTION_UUID,
      };

      uint16_t length = 0;
      const uint8_t *payload = NULL;
      esp_err_t get_attr_ret = esp_ble_gatts_get_attr_value(param->add_char.attr_handle, &length, &payload);
      if (get_attr_ret != ESP_OK)
      {
        ESP_LOGE(GATTS_TAG, "get attr value failed, error code =%x", get_attr_ret);
      }

      ESP_LOGI(GATTS_TAG, "Characteristic length = %x", length);
      for (int i = 0; i < length; i++)
        ESP_LOGI(GATTS_TAG, "prf_char[%x] =%x", i, payload[i]);

      esp_err_t add_descr_ret = esp_ble_gatts_add_char_descr(s_profiles[APP_ID].service_handle,
                                                             //&s_profiles[APP_ID].characteristic_uuid,
                                                             &descr_uuid,
                                                             APP_CHARACTERISTIC_DESCRIPTION_PERMISSION,
                                                             &user_desc_value,
                                                             NULL);
      if (add_descr_ret != ESP_OK)
        ESP_LOGE(GATTS_TAG, "add char descr failed, error code =%x", add_descr_ret);

      break;
    }
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
    {
      s_profiles[APP_ID].descr_handle = param->add_char_descr.attr_handle;
      ESP_LOGI(GATTS_TAG,
               "Descriptor added, status %d, attr_handle %d, service_handle %d",
               param->add_char_descr.status,
               param->add_char_descr.attr_handle,
               param->add_char_descr.service_handle);
      break;
    }
    case ESP_GATTS_READ_EVT:
    {
      ESP_LOGI(GATTS_TAG,
               "Characteristic read, connection_id %d, trans_id %lu, handle %d",
               param->read.conn_id,
               param->read.trans_id,
               param->read.handle);

      esp_gatt_rsp_t rsp;
      memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
      rsp.attr_value.handle = param->read.handle;
      rsp.attr_value.len = sizeof(char_value);
      memcpy(rsp.attr_value.value, char_value, sizeof(char_value));

      ESP_LOGI(GATTS_TAG, "Sending response with value: ");
      ESP_LOG_BUFFER_HEX(GATTS_TAG, rsp.attr_value.value, rsp.attr_value.len);

      esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
      break;
    }
    case ESP_GATTS_WRITE_EVT:
    {
      ESP_LOGI(GATTS_TAG,
               "Characteristic write, conn_id %d, trans_id %lu, handle %d",
               param->write.conn_id,
               param->write.trans_id,
               param->write.handle);

      if (!param->write.is_prep)
      {
        ESP_LOGI(GATTS_TAG, "value len %d, value ", param->write.len);
        ESP_LOG_BUFFER_HEX(GATTS_TAG, param->write.value, param->write.len);

        if (s_profiles[APP_ID].characteristic_handle == param->write.handle)
        {
          ESP_LOGI(GATTS_TAG, "Updating characteristic value");
          memcpy(char_value, param->write.value, param->write.len);

          // Check if notification or indication is enabled
          if (APP_CHARACTERISTIC_PROPERTY & (ESP_GATT_CHAR_PROP_BIT_NOTIFY | ESP_GATT_CHAR_PROP_BIT_INDICATE))
          {
            ESP_LOGI(GATTS_TAG, "Notification enable");
            // the size of notify_data[] need less than MTU size
            uint8_t notify_data[15];
            for (int i = 0; i < sizeof(notify_data); ++i)
              notify_data[i] = i % 0xff;

            bool need_confirm = (APP_CHARACTERISTIC_PROPERTY & ESP_GATT_CHAR_PROP_BIT_INDICATE) != 0;
            esp_ble_gatts_send_indicate(gatts_if,
                                        param->write.conn_id,
                                        s_profiles[APP_ID].characteristic_handle,
                                        sizeof(notify_data),
                                        notify_data,
                                        need_confirm);
          }
        }
      }
      esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
      break;
    }
    case ESP_GATTS_RESPONSE_EVT:
    {
      ESP_LOGI(GATTS_TAG, "Response event, status %d", param->rsp.status);
      break;
    }
    case ESP_GATTS_EXEC_WRITE_EVT:
    {
      ESP_LOGI(GATTS_TAG, "Exec write event");
      break;
    }
    case ESP_GATTS_MTU_EVT:
    {
      ESP_LOGI(GATTS_TAG, "MTU event, MTU %d", param->mtu.mtu);
      break;
    }
    case ESP_GATTS_DISCONNECT_EVT:
    {
      ESP_LOGI(GATTS_TAG, "Disconnect event, reason %d", param->disconnect.reason);
      ble_gap_start_adv();
      break;
    }
    default:
    {
      ESP_LOGI(GATTS_TAG, "Unhandled GATTS event: %d", event);
      break;
    }
  }
}

esp_err_t ble_gatts_init()
{
  esp_err_t ret;

  ret = esp_ble_gatts_register_callback(gatts_event_handler);
  if (ret)
    return ret;

  ret = ble_gatts_add_profile(&(ble_gatts_profile_t){
    .gatts_cb = gatts_profile_event_handler,
    .profile_name = "Default Profile",
  });
  if (ret != ESP_OK)
    return ret;

  return ESP_OK;
}
