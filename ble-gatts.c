/**
 * @file ble-gatts.c
 * @author Pedro Luis Dionísio Fraga (pedrodfraga@hotmail.com)
 * @brief GATT Server implementation with characteristic abstraction
 * @version 0.2
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

#define GATTS_TAG "BLE_GATTS"

// Constants
#define MAX_CHARACTERISTICS  16
#define HANDLES_PER_CHAR     3  // 1 for char declaration + 1 for char value + 1 for descriptor
#define SERVICE_HANDLE_COUNT 1
#define MAX_MTU_SIZE         500
#define GATTS_APP_ID         0

// Calculate handles: service + (characteristics * handles_per_char)
#define CALC_NUM_HANDLES(char_count) (SERVICE_HANDLE_COUNT + ((char_count) * HANDLES_PER_CHAR))

// Internal structure to track characteristic handles
typedef struct
{
  uint16_t char_handle;       // Characteristic value handle
  uint16_t cccd_handle;       // CCCD handle (for notifications)
  uint16_t descr_handle;      // User description handle
  ble_characteristic_t *def;  // Pointer to user definition
} ble_char_handle_t;

// Module state
static ble_characteristic_t *s_characteristics = NULL;
static size_t s_char_count = 0;
static uint16_t s_service_uuid = 0;
static uint16_t s_service_handle = 0;
static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;
static uint16_t s_conn_id = 0;
static bool s_is_connected = false;

// Handle tracking
static ble_char_handle_t s_char_handles[MAX_CHARACTERISTICS];
static size_t s_registered_chars = 0;
static size_t s_pending_descr_char = 0;  // Index of char waiting for descriptor registration

// Forward declarations
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void handle_char_read(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void handle_char_write(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static ble_char_handle_t *find_char_by_handle(uint16_t handle);
static ble_char_handle_t *find_char_by_descr_handle(uint16_t handle);

/**
 * @brief Initialize GATTS with user-defined characteristics
 */
esp_err_t ble_gatts_init(ble_characteristic_t *chars, size_t count, uint16_t service_uuid)
{
  if (chars == NULL || count == 0)
  {
    ESP_LOGE(GATTS_TAG, "Invalid parameters");
    return ESP_ERR_INVALID_ARG;
  }

  if (count > MAX_CHARACTERISTICS)
  {
    ESP_LOGE(GATTS_TAG, "Too many characteristics (max %d)", MAX_CHARACTERISTICS);
    return ESP_ERR_NO_MEM;
  }

  s_characteristics = chars;
  s_char_count = count;
  s_service_uuid = service_uuid;
  s_registered_chars = 0;

  memset(s_char_handles, 0, sizeof(s_char_handles));

  esp_err_t ret = esp_ble_gatts_register_callback(gatts_event_handler);
  if (ret != ESP_OK)
  {
    ESP_LOGE(GATTS_TAG, "GATTS callback registration failed: %s", esp_err_to_name(ret));
    return ret;
  }

  ret = esp_ble_gatts_app_register(GATTS_APP_ID);
  if (ret != ESP_OK)
  {
    ESP_LOGE(GATTS_TAG, "GATTS app register failed: %s", esp_err_to_name(ret));
    return ret;
  }

  ret = esp_ble_gatt_set_local_mtu(MAX_MTU_SIZE);
  if (ret != ESP_OK)
  {
    ESP_LOGW(GATTS_TAG, "Set MTU failed: %s", esp_err_to_name(ret));
  }

  ESP_LOGI(GATTS_TAG, "GATTS initialized with %d characteristics", count);
  return ESP_OK;
}

/**
 * @brief Deinitialize GATTS
 */
esp_err_t ble_gatts_deinit()
{
  esp_err_t ret = esp_ble_gatts_app_unregister(s_gatts_if);
  if (ret != ESP_OK)
  {
    ESP_LOGE(GATTS_TAG, "App unregister failed: %s", esp_err_to_name(ret));
    return ret;
  }

  s_characteristics = NULL;
  s_char_count = 0;
  s_gatts_if = ESP_GATT_IF_NONE;
  s_registered_chars = 0;

  return ESP_OK;
}

/**
 * @brief Main GATTS event handler
 */
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
  switch (event)
  {
    case ESP_GATTS_REG_EVT:
    {
      if (param->reg.status != ESP_GATT_OK)
      {
        ESP_LOGE(GATTS_TAG, "App registration failed, status %d", param->reg.status);
        return;
      }

      s_gatts_if = gatts_if;
      ESP_LOGI(GATTS_TAG, "App registered, gatts_if %d", gatts_if);

      // Create the primary service
      esp_gatt_srvc_id_t service_id = {
        .is_primary = true,
        .id =
          {
            .inst_id = 0,
            .uuid =
              {
                .len = ESP_UUID_LEN_16,
                .uuid.uuid16 = s_service_uuid,
              },
          },
      };

      uint16_t num_handles = CALC_NUM_HANDLES(s_char_count);
      esp_ble_gatts_create_service(gatts_if, &service_id, num_handles);
      break;
    }

    case ESP_GATTS_CREATE_EVT:
    {
      if (param->create.status != ESP_GATT_OK)
      {
        ESP_LOGE(GATTS_TAG, "Service creation failed, status %d", param->create.status);
        return;
      }

      s_service_handle = param->create.service_handle;
      ESP_LOGI(GATTS_TAG, "Service created, handle %d", s_service_handle);

      // Start the service
      esp_ble_gatts_start_service(s_service_handle);

      // Add first characteristic
      if (s_char_count > 0)
      {
        ble_characteristic_t *ch = &s_characteristics[0];

        // Determine properties based on handlers
        esp_gatt_char_prop_t props = 0;
        if (ch->read != NULL)
          props |= ESP_GATT_CHAR_PROP_BIT_READ;
        if (ch->write != NULL)
          props |= ESP_GATT_CHAR_PROP_BIT_WRITE;

        // Determine permissions
        esp_gatt_perm_t perms = 0;
        if (ch->read != NULL)
          perms |= ESP_GATT_PERM_READ;
        if (ch->write != NULL)
          perms |= ESP_GATT_PERM_WRITE;

        esp_bt_uuid_t char_uuid = {
          .len = ESP_UUID_LEN_16,
          .uuid.uuid16 = ch->uuid,
        };

        esp_err_t ret = esp_ble_gatts_add_char(s_service_handle, &char_uuid, perms, props, NULL, NULL);
        if (ret != ESP_OK)
          ESP_LOGE(GATTS_TAG, "Add char failed: %s", esp_err_to_name(ret));
        else
          ESP_LOGI(GATTS_TAG, "Adding characteristic '%s' (UUID: 0x%04X)", ch->name, ch->uuid);
      }
      break;
    }

    case ESP_GATTS_ADD_CHAR_EVT:
    {
      if (param->add_char.status != ESP_GATT_OK)
      {
        ESP_LOGE(GATTS_TAG, "Add char failed, status %d", param->add_char.status);
        return;
      }

      // Store handle for this characteristic
      if (s_registered_chars < s_char_count)
      {
        s_char_handles[s_registered_chars].char_handle = param->add_char.attr_handle;
        s_char_handles[s_registered_chars].def = &s_characteristics[s_registered_chars];

        ESP_LOGI(GATTS_TAG,
                 "Characteristic added: '%s' handle=%d",
                 s_characteristics[s_registered_chars].name,
                 param->add_char.attr_handle);

        // Add User Description descriptor if description is provided
        ble_characteristic_t *current_ch = &s_characteristics[s_registered_chars];
        if (current_ch->description != NULL && current_ch->description[0] != '\0')
        {
          s_pending_descr_char = s_registered_chars;

          esp_bt_uuid_t descr_uuid = {
            .len = ESP_UUID_LEN_16,
            .uuid.uuid16 = ESP_GATT_UUID_CHAR_DESCRIPTION,  // 0x2901 - Characteristic User Description
          };

          esp_attr_value_t descr_value = {
            .attr_max_len = strlen(current_ch->description),
            .attr_len = strlen(current_ch->description),
            .attr_value = (uint8_t *)current_ch->description,
          };

          esp_err_t ret =
            esp_ble_gatts_add_char_descr(s_service_handle, &descr_uuid, ESP_GATT_PERM_READ, &descr_value, NULL);
          if (ret != ESP_OK)
          {
            ESP_LOGE(GATTS_TAG, "Add char descr failed: %s", esp_err_to_name(ret));
          }
          else
          {
            ESP_LOGI(GATTS_TAG, "Adding descriptor for '%s': \"%s\"", current_ch->name, current_ch->description);
          }
        }
        else
        {
          // No descriptor, proceed to register characteristic and move to next
          s_registered_chars++;

          // Add next characteristic if available
          if (s_registered_chars < s_char_count)
          {
            ble_characteristic_t *ch = &s_characteristics[s_registered_chars];

            esp_gatt_char_prop_t props = 0;
            if (ch->read != NULL)
              props |= ESP_GATT_CHAR_PROP_BIT_READ;
            if (ch->write != NULL)
              props |= ESP_GATT_CHAR_PROP_BIT_WRITE;

            esp_gatt_perm_t perms = 0;
            if (ch->read != NULL)
              perms |= ESP_GATT_PERM_READ;
            if (ch->write != NULL)
              perms |= ESP_GATT_PERM_WRITE;

            esp_bt_uuid_t char_uuid = {
              .len = ESP_UUID_LEN_16,
              .uuid.uuid16 = ch->uuid,
            };

            esp_ble_gatts_add_char(s_service_handle, &char_uuid, perms, props, NULL, NULL);
            ESP_LOGI(GATTS_TAG, "Adding characteristic '%s' (UUID: 0x%04X)", ch->name, ch->uuid);
          }
          else
          {
            ESP_LOGI(GATTS_TAG, "All %d characteristics registered", s_registered_chars);
          }
        }
      }
      break;
    }

    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
    {
      if (param->add_char_descr.status != ESP_GATT_OK)
      {
        ESP_LOGE(GATTS_TAG, "Add char descriptor failed, status %d", param->add_char_descr.status);
        return;
      }

      // Store descriptor handle
      if (s_pending_descr_char < s_char_count)
      {
        s_char_handles[s_pending_descr_char].descr_handle = param->add_char_descr.attr_handle;

        ESP_LOGI(GATTS_TAG,
                 "Descriptor added for '%s' handle=%d",
                 s_characteristics[s_pending_descr_char].name,
                 param->add_char_descr.attr_handle);

        s_registered_chars++;

        // Add next characteristic if available
        if (s_registered_chars < s_char_count)
        {
          ble_characteristic_t *ch = &s_characteristics[s_registered_chars];

          esp_gatt_char_prop_t props = 0;
          if (ch->read != NULL)
            props |= ESP_GATT_CHAR_PROP_BIT_READ;
          if (ch->write != NULL)
            props |= ESP_GATT_CHAR_PROP_BIT_WRITE;

          esp_gatt_perm_t perms = 0;
          if (ch->read != NULL)
            perms |= ESP_GATT_PERM_READ;
          if (ch->write != NULL)
            perms |= ESP_GATT_PERM_WRITE;

          esp_bt_uuid_t char_uuid = {
            .len = ESP_UUID_LEN_16,
            .uuid.uuid16 = ch->uuid,
          };

          esp_ble_gatts_add_char(s_service_handle, &char_uuid, perms, props, NULL, NULL);
          ESP_LOGI(GATTS_TAG, "Adding characteristic '%s' (UUID: 0x%04X)", ch->name, ch->uuid);
        }
        else
        {
          ESP_LOGI(GATTS_TAG, "All %d characteristics registered", s_registered_chars);
        }
      }
      break;
    }

    case ESP_GATTS_CONNECT_EVT:
    {
      s_conn_id = param->connect.conn_id;
      s_is_connected = true;

      ESP_LOGI(GATTS_TAG,
               "Client connected, conn_id=%d, remote=" ESP_BD_ADDR_STR,
               param->connect.conn_id,
               ESP_BD_ADDR_HEX(param->connect.remote_bda));

      // Update connection parameters
      ble_gap_update_connection_params(param->connect.remote_bda, 0x20, 0x40, 0, 400);
      break;
    }

    case ESP_GATTS_DISCONNECT_EVT:
    {
      s_is_connected = false;

      ESP_LOGI(GATTS_TAG, "Client disconnected, reason=0x%x", param->disconnect.reason);

      // Restart advertising
      ble_gap_start_adv();
      break;
    }

    case ESP_GATTS_READ_EVT:
    {
      handle_char_read(gatts_if, param);
      break;
    }

    case ESP_GATTS_WRITE_EVT:
    {
      handle_char_write(gatts_if, param);
      break;
    }

    case ESP_GATTS_MTU_EVT:
    {
      ESP_LOGI(GATTS_TAG, "MTU updated to %d", param->mtu.mtu);
      break;
    }

    case ESP_GATTS_START_EVT:
    {
      ESP_LOGI(GATTS_TAG, "Service started, status %d", param->start.status);
      break;
    }

    default:
    {
      ESP_LOGD(GATTS_TAG, "Unhandled event: %d", event);
      break;
    }
  }
}

/**
 * @brief Handle characteristic read request
 */
static void handle_char_read(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
  // First check if this is a descriptor read
  ble_char_handle_t *ch_descr = find_char_by_descr_handle(param->read.handle);
  if (ch_descr != NULL)
  {
    // This is a read request for a descriptor (User Description)
    esp_gatt_rsp_t rsp = {0};
    rsp.attr_value.handle = param->read.handle;

    if (ch_descr->def->description != NULL)
    {
      size_t total_len = strlen(ch_descr->def->description);
      uint16_t offset = param->read.offset;

      // Handle long read with offset
      if (offset >= total_len)
      {
        // All data has been sent
        rsp.attr_value.len = 0;
      }
      else
      {
        size_t remaining = total_len - offset;
        size_t to_send = remaining;
        if (to_send > sizeof(rsp.attr_value.value))
          to_send = sizeof(rsp.attr_value.value);

        memcpy(rsp.attr_value.value, ch_descr->def->description + offset, to_send);
        rsp.attr_value.len = to_send;
        rsp.attr_value.offset = offset;

        ESP_LOGI(GATTS_TAG,
                 "Sending descriptor for '%s' (offset=%d, len=%d/%d)",
                 ch_descr->def->name,
                 offset,
                 to_send,
                 total_len);
      }
    }
    else
    {
      rsp.attr_value.len = 0;
    }

    esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
    return;
  }

  // Check if this is a characteristic value read
  ble_char_handle_t *ch = find_char_by_handle(param->read.handle);

  if (ch == NULL)
  {
    ESP_LOGW(GATTS_TAG, "Read request for unknown handle %d", param->read.handle);
    esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_INVALID_HANDLE, NULL);
    return;
  }

  ESP_LOGI(GATTS_TAG, "Read request for '%s'", ch->def->name);

  esp_gatt_rsp_t rsp = {0};
  rsp.attr_value.handle = param->read.handle;

  if (ch->def->read == NULL)
  {
    // Write-only characteristic
    ESP_LOGW(GATTS_TAG, "Characteristic '%s' is write-only", ch->def->name);
    esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_READ_NOT_PERMIT, NULL);
    return;
  }

  // Call user's read handler
  int bytes_read = ch->def->read(rsp.attr_value.value, sizeof(rsp.attr_value.value));

  if (bytes_read < 0)
  {
    ESP_LOGE(GATTS_TAG, "Read handler error for '%s'", ch->def->name);
    esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_ERROR, NULL);
    return;
  }

  rsp.attr_value.len = bytes_read;

  ESP_LOGI(GATTS_TAG, "Sending %d bytes for '%s'", bytes_read, ch->def->name);
  ESP_LOG_BUFFER_HEX_LEVEL(GATTS_TAG, rsp.attr_value.value, bytes_read, ESP_LOG_DEBUG);

  esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
}

/**
 * @brief Handle characteristic write request
 */
static void handle_char_write(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
  ble_char_handle_t *ch = find_char_by_handle(param->write.handle);

  if (ch == NULL)
  {
    ESP_LOGW(GATTS_TAG, "Write request for unknown handle %d", param->write.handle);
    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INVALID_HANDLE, NULL);
    return;
  }

  ESP_LOGI(GATTS_TAG, "Write request for '%s', len=%d", ch->def->name, param->write.len);
  ESP_LOG_BUFFER_HEX_LEVEL(GATTS_TAG, param->write.value, param->write.len, ESP_LOG_DEBUG);

  if (ch->def->write == NULL)
  {
    // Read-only characteristic
    ESP_LOGW(GATTS_TAG, "Characteristic '%s' is read-only", ch->def->name);
    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_WRITE_NOT_PERMIT, NULL);
    return;
  }

  // Call user's write handler
  ble_char_error_t result = ch->def->write(param->write.value, param->write.len);

  // Map error code to GATT status
  esp_gatt_status_t status;
  switch (result)
  {
    case BLE_CHAR_OK:
      status = ESP_GATT_OK;
      ESP_LOGI(GATTS_TAG, "Write to '%s' successful", ch->def->name);
      break;
    case BLE_CHAR_ERR_SIZE:
      status = ESP_GATT_INVALID_ATTR_LEN;
      ESP_LOGW(GATTS_TAG, "Write to '%s' failed: invalid size", ch->def->name);
      break;
    case BLE_CHAR_ERR_VALUE:
      status = ESP_GATT_OUT_OF_RANGE;
      ESP_LOGW(GATTS_TAG, "Write to '%s' failed: value out of range", ch->def->name);
      break;
    case BLE_CHAR_ERR_READONLY:
      status = ESP_GATT_WRITE_NOT_PERMIT;
      ESP_LOGW(GATTS_TAG, "Write to '%s' failed: read-only", ch->def->name);
      break;
    case BLE_CHAR_ERR_BUSY:
      status = ESP_GATT_BUSY;
      ESP_LOGW(GATTS_TAG, "Write to '%s' failed: busy", ch->def->name);
      break;
    default:
      status = ESP_GATT_ERROR;
      ESP_LOGE(GATTS_TAG, "Write to '%s' failed: unknown error %d", ch->def->name, result);
      break;
  }

  // Send response if needed
  if (param->write.need_rsp)
  {
    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, NULL);
  }
}

/**
 * @brief Find characteristic by attribute handle
 */
static ble_char_handle_t *find_char_by_handle(uint16_t handle)
{
  for (size_t i = 0; i < s_char_count; i++)
  {
    if (s_char_handles[i].char_handle == handle)
    {
      return &s_char_handles[i];
    }
  }
  return NULL;
}

/**
 * @brief Find characteristic by descriptor handle
 */
static ble_char_handle_t *find_char_by_descr_handle(uint16_t handle)
{
  for (size_t i = 0; i < s_char_count; i++)
  {
    if (s_char_handles[i].descr_handle != 0 && s_char_handles[i].descr_handle == handle)
    {
      return &s_char_handles[i];
    }
  }
  return NULL;
}

/**
 * @brief Check if a BLE client is currently connected
 */
bool ble_gatts_is_connected(void)
{
  return s_is_connected;
}
