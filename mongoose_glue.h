// SPDX-FileCopyrightText: 2024 Cesanta Software Limited
// SPDX-License-Identifier: GPL-2.0-only or commercial
// Generated by Mongoose Wizard, https://mongoose.ws/wizard/

#ifndef MONGOOSE_GLUE_H
#define MONGOOSE_GLUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mongoose.h"

////////////////////////////////////////////// HTTP
#define WIZARD_ENABLE_HTTP 1
#define WIZARD_ENABLE_HTTPS 0
#define WIZARD_ENABLE_HTTTP_UI 0
#define WIZARD_ENABLE_HTTP_UI_LOGIN 0

//////////////////////////////////////////////  MQTT
#define WIZARD_ENABLE_MQTT 0
#define WIZARD_MQTT_URL ""

//////////////////////////////////////////////  SNTP
#define WIZARD_ENABLE_SNTP 0  // Enable time sync.
#define WIZARD_SNTP_TYPE 0    // 0: default Google, 1: DHCP, 2: custom
#define WIZARD_SNTP_URL "udp://time.google.com:123"  // Custom SNTP server URL
#define WIZARD_SNTP_INTERVAL_SECONDS 3600            // Frequency of SNTP syncs

//////////////////////////////////////////////  DNS
#define WIZARD_DNS_TYPE 0     // 0: default Google, 1: DHCP, 2: custom
#define WIZARD_DNS_URL "udp://8.8.8.8:53"            // Custom DNS server URL

//////////////////////////////////////////////  MODBUS
#define WIZARD_ENABLE_MODBUS 0
#define WIZARD_MODBUS_PORT 502

//////////////////////////////////////////////  Initialize & run
void mongoose_init(void);    // Initialise Mongoose
void mongoose_poll(void);    // Poll Mongoose
extern struct mg_mgr g_mgr;  // Mongoose event manager
void glue_init(void);        // Called at the end of mongoose_init()

#define run_mongoose() \
  do {                 \
    mongoose_init();   \
    for (;;) {         \
      mongoose_poll(); \
    }                  \
  } while (0)

#if WIZARD_ENABLE_MQTT
void glue_lock_init(void);  // Initialise global Mongoose mutex
void glue_lock(void);       // Lock global Mongoose mutex
void glue_unlock(void);     // Unlock global Mongoose mutex
#else
#define glue_lock_init()
#define glue_lock()
#define glue_unlock()
#endif

// Print arbitrary string. Supported format is printf, plus %m/%M, see
// https://mongoose.ws/documentation/#mg_snprintf-mg_vsnprintf
size_t glue_printf(void *context, const char *format, ...);

// Firmware Glue

struct time {
  char time[20];
};
struct time *glue_get_time(void);
void glue_set_time(struct time *);


#ifdef __cplusplus
}
#endif
#endif  // MONGOOSE_GLUE_H
