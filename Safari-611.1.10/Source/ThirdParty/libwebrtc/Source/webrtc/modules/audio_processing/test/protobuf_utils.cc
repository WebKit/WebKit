/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/test/protobuf_utils.h"

#include <memory>

#include "rtc_base/system/arch.h"

namespace {
// Allocates new memory in the memory owned by the unique_ptr to fit the raw
// message and returns the number of bytes read when having a string stream as
// input.
size_t ReadMessageBytesFromString(std::stringstream* input,
                                  std::unique_ptr<uint8_t[]>* bytes) {
  int32_t size = 0;
  input->read(reinterpret_cast<char*>(&size), sizeof(int32_t));
  int32_t size_read = input->gcount();
  if (size_read != sizeof(int32_t))
    return 0;
  if (size <= 0)
    return 0;

  *bytes = std::make_unique<uint8_t[]>(size);
  input->read(reinterpret_cast<char*>(bytes->get()),
              size * sizeof((*bytes)[0]));
  size_read = input->gcount();
  return size_read == size ? size : 0;
}
}  // namespace

namespace webrtc {

size_t ReadMessageBytesFromFile(FILE* file, std::unique_ptr<uint8_t[]>* bytes) {
// The "wire format" for the size is little-endian. Assume we're running on
// a little-endian machine.
#ifndef WEBRTC_ARCH_LITTLE_ENDIAN
#error "Need to convert messsage from little-endian."
#endif
  int32_t size = 0;
  if (fread(&size, sizeof(size), 1, file) != 1)
    return 0;
  if (size <= 0)
    return 0;

  *bytes = std::make_unique<uint8_t[]>(size);
  return fread(bytes->get(), sizeof((*bytes)[0]), size, file);
}

// Returns true on success, false on error or end-of-file.
bool ReadMessageFromFile(FILE* file, MessageLite* msg) {
  std::unique_ptr<uint8_t[]> bytes;
  size_t size = ReadMessageBytesFromFile(file, &bytes);
  if (!size)
    return false;

  msg->Clear();
  return msg->ParseFromArray(bytes.get(), size);
}

// Returns true on success, false on error or end of string stream.
bool ReadMessageFromString(std::stringstream* input, MessageLite* msg) {
  std::unique_ptr<uint8_t[]> bytes;
  size_t size = ReadMessageBytesFromString(input, &bytes);
  if (!size)
    return false;

  msg->Clear();
  return msg->ParseFromArray(bytes.get(), size);
}

}  // namespace webrtc
