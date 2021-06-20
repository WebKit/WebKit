/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/encoder/blob_encoding.h"

#include <cstdint>

#include "logging/rtc_event_log/encoder/var_int.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

std::string EncodeBlobs(const std::vector<std::string>& blobs) {
  RTC_DCHECK(!blobs.empty());

  size_t result_length_bound = kMaxVarIntLengthBytes * blobs.size();
  for (const auto& blob : blobs) {
    // Providing an input so long that it would cause a wrap-around is an error.
    RTC_DCHECK_GE(result_length_bound + blob.length(), result_length_bound);
    result_length_bound += blob.length();
  }

  std::string result;
  result.reserve(result_length_bound);

  // First, encode all of the lengths.
  for (absl::string_view blob : blobs) {
    result += EncodeVarInt(blob.length());
  }

  // Second, encode the actual blobs.
  for (absl::string_view blob : blobs) {
    result.append(blob.data(), blob.length());
  }

  RTC_DCHECK_LE(result.size(), result_length_bound);
  return result;
}

std::vector<absl::string_view> DecodeBlobs(absl::string_view encoded_blobs,
                                           size_t num_of_blobs) {
  if (encoded_blobs.empty()) {
    RTC_LOG(LS_WARNING) << "Corrupt input; empty input.";
    return std::vector<absl::string_view>();
  }

  if (num_of_blobs == 0u) {
    RTC_LOG(LS_WARNING)
        << "Corrupt input; number of blobs must be greater than 0.";
    return std::vector<absl::string_view>();
  }

  // Read the lengths of all blobs.
  std::vector<uint64_t> lengths(num_of_blobs);
  for (size_t i = 0; i < num_of_blobs; ++i) {
    bool success = false;
    std::tie(success, encoded_blobs) = DecodeVarInt(encoded_blobs, &lengths[i]);
    if (!success) {
      RTC_LOG(LS_WARNING) << "Corrupt input; varint decoding failed.";
      return std::vector<absl::string_view>();
    }
  }

  // Read the blobs themselves.
  std::vector<absl::string_view> blobs(num_of_blobs);
  for (size_t i = 0; i < num_of_blobs; ++i) {
    if (lengths[i] > encoded_blobs.length()) {
      RTC_LOG(LS_WARNING) << "Corrupt input; blob sizes exceed input size.";
      return std::vector<absl::string_view>();
    }

    blobs[i] = encoded_blobs.substr(0, lengths[i]);
    encoded_blobs = encoded_blobs.substr(lengths[i]);
  }

  if (!encoded_blobs.empty()) {
    RTC_LOG(LS_WARNING) << "Corrupt input; unrecognized trailer.";
    return std::vector<absl::string_view>();
  }

  return blobs;
}

}  // namespace webrtc
