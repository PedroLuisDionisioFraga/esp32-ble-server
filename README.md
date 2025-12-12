# 📡 BLE Component - Air Fryer

Bluetooth Low Energy (BLE) component for the FETIN 2025 Air Fryer project. This component implements a complete BLE GATT server using the ESP-IDF Bluedroid stack.

## 📋 Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Service UUIDs](#service-uuids)
- [Public API](#public-api)
- [Configuration](#configuration)
- [Dependencies](#dependencies)
- [Usage](#usage)
- [Author](#author)

## 🎯 Overview

This component provides a BLE interface for Air Fryer control and monitoring, enabling:

- **BLE Advertising**: Device visible for connections
- **GATT Server**: Services and characteristics for communication
- **Read/Write**: Bidirectional control via BLE characteristics
- **Notifications**: Asynchronous data transmission to connected devices

## 🏗️ Architecture

The component is divided into three main layers:

```
┌─────────────────────────────────────────┐
│              ble.c (Main)               │
│         Inicialização Principal         │
├─────────────────────────────────────────┤
│  ble-gap.c  │  ble-gatt.c  │ ble-gatts.c│
│   (GAP)     │   (GATT)     │  (Server)  │
├─────────────────────────────────────────┤
│         ESP-IDF Bluedroid Stack         │
└─────────────────────────────────────────┘
```

### Modules

| Module | Description |
|--------|-------------|
| **ble.c** | Main entry point, initializes BT controller and Bluedroid |
| **ble-gap.c** | Generic Access Profile - Manages advertising and connections |
| **ble-gatt.c** | Generic Attribute Profile - MTU configuration |
| **ble-gatts.c** | GATT Server - Services, characteristics and event handlers |

## 🔑 Service UUIDs

### Services

| UUID | Name | Description |
|------|------|-------------|
| `0x00FF` | Air Fryer Service | Main service for control and monitoring |
| `0xED58` | Advertising Service | UUID used in advertising data |
| `0xAFBD` | Scan Response Service | UUID used in scan response data |

### Characteristics

| UUID | Name | Properties | Description |
|------|------|------------|-------------|
| `0xFF01` | Control | Read/Write/Notify | Main control characteristic |

### Descriptors

| UUID | Name | Description |
|------|------|-------------|
| `0x2901` | Characteristic User Description | Human-readable characteristic name |
| `0x2902` | Client Characteristic Configuration | Notification configuration |

## 📚 Public API

### Initialization

```c
#include "ble.h"

// Initialize the entire BLE stack
void ble_init();
```

### GAP (Generic Access Profile)

```c
#include "ble-gap.h"

// Initialize GAP with device name
esp_err_t ble_gap_init(const char *device_name);

// Start advertising
esp_err_t ble_gap_start_adv();

// Update connection parameters
esp_err_t ble_gap_update_connection_params(
    uint8_t *bda,           // Bluetooth device address
    uint16_t min_interval,  // Minimum connection interval
    uint16_t max_interval,  // Maximum connection interval
    uint16_t latency,       // Slave latency
    uint16_t timeout        // Supervision timeout
);
```

### GATT Server

```c
#include "ble-gatts.h"

// Initialize GATT Server
esp_err_t ble_gatts_init();

// Add a new profile
esp_err_t ble_gatts_add_profile(ble_gatts_profile_t *profile);
```

### Profile Structure

```c
typedef struct {
    esp_gatts_cb_t gatts_cb;           // GATT event callback
    esp_gatt_if_t gatts_if;            // GATT interface
    uint16_t app_id;                   // Application ID
    uint16_t connection_id;            // Connection ID
    esp_gatt_srvc_id_t service_id;     // Service ID
    uint16_t service_handle;           // Service handle
    esp_bt_uuid_t characteristic_uuid; // Characteristic UUID
    uint16_t characteristic_handle;    // Characteristic handle
    esp_gatt_perm_t perm;              // Permissions
    esp_gatt_char_prop_t property;     // Properties
    uint16_t descr_handle;             // Descriptor handle
    esp_bt_uuid_t descr_uuid;          // Descriptor UUID
    char *profile_name;                // Profile name
} ble_gatts_profile_t;
```

## ⚙️ Configuration

### Advertising Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| `adv_int_min` | 0x20 (20ms) | Minimum advertising interval |
| `adv_int_max` | 0x40 (40ms) | Maximum advertising interval |
| `adv_type` | ADV_TYPE_IND | Connectable undirected advertising |
| `own_addr_type` | BLE_ADDR_TYPE_PUBLIC | Public address type |
| `channel_map` | ADV_CHNL_ALL | All advertising channels |

### GATT Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| `MAX_MTU_SIZE` | 500 | Maximum MTU size |
| `GATTS_NUM_HANDLES` | 8 | Number of GATT handles |
| `MAX_PROFILES` | 5 | Maximum number of profiles |

## 📦 Dependencies

This component depends on:

- **ESP-IDF**: Development framework (Bluedroid stack)
- **bt**: ESP-IDF Bluetooth component
- **nvm-driver**: Non-Volatile Memory driver (storage)

### CMakeLists.txt

```cmake
idf_component_register(SRCS "ble-gatts.c" "ble-gatt.c" "ble-gap.c" "ble.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES nvm-driver bt)
```

## 🚀 Usage

### Basic Example

```c
#include "ble.h"

void app_main(void)
{
    // Initialize BLE with name "AIR-FRYER"
    ble_init();

    // The device is now visible and ready for connections
}
```

### Initialization Flow

1. **NVM Init**: Initialize non-volatile memory (required for BT)
2. **BT Controller**: Configure and enable the Bluetooth controller
3. **Bluedroid**: Initialize and enable the Bluedroid stack
4. **GATTS Init**: Register the GATT Server event handler
5. **GAP Init**: Configure advertising and GAP parameters
6. **GATT Init**: Configure local MTU

### Handled Events

The component handles the following main events:

#### GAP Events
- `ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT`: Advertising data configured
- `ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT`: Scan response configured
- `ESP_GAP_BLE_ADV_START_COMPLETE_EVT`: Advertising started
- `ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT`: Advertising stopped
- `ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT`: Connection parameters updated

#### GATTS Events
- `ESP_GATTS_REG_EVT`: Application registered
- `ESP_GATTS_CREATE_EVT`: Service created
- `ESP_GATTS_ADD_CHAR_EVT`: Characteristic added
- `ESP_GATTS_CONNECT_EVT`: Device connected
- `ESP_GATTS_READ_EVT`: Characteristic read
- `ESP_GATTS_WRITE_EVT`: Characteristic write

## 👤 Author

**Pedro Luis Dionísio Fraga**
- Email: pedrodfraga@hotmail.com

## 📄 License

Copyright (c) 2025 - All rights reserved.

---

> **Note**: This component was developed for ESP32 using ESP-IDF. Make sure you have the development environment properly configured before compiling.

## 📝 References
- [ESP-IDF BLE GATT Server Documentation](https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/bluedroid/ble/gatt_server/tutorial/Gatt_Server_Example_Walkthrough.md)
