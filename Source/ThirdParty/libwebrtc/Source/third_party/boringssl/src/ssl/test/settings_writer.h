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

#ifndef HEADER_SETTINGS_WRITER
#define HEADER_SETTINGS_WRITER

#include <string>

#include <openssl/bytestring.h>
#include <openssl/ssl.h>

#include "test_config.h"

// SettingsWriter persists data about a test handshake to files. If non-empty,
// `config->write_settings` controls the output path for fuzzing input data, and
// `config->write_hint_trace` controls the output path for handshake hint
// replay test traces.
struct SettingsWriter {
 public:
  SettingsWriter() = default;

  // Init initializes the writer for a new connection with index given by `i`.
  // Each connection gets a unique output file. Buffers pre-handshake data found
  // in `config` and `session` to be written later.
  bool Init(int i, const TestConfig *config, SSL_SESSION *session);

  // Commit writes the buffered data to disk.
  bool Commit();

  // Buffers handshake hint and client hello data to be written to handshake
  // hint replay test traces, and to fuzzing corpus. Should be called after the
  // ClientHello.
  bool WriteHintTrace(bssl::Span<const uint8_t> client_hello,
                      bssl::Span<const uint8_t> hints);

 private:
  // Path prefix and CBB for fuzzing corpus.
  std::string settings_path_;
  bssl::ScopedCBB settings_cbb_;
  // Path prefix and CBB for handshake hint replay test traces.
  std::string hint_trace_path_;
  bssl::ScopedCBB hint_trace_cbb_;
};

#endif  // HEADER_SETTINGS_WRITER
