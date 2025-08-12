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

#define MAX_MTU_SIZE         500
#define GATTS_NUM_HANDLES    5
#define MAX_PROFILES         5
#define FIRST_PROFILE_APP_ID 0

static const char *GATTS_TAG = "BLE_GATTS";

static uint16_t my_service_uuid = 0x180A;
static uint16_t my_char_uuid = 0x2A57;

static uint8_t char_property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE;
static uint8_t char_value[5] = {'P', 'E', 'D', 'R', 'O'};
static uint8_t user_desc_value[] = "Atribute name test";

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param);

static uint8_t s_num_profiles = 0;
static ble_profile_t s_profiles[MAX_PROFILES] = {
  [FIRST_PROFILE_APP_ID] =
    {
      .gatts_cb = gatts_profile_event_handler,
      .gatts_if = ESP_GATT_IF_NONE,
      .profile_name = "Default Profile",
    },
};

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
  if (event == ESP_GATTS_REG_EVT)
  {
    if (param->reg.status == ESP_GATT_OK)
    {
      ESP_LOGI(GATTS_TAG, "Register app success, app_id %d, gatts_if %d", param->reg.app_id, gatts_if);
      s_profiles[0].gatts_if = gatts_if;
    }
    else
    {
      ESP_LOGE(GATTS_TAG, "Reg app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
      return;
    }
  }

  for (int idx = 0; idx < s_num_profiles; idx++)
  {
    if (gatts_if == ESP_GATT_IF_NONE || gatts_if == s_profiles[idx].gatts_if)
    {
      ESP_LOGE(GATTS_TAG,
               "Checking profile '%s' with profile_gatts_if %d, gatts_if %d",
               s_profiles[idx].profile_name,
               s_profiles[idx].gatts_if,
               gatts_if);
      // ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function
      if (gatts_if == ESP_GATT_IF_NONE || gatts_if == s_profiles[idx].gatts_if)
      {
        if (s_profiles[idx].gatts_cb)
        {
          ESP_LOGI(GATTS_TAG, "Calling profile '%s' callback for event %d", s_profiles[idx].profile_name, event);
          s_profiles[idx].gatts_cb(event, gatts_if, param);
        }
      }
      ESP_LOGW(GATTS_TAG,
               "Skipping profile '%s' for event %d, gatts_if %d does not match",
               s_profiles[idx].profile_name,
               event,
               gatts_if);
    }
  }
}

esp_err_t ble_gatts_add_profile(ble_profile_t *profile)
{
  ESP_LOGI(GATTS_TAG, "Adding profile '%s'", profile->profile_name);
  ESP_LOGI(GATTS_TAG, "Number of profiles before: %d", s_num_profiles);

  if (s_num_profiles >= MAX_PROFILES)
    return ESP_ERR_NO_MEM;

  profile->app_id = FIRST_PROFILE_APP_ID + s_num_profiles;
  profile->gatts_if = ESP_GATT_IF_NONE;
  memcpy(&s_profiles[s_num_profiles], profile, sizeof(ble_profile_t));

  if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED)
    return ESP_ERR_INVALID_STATE;

  esp_ble_gatts_app_register(profile->app_id);
  s_num_profiles++;
  ESP_LOGI(GATTS_TAG, "Added profile '%s', app_id: %d", profile->profile_name, profile->app_id);

  return ESP_OK;
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                        esp_ble_gatts_cb_param_t *param)
{
  esp_err_t ret = ESP_FAIL;

  ESP_LOGE(GATTS_TAG, "gatts_profile_event_handler: event=%d, gatts_if=%d", event, gatts_if);
  switch (event)
  {
    case ESP_GATTS_REG_EVT:  // Called when the GATT server is registered
    {
      ESP_LOGI(GATTS_TAG, "GATTS registered, app_id: %d, gatts_if: %d", param->reg.app_id, gatts_if);

      s_profiles[0].service_id_info.is_primary = true;
      s_profiles[0].service_id_info.id.inst_id = 0x00;
      s_profiles[0].service_id_info.id.uuid.len = ESP_UUID_LEN_16;
      s_profiles[0].service_id_info.id.uuid.uuid.uuid16 = my_service_uuid;

      esp_ble_gatts_create_service(gatts_if, &s_profiles[0].service_id_info, GATTS_NUM_HANDLES);

      ESP_LOGI(GATTS_TAG, "Service created with UUID: 0x%04x", my_service_uuid);
      break;
    }
    case ESP_GATTS_CREATE_EVT:
    {
      s_profiles[0].service_handle = param->create.service_handle;

      esp_ble_gatts_start_service(s_profiles[0].service_handle);

      s_profiles[0].char_uuid.len = ESP_UUID_LEN_16;
      s_profiles[0].char_uuid.uuid.uuid16 = my_char_uuid;

      char_property = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY;

      esp_err_t add_char_ret = esp_ble_gatts_add_char(s_profiles[0].service_handle,
                                                      &s_profiles[0].char_uuid,
                                                      ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                      char_property,
                                                      NULL,  // valor inicial
                                                      NULL   // descritor
      );
      if (add_char_ret)
      {
        ESP_LOGE(GATTS_TAG, "add char failed, error code =%x", add_char_ret);
      }

      break;
    }
    case ESP_GATTS_ADD_CHAR_EVT:
    {
      s_profiles[0].char_handle = param->add_char.attr_handle;

      s_profiles[0].char_uuid.len = ESP_UUID_LEN_16;
      s_profiles[0].char_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

      esp_err_t add_descr_ret = esp_ble_gatts_add_char_descr(s_profiles[0].service_handle,
                                                             &s_profiles[0].char_uuid,
                                                             ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                             NULL,
                                                             NULL);
      if (add_descr_ret)
        ESP_LOGE(GATTS_TAG, "add char descr failed, error code =%x", add_descr_ret);

      break;
    }
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
    {
      s_profiles[0].descr_handle = param->add_char_descr.attr_handle;
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
               "Characteristic read, conn_id %d, trans_id %lu, handle %d",
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
        // Atualize o valor da característica
        memcpy(char_value, param->write.value, param->write.len);
      }
      esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
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

  ret = ble_gatts_add_profile(&s_profiles[0]);
  if (ret != ESP_OK)
    return ret;

  return ESP_OK;
}
