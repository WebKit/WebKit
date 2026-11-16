// Copyright 2018 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "settings_writer.h"

#include <stdio.h>

#include <optional>

#include <openssl/ssl.h>

#include "fuzzer_tags.h"
#include "test_config.h"


namespace {

bool WriteData(CBB *cbb, bssl::Span<const uint8_t> data,
               std::optional<uint16_t> tag = std::nullopt) {
  CBB child;
  return (!tag.has_value() || CBB_add_u16(cbb, *tag)) &&
         CBB_add_u24_length_prefixed(cbb, &child) &&
         CBB_add_bytes(&child, data.data(), data.size()) &&  //
         CBB_flush(cbb);
}

}  // namespace

bool SettingsWriter::Init(int i, const TestConfig *config,
                          SSL_SESSION *session) {
  // Treat provided flags as a path prefix for each connection in the run, and
  // append the connection index to write each connection's data to a separate
  // file.
  char conn_index[DECIMAL_SIZE(int)];
  snprintf(conn_index, sizeof(conn_index), "%d", i);

  if (!config->write_settings.empty()) {
    settings_path_ = config->write_settings + conn_index;
    if (!CBB_init(settings_cbb_.get(), 64)) {
      return false;
    }

    if (session != nullptr) {
      uint8_t *data;
      size_t len;
      if (!SSL_SESSION_to_bytes(session, &data, &len)) {
        return false;
      }
      bssl::UniquePtr<uint8_t> free_data(data);
      if (!WriteData(settings_cbb_.get(), bssl::Span(data, len), kSessionTag)) {
        return false;
      }
    }
    if (config->is_server &&
        (config->require_any_client_certificate || config->verify_peer) &&
        !CBB_add_u16(settings_cbb_.get(), kRequestClientCert)) {
      return false;
    }
  }

  if (!config->write_hint_trace.empty()) {
    hint_trace_path_ = config->write_hint_trace + conn_index;
    if (!CBB_init(hint_trace_cbb_.get(), 64)) {
      return false;
    }
  }
  return true;
}

bool SettingsWriter::Commit() {
  struct FileCloser {
    void operator()(FILE *f) const { fclose(f); }
  };
  using ScopedFILE = std::unique_ptr<FILE, FileCloser>;
  if (!settings_path_.empty()) {
    uint8_t *settings;
    size_t settings_len;
    if (!CBB_add_u16(settings_cbb_.get(), kDataTag) ||
        !CBB_finish(settings_cbb_.get(), &settings, &settings_len)) {
      return false;
    }
    bssl::UniquePtr<uint8_t> free_settings(settings);
    ScopedFILE file(fopen(settings_path_.c_str(), "w"));
    if (!file || fwrite(settings, settings_len, 1, file.get()) != 1) {
      return false;
    }
  }

  if (!hint_trace_path_.empty()) {
    uint8_t *hints_trace;
    size_t hints_trace_len;
    if (!CBB_finish(hint_trace_cbb_.get(), &hints_trace, &hints_trace_len)) {
      return false;
    }
    bssl::UniquePtr<uint8_t> free_hints_trace(hints_trace);
    // Skip if there were no hints.
    if (hints_trace_len > 0) {
      ScopedFILE file(fopen(hint_trace_path_.c_str(), "w"));
      if (!file || fwrite(hints_trace, hints_trace_len, 1, file.get()) != 1) {
        return false;
      }
    }
  }

  return true;
}

bool SettingsWriter::WriteHintTrace(bssl::Span<const uint8_t> client_hello,
                                    bssl::Span<const uint8_t> hints) {
  return (settings_path_.empty() ||
          WriteData(settings_cbb_.get(), hints, kHintsTag)) &&
         (hint_trace_path_.empty() ||
          (WriteData(hint_trace_cbb_.get(), client_hello) &&
           WriteData(hint_trace_cbb_.get(), hints)));
}
