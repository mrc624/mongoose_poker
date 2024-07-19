// SPDX-FileCopyrightText: 2024 Cesanta Software Limited
// SPDX-License-Identifier: GPL-2.0-only or commercial
// Generated by Mongoose Wizard, https://mongoose.ws/wizard/

#include "mongoose.h"
#include "mongoose_glue.h"

#if MG_ARCH == MG_ARCH_UNIX || MG_ARCH == MG_ARCH_WIN32
#define HTTP_URL "http://0.0.0.0:8080"
#else
#define HTTP_URL "http://0.0.0.0:80"
#endif

#define NO_CACHE_HEADERS "Cache-Control: no-cache\r\n"
#define JSON_HEADERS "Content-Type: application/json\r\n" NO_CACHE_HEADERS

struct mg_mgr g_mgr;  // Mongoose event manager

#if (WIZARD_ENABLE_HTTP || WIZARD_ENABLE_HTTPS) && WIZARD_ENABLE_HTTP_UI

// Every time device state changes, this counter increments.
// Used by the heartbeat endpoint, to signal the UI when to refresh
static unsigned long s_device_change_version = 0;

struct attribute {
  const char *name;
  const char *type;
  size_t offset;
  size_t size;
  bool readonly;
};

struct apihandler {
  const char *name;
  const char *type;
  bool readonly;
  int read_level;
  int write_level;
  unsigned long version;               // Every change increments version
  const struct attribute *attributes;  // Points to the strucure descriptor
  void *(*getter)(void);               // Getter/check/begin function
  void (*setter)(void *);              // Setter/start/end function
  void (*writer)(void);                // Write function (OTA and upload)
  size_t data_size;                    // Size of C structure
};

struct attribute s_time_attributes[] = {
  {"utc", "string", offsetof(struct time, utc), 32, false},
  {"local", "string", offsetof(struct time, local), 32, false},
  {"up", "string", offsetof(struct time, up), 32, false},
  {NULL, NULL, 0, 0, false}
};
static struct apihandler s_apihandlers[] = {
  {"time", "object", false, 0, 0, 0UL, s_time_attributes, (void *(*)(void)) glue_get_time, (void (*)(void *)) glue_set_time, NULL, sizeof(struct time)}
};

static struct apihandler *find_handler(struct mg_http_message *hm) {
  size_t i;
  if (hm->uri.len < 6 || strncmp(hm->uri.buf, "/api/", 5) != 0) return NULL;
  for (i = 0; i < sizeof(s_apihandlers) / sizeof(s_apihandlers[0]); i++) {
    struct apihandler *h = &s_apihandlers[i];
    size_t n = strlen(h->name);
    if (n + 5 > hm->uri.len) continue;
    if (strncmp(hm->uri.buf + 5, h->name, n) != 0) continue;
    MG_INFO(("%.*s %s %lu %lu", hm->uri.len, hm->uri.buf, h->name, n + 5,
             hm->uri.len));
    if (n + 5 < hm->uri.len && hm->uri.buf[n + 5] != '/') continue;
    return h;
  }
  return NULL;
}

void mg_json_get_str2(struct mg_str json, const char *path, char *buf,
                      size_t len) {
  struct mg_str s = mg_json_get_tok(json, path);
  if (s.len > 1 && s.buf[0] == '"') {
    mg_json_unescape(mg_str_n(s.buf + 1, s.len - 2), buf, len);
  }
}

#if WIZARD_ENABLE_HTTP_UI_LOGIN

struct user {
  struct user *next;
  char name[32];   // User name
  char token[21];  // Login token
  int level;       // Access level
};

static struct user *s_users;  // List of authenticated users

// Parse HTTP requests, return authenticated user or NULL
static struct user *authenticate(struct mg_http_message *hm) {
  char user[100], pass[100];
  struct user *u, *result = NULL;
  mg_http_creds(hm, user, sizeof(user), pass, sizeof(pass));

  if (user[0] != '\0' && pass[0] != '\0') {
    // Both user and password is set, auth by user/password via glue API
    int level = glue_authenticate(user, pass);
    MG_DEBUG(("user %s, level: %d", user, level));
    if (level > 0) {  // Proceed only if the firmware authenticated us
      // uint64_t uid = hash(3, mg_str(user), mg_str(":"), mg_str(pass));
      for (u = s_users; u != NULL && result == NULL; u = u->next) {
        if (strcmp(user, u->name) == 0) result = u;
      }
      // Not yet authenticated, add to the list
      if (result == NULL) {
        result = (struct user *) calloc(1, sizeof(*result));
        mg_snprintf(result->name, sizeof(result->name), "%s", user);
        mg_random_str(result->token, sizeof(result->token) - 1);
        result->level = level, result->next = s_users, s_users = result;
      }
    }
  } else if (user[0] == '\0' && pass[0] != '\0') {
    for (u = s_users; u != NULL && result == NULL; u = u->next) {
      if (strcmp(u->token, pass) == 0) result = u;
    }
  }
  MG_VERBOSE(("[%s/%s] -> %s", user, pass, result ? "OK" : "FAIL"));
  return result;
}

static void handle_login(struct mg_connection *c, struct user *u) {
  char cookie[256];
  mg_snprintf(cookie, sizeof(cookie),
              "Set-Cookie: access_token=%s; Path=/; "
              "%sHttpOnly; SameSite=Lax; Max-Age=%d\r\n",
              u->token, c->is_tls ? "Secure; " : "", 3600 * 24);
  mg_http_reply(c, 200, cookie, "{%m:%m,%m:%d}",  //
                MG_ESC("user"), MG_ESC(u->name),  //
                MG_ESC("level"), u->level);
}

static void handle_logout(struct mg_connection *c) {
  char cookie[256];
  mg_snprintf(cookie, sizeof(cookie),
              "Set-Cookie: access_token=; Path=/; "
              "Expires=Thu, 01 Jan 1970 00:00:00 UTC; "
              "%sHttpOnly; Max-Age=0; \r\n",
              c->is_tls ? "Secure; " : "");
  mg_http_reply(c, 401, cookie, "Unauthorized\n");
}
#endif  // WIZARD_ENABLE_HTTP_UI_LOGIN

struct upload_state {
  char marker;               // Tells that we're a file upload connection
  size_t expected;           // POST data length, bytes
  size_t received;           // Already received bytes
  void *fp;                  // Opened file
  bool (*fn_close)(void *);  // Close function
  bool (*fn_write)(void *, void *, size_t);  // Write function
};

struct action_state {
  char marker;       // Tells that we're an action connection
  bool (*fn)(void);  // Action status function
};

// static bool is_level_ok(struct mg_connection *c, struct mg_http_message *hm,
//                         int user_level, int read_level, int write_level) {
//   bool ok = true;
//   if (read_level > 0 && user_level < read_level) ok = false;
//   if (write_level > 0 && user_level < write_level && hm->body.len > 0)
//     ok = false;
//   if (ok == false) mg_http_reply(c, 403, "", "");
//   return ok;
// }

static void close_uploaded_file(struct upload_state *us) {
  us->marker = 0;
  if (us->fn_close != NULL && us->fp != NULL) {
    us->fn_close(us->fp);
    us->fp = NULL;
  }
  memset(us, 0, sizeof(*us));
}

static void upload_handler(struct mg_connection *c, int ev, void *ev_data) {
  struct upload_state *us = (struct upload_state *) c->data;
  if (sizeof(*us) > sizeof(c->data)) {
    mg_error(
        c, "FAILURE: sizeof(c->data) == %lu, need %lu. Set -DMG_DATA_SIZE=XXX",
        sizeof(c->data), sizeof(*us));
    return;
  }
  // Catch uploaded file data for both MG_EV_READ and MG_EV_HTTP_HDRS
  if (us->marker == 'U' && ev == MG_EV_READ && us->expected > 0 &&
      c->recv.len > 0) {
    size_t alignment = 32;  // Maximum flash write granularity (STM32H7)
    size_t aligned = (us->received + c->recv.len < us->expected)
                         ? aligned = MG_ROUND_DOWN(c->recv.len, alignment)
                         : c->recv.len;  // Last write can be unaligned
    bool ok = us->fn_write(us->fp, c->recv.buf, aligned);
    us->received += aligned;
    MG_DEBUG(("%lu chunk: %lu/%lu, %lu/%lu, ok: %d", c->id, aligned,
              c->recv.len, us->received, us->expected, ok));
    mg_iobuf_del(&c->recv, 0, aligned);  // Delete received data
    if (ok == false) {
      mg_http_reply(c, 400, "", "Upload error\n");
      close_uploaded_file(us);
      c->is_draining = 1;  // Close connection when response it sent
    } else if (us->received >= us->expected) {
      // Uploaded everything. Send response back
      MG_INFO(("%lu done, %lu bytes", c->id, us->received));
      mg_http_reply(c, 200, NULL, "%lu ok\n", us->received);
      close_uploaded_file(us);
      c->is_draining = 1;  // Close connection when response it sent
    }
  }

  // Close uploading file descriptor
  if (us->marker == 'U' && ev == MG_EV_CLOSE) close_uploaded_file(us);
  (void) ev_data;
}

static void prep_upload(struct mg_connection *c, struct mg_http_message *hm,
                        void *(*fn_open)(char *, size_t),
                        bool (*fn_close)(void *),
                        bool (*fn_write)(void *, void *, size_t)) {
  struct upload_state *us = (struct upload_state *) c->data;
  struct mg_str parts[3];
  char path[MG_PATH_MAX];
  memset(us, 0, sizeof(*us));                    // Cleanup upload state
  memset(parts, 0, sizeof(parts));               // Init match parts
  mg_match(hm->uri, mg_str("/api/*/#"), parts);  // Fetch file name
  mg_url_decode(parts[1].buf, parts[1].len, path, sizeof(path), 0);
  us->fp = fn_open(path, hm->body.len);
  MG_DEBUG(("file: [%s] size: %lu fp: %p", path, hm->body.len, us->fp));
  us->marker = 'U';  // Mark us as an upload connection
  if (us->fp == NULL) {
    mg_http_reply(c, 400, JSON_HEADERS, "File open error\n");
    c->is_draining = 1;
  } else {
    us->expected = hm->body.len;              // Store number of bytes we expect
    us->fn_close = fn_close;                  // Store closing function
    us->fn_write = fn_write;                  // Store writing function
    mg_iobuf_del(&c->recv, 0, hm->head.len);  // Delete HTTP headers
    c->pfn = upload_handler;
  }
}

static void handle_uploads(struct mg_connection *c, int ev, void *ev_data) {
  struct upload_state *us = (struct upload_state *) c->data;

  // Catch /upload requests early, without buffering whole body
  // When we receive MG_EV_HTTP_HDRS event, that means we've received all
  // HTTP headers but not necessarily full HTTP body
  if (ev == MG_EV_HTTP_HDRS && us->marker == 0) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    struct apihandler *h = find_handler(hm);
#if WIZARD_ENABLE_HTTP_UI_LOGIN
    struct user *u = authenticate(hm);
    if (mg_match(hm->uri, mg_str("/api/#"), NULL) &&
        (u == NULL ||
         (h != NULL && (u->level < h->read_level ||
                        (hm->body.len > 0 && u->level < h->write_level))))) {
      // MG_INFO(("DENY: %d, %d %d", u->level, h->read_level, h->write_level));
      mg_http_reply(c, 403, JSON_HEADERS, "Not Authorised\n");
    } else
#endif
        if (h != NULL &&
            (strcmp(h->type, "upload") == 0 || strcmp(h->type, "ota") == 0)) {
      // OTA/upload endpoints
      prep_upload(c, hm, (void *(*) (char *, size_t)) h->getter,
                  (bool (*)(void *)) h->setter,
                  (bool (*)(void *, void *, size_t)) h->writer);
    }
  }
}

static void handle_action(struct mg_connection *c, struct mg_http_message *hm,
                          bool (*check_fn)(void), void (*start_fn)(void)) {
  if (hm->body.len > 0) {
    start_fn();
    if (check_fn()) {
      struct action_state *as = (struct action_state *) c->data;
      as->marker = 'A';
      as->fn = check_fn;
    } else {
      mg_http_reply(c, 200, JSON_HEADERS, "false");
    }
  } else {
    mg_http_reply(c, 200, JSON_HEADERS, "%s", check_fn() ? "true" : "false");
  }
}

size_t print_struct(void (*out)(char, void *), void *ptr, va_list *ap) {
  struct apihandler *h = va_arg(*ap, struct apihandler *);
  char *data = va_arg(*ap, char *);
  size_t i, len = 0;
  for (i = 0; h->attributes[i].name != NULL; i++) {
    char *attrptr = data + h->attributes[i].offset;
    len += mg_xprintf(out, ptr, "%s%m:", i == 0 ? "" : ",",
                      MG_ESC(h->attributes[i].name));
    if (strcmp(h->attributes[i].type, "int") == 0) {
      len += mg_xprintf(out, ptr, "%d", *(int *) attrptr);
    } else if (strcmp(h->attributes[i].type, "double") == 0) {
      len += mg_xprintf(out, ptr, "%g", *(double *) attrptr);
    } else if (strcmp(h->attributes[i].type, "bool") == 0) {
      len += mg_xprintf(out, ptr, "%s", *(bool *) attrptr ? "true" : "false");
    } else if (strcmp(h->attributes[i].type, "string") == 0) {
      len += mg_xprintf(out, ptr, "%m", MG_ESC(attrptr));
    } else {
      len += mg_xprintf(out, ptr, "null");
    }
  }
  return len;
}

static void handle_api_call(struct mg_connection *c, struct mg_http_message *hm,
                            struct apihandler *h) {
  if (strcmp(h->type, "object") == 0) {
    void *data = h->getter();
    if (hm->body.len > 0 && h->data_size > 0) {
      char *tmp = calloc(1, h->data_size);
      size_t i;
      memcpy(tmp, data, h->data_size);
      for (i = 0; h->attributes[i].name != NULL; i++) {
        const struct attribute *a = &h->attributes[i];
        char jpath[100];
        mg_snprintf(jpath, sizeof(jpath), "$.%s", a->name);
        if (strcmp(a->type, "int") == 0) {
          double d;
          if (mg_json_get_num(hm->body, jpath, &d)) {
            int v = (int) d;
            memcpy(tmp + a->offset, &v, sizeof(v));
          }
        } else if (strcmp(a->type, "bool") == 0) {
          mg_json_get_bool(hm->body, jpath, (bool *) (tmp + a->offset));
        } else if (strcmp(a->type, "double") == 0) {
          mg_json_get_num(hm->body, jpath, (double *) (tmp + a->offset));
        } else if (strcmp(a->type, "string") == 0) {
          mg_json_get_str2(hm->body, jpath, tmp + a->offset, a->size);
        }
      }
      // If structure changes, increment version
      if (memcmp(data, tmp, h->data_size) != 0) s_device_change_version++;
      h->setter(tmp);
      free(tmp);
    }
    mg_http_reply(c, 200, JSON_HEADERS, "{%M}\n", print_struct, h, data);
  } else if (strcmp(h->type, "action") == 0) {
    handle_action(c, hm, (bool (*)(void)) h->getter,
                  (void (*)(void)) h->setter);
  } else {
    mg_http_reply(c, 500, JSON_HEADERS, "API type %s unknown\n", h->type);
  }
}

void glue_update_state(void) {
  s_device_change_version++;
}

// Mongoose event handler function, gets called by the mg_mgr_poll()
static void http_ev_handler(struct mg_connection *c, int ev, void *ev_data) {
  handle_uploads(c, ev, ev_data);
  (void) handle_action;  // Squash warnings

  if (ev == MG_EV_POLL && c->data[0] == 'A') {
    // Check if action in progress is complete
    struct action_state *as = (struct action_state *) c->data;
    if (as->fn() == false) {
      mg_http_reply(c, 200, JSON_HEADERS, "true");
      memset(as, 0, sizeof(*as));
    }
  } else if (ev == MG_EV_HTTP_MSG && c->data[0] != 'U') {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    struct apihandler *h = find_handler(hm);

#if WIZARD_ENABLE_HTTP_UI_LOGIN
    struct user *u = authenticate(hm);
    if (mg_match(hm->uri, mg_str("/api/#"), NULL) &&
        (u == NULL ||
         (h != NULL && (u->level < h->read_level ||
                        (hm->body.len > 0 && u->level < h->write_level))))) {
      // MG_INFO(("DENY: %d, %d %d", u->level, h->read_level, h->write_level));
      mg_http_reply(c, 403, JSON_HEADERS, "Not Authorised\n");
    } else if (mg_match(hm->uri, mg_str("/api/login"), NULL)) {
      handle_login(c, u);
    } else if (mg_match(hm->uri, mg_str("/api/logout"), NULL)) {
      handle_logout(c);
    } else
#endif
        if (mg_match(hm->uri, mg_str("/api/ok"), NULL)) {
      mg_http_reply(c, 200, JSON_HEADERS, "true\n");
    } else if (mg_match(hm->uri, mg_str("/api/heartbeat"), NULL)) {
      mg_http_reply(c, 200, JSON_HEADERS, "{%m:%lu}\n", MG_ESC("version"),
                    s_device_change_version);
    } else if (h != NULL) {
      handle_api_call(c, hm, h);
    } else {
      struct mg_http_serve_opts opts;
      memset(&opts, 0, sizeof(opts));
      opts.root_dir = "/web_root/";
      opts.fs = &mg_fs_packed;
      opts.extra_headers = NO_CACHE_HEADERS;
      mg_http_serve_dir(c, hm, &opts);
    }
    // Show this request
    MG_DEBUG(("%lu %.*s %.*s %lu -> %.*s", c->id, hm->method.len,
              hm->method.buf, hm->uri.len, hm->uri.buf, hm->body.len,
              c->send.len > 15 ? 3 : 0, &c->send.buf[9]));
  } else if (ev == MG_EV_ACCEPT) {
    if (c->fn_data != NULL) {  // TLS listener
      struct mg_tls_opts opts;
      memset(&opts, 0, sizeof(opts));
      opts.cert = mg_unpacked("/certs/server_cert.pem");
      opts.key = mg_unpacked("/certs/server_key.pem");
      mg_tls_init(c, &opts);
    }
  }
}
#endif  // WIZARD_ENABLE_HTTP

#if WIZARD_ENABLE_SNTP
static uint64_t s_sntp_timer = 0;
bool s_sntp_response_received = false;
static void sntp_ev_handler(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_SNTP_TIME) {
    uint64_t t = *(uint64_t *) ev_data;
    glue_sntp_on_time(t);
    s_sntp_response_received = true;
    s_sntp_timer += (WIZARD_SNTP_INTERVAL_SECONDS) *1000;
  }
  (void) c;
}

static void sntp_timer(void *param) {
  // uint64_t t1 = mg_now(), t2 = mg_millis();
  uint64_t timeout = (WIZARD_SNTP_INTERVAL_SECONDS) *1000;
  if (s_sntp_response_received == false) timeout = 1000;
  // This function is called every second. Once we received a response,
  // trigger SNTP sync less frequently, as set by the define
  if (mg_timer_expired(&s_sntp_timer, timeout, mg_millis())) {
    mg_sntp_connect(param, WIZARD_SNTP_URL, sntp_ev_handler, NULL);
  }
}
#endif  // WIZARD_ENABLE_SNTP

#if WIZARD_ENABLE_MQTT
struct mg_connection *g_mqtt_conn;  // MQTT client connection

static void mqtt_event_handler(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_CONNECT) {
    glue_mqtt_tls_init(c);
  } else if (ev == MG_EV_MQTT_OPEN) {
    glue_mqtt_on_connect(c, *(int *) ev_data);
  } else if (ev == MG_EV_MQTT_CMD) {
    glue_mqtt_on_cmd(c, ev_data);
  } else if (ev == MG_EV_MQTT_MSG) {
    struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
    glue_mqtt_on_message(c, mm->topic, mm->data);
  } else if (ev == MG_EV_CLOSE) {
    MG_DEBUG(("%lu Closing", c->id));
    g_mqtt_conn = NULL;
  }
}

static void mqtt_timer(void *arg) {
  struct mg_mgr *mgr = (struct mg_mgr *) arg;
  if (g_mqtt_conn == NULL) {
    g_mqtt_conn = glue_mqtt_connect(mgr, mqtt_event_handler);
  }
}
#endif  // WIZARD_ENABLE_MQTT

#if WIZARD_ENABLE_MODBUS
static void handle_modbus_pdu(struct mg_connection *c, uint8_t *buf,
                              size_t len) {
  MG_DEBUG(("Received PDU %p len %lu, hexdump:", buf, len));
  mg_hexdump(buf, len);
  // size_t hdr_size = 8, max_data_size = sizeof(response) - hdr_size;
  if (len < 12) {
    MG_ERROR(("PDU too small"));
  } else {
    uint8_t func = buf[7];  // Function
    bool success = false;
    size_t response_len = 0;
    uint8_t response[260];
    memcpy(response, buf, 8);
    // uint16_t tid = mg_ntohs(*(uint16_t *) &buf[0]);  // Transaction ID
    // uint16_t pid = mg_ntohs(*(uint16_t *) &buf[0]);  // Protocol ID
    // uint16_t len = mg_ntohs(*(uint16_t *) &buf[4]);  // PDU length
    // uint8_t uid = buf[6];                            // Unit identifier
    if (func == 6) {  // write single holding register
      uint16_t start = mg_ntohs(*(uint16_t *) &buf[8]);
      uint16_t value = mg_ntohs(*(uint16_t *) &buf[10]);
      success = glue_modbus_write_reg(start, value);
      *(uint16_t *) &response[8] = mg_htons(start);
      *(uint16_t *) &response[10] = mg_htons(value);
      response_len = 12;
      MG_DEBUG(("Glue returned %s", success ? "success" : "failure"));
    } else if (func == 16) {  // Write multiple
      uint16_t start = mg_ntohs(*(uint16_t *) &buf[8]);
      uint16_t num = mg_ntohs(*(uint16_t *) &buf[10]);
      uint16_t i, *data = (uint16_t *) &buf[13];
      if ((size_t) (num * 2 + 10) < sizeof(response)) {
        for (i = 0; i < num; i++) {
          success =
              glue_modbus_write_reg((uint16_t) (start + i), mg_htons(data[i]));
          if (success == false) break;
        }
        *(uint16_t *) &response[8] = mg_htons(start);
        *(uint16_t *) &response[10] = mg_htons(num);
        response_len = 12;
        MG_DEBUG(("Glue returned %s", success ? "success" : "failure"));
      }
    } else if (func == 3 || func == 4) {  // Read multiple
      uint16_t start = mg_ntohs(*(uint16_t *) &buf[8]);
      uint16_t num = mg_ntohs(*(uint16_t *) &buf[10]);
      if ((size_t) (num * 2 + 9) < sizeof(response)) {
        uint16_t i, val, *data = (uint16_t *) &response[9];
        for (i = 0; i < num; i++) {
          success = glue_modbus_read_reg((uint16_t) (start + i), &val);
          if (success == false) break;
          data[i] = mg_htons(val);
        }
        response[8] = (uint8_t) (num * 2);
        response_len = 9 + response[8];
        MG_DEBUG(("Glue returned %s", success ? "success" : "failure"));
      }
    }
    if (success == false) {
      response_len = 9;
      response[7] |= 0x80;
      response[8] = 4;  // Server Device Failure
    }
    *(uint16_t *) &response[4] = mg_htons((uint16_t) (response_len - 6));
    MG_DEBUG(("Sending PDU response %lu:", response_len));
    mg_hexdump(response, response_len);
    mg_send(c, response, response_len);
  }
}

static void modbus_ev_handler(struct mg_connection *c, int ev, void *ev_data) {
  // if (ev == MG_EV_OPEN) c->is_hexdumping = 1;
  if (ev == MG_EV_READ) {
    uint16_t len;
    if (c->recv.len < 7) return;  // Less than minimum length, buffer more
    len = mg_ntohs(*(uint16_t *) &c->recv.buf[4]);  // PDU length
    MG_INFO(("Got %lu, expecting %lu", c->recv.len, len + 6));
    if (c->recv.len < len + 6U) return;          // Partial frame, buffer more
    handle_modbus_pdu(c, c->recv.buf, len + 6);  // Parse PDU and call user
    mg_iobuf_del(&c->recv, 0, len + 6U);         // Delete received PDU
  }
  (void) ev_data;
}
#endif  // WIZARD_ENABLE_MODBUS

void mongoose_init(void) {
  mg_mgr_init(&g_mgr);      // Initialise event manager
  mg_log_set(MG_LL_DEBUG);  // Set log level to debug

#if (WIZARD_ENABLE_HTTP || WIZARD_ENABLE_HTTPS) && WIZARD_ENABLE_HTTP_UI
  MG_INFO(("Starting HTTP listener"));
  mg_http_listen(&g_mgr, HTTP_URL, http_ev_handler, NULL);
#endif

#if WIZARD_ENABLE_SNTP
  MG_INFO(("Starting SNTP timer"));
  mg_timer_add(&g_mgr, 1000, MG_TIMER_REPEAT, sntp_timer, &g_mgr);
#endif

#if WIZARD_DNS_TYPE == 2
  g_mgr.dns4.url = WIZARD_DNS_URL;
#endif

#if WIZARD_ENABLE_MQTT
  MG_INFO(("Starting MQTT reconnection timer"));
  mg_timer_add(&g_mgr, 1000, MG_TIMER_REPEAT, mqtt_timer, &g_mgr);
#endif

#if WIZARD_ENABLE_MODBUS
  {
    char url[100];
    mg_snprintf(url, sizeof(url), "tcp://0.0.0.0:%d", WIZARD_MODBUS_PORT);
    MG_INFO(("Starting Modbus-TCP server on port %d", WIZARD_MODBUS_PORT));
    mg_listen(&g_mgr, url, modbus_ev_handler, NULL);
  }
#endif

  MG_INFO(("Mongoose init complete, calling user init"));
  glue_init();
}

void mongoose_poll(void) {
  glue_lock();
  mg_mgr_poll(&g_mgr, 50);
  glue_unlock();
}
