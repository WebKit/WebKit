/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/single_process_encoded_image_data_injector.h"

#include <algorithm>
#include <cstddef>

#include "absl/memory/memory.h"
#include "api/video/encoded_image.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace webrtc_pc_e2e {

SingleProcessEncodedImageDataInjector::SingleProcessEncodedImageDataInjector() =
    default;
SingleProcessEncodedImageDataInjector::
    ~SingleProcessEncodedImageDataInjector() = default;

EncodedImage SingleProcessEncodedImageDataInjector::InjectData(
    uint16_t id,
    bool discard,
    const EncodedImage& source) {
  RTC_CHECK(source.size() >= ExtractionInfo::kUsedBufferSize);

  ExtractionInfo info;
  info.discard = discard;
  size_t insertion_pos = source.size() - ExtractionInfo::kUsedBufferSize;
  memcpy(info.origin_data, &source.data()[insertion_pos],
         ExtractionInfo::kUsedBufferSize);
  {
    MutexLock lock(&lock_);
    // Will create new one if missed.
    ExtractionInfoVector& ev = extraction_cache_[id];
    info.sub_id = ev.next_sub_id++;
    ev.infos[info.sub_id] = info;
  }

  auto buffer = EncodedImageBuffer::Create(source.data(), source.size());
  buffer->data()[insertion_pos] = id & 0x00ff;
  buffer->data()[insertion_pos + 1] = (id & 0xff00) >> 8;
  buffer->data()[insertion_pos + 2] = info.sub_id;

  EncodedImage out = source;
  out.SetEncodedData(buffer);
  return out;
}

void SingleProcessEncodedImageDataInjector::AddParticipantInCall() {
  MutexLock crit(&lock_);
  expected_receivers_count_++;
}

void SingleProcessEncodedImageDataInjector::RemoveParticipantInCall() {
  MutexLock crit(&lock_);
  expected_receivers_count_--;
  // Now we need go over `extraction_cache_` and removed frames which have been
  // received by `expected_receivers_count_`.
  for (auto& [frame_id, extraction_infos] : extraction_cache_) {
    for (auto it = extraction_infos.infos.begin();
         it != extraction_infos.infos.end();) {
      // Frame is received if `received_count` equals to
      // `expected_receivers_count_`.
      if (it->second.received_count == expected_receivers_count_) {
        it = extraction_infos.infos.erase(it);
      } else {
        ++it;
      }
    }
  }
}

EncodedImageExtractionResult SingleProcessEncodedImageDataInjector::ExtractData(
    const EncodedImage& source) {
  size_t size = source.size();
  auto buffer = EncodedImageBuffer::Create(source.data(), source.size());
  EncodedImage out = source;
  out.SetEncodedData(buffer);

  std::vector<size_t> frame_sizes;
  std::vector<size_t> frame_sl_index;
  size_t max_spatial_index = out.SpatialIndex().value_or(0);
  for (size_t i = 0; i <= max_spatial_index; ++i) {
    auto frame_size = source.SpatialLayerFrameSize(i);
    if (frame_size.value_or(0)) {
      frame_sl_index.push_back(i);
      frame_sizes.push_back(frame_size.value());
    }
  }
  if (frame_sizes.empty()) {
    frame_sizes.push_back(size);
  }

  size_t prev_frames_size = 0;
  std::optional<uint16_t> id = std::nullopt;
  bool discard = true;
  std::vector<ExtractionInfo> extraction_infos;
  for (size_t frame_size : frame_sizes) {
    size_t insertion_pos =
        prev_frames_size + frame_size - ExtractionInfo::kUsedBufferSize;
    // Extract frame id from first 2 bytes starting from insertion pos.
    uint16_t next_id = buffer->data()[insertion_pos] +
                       (buffer->data()[insertion_pos + 1] << 8);
    // Extract frame sub id from second 3 byte starting from insertion pos.
    uint8_t sub_id = buffer->data()[insertion_pos + 2];
    RTC_CHECK(!id || *id == next_id)
        << "Different frames encoded into single encoded image: " << *id
        << " vs " << next_id;
    id = next_id;
    ExtractionInfo info;
    {
      MutexLock lock(&lock_);
      auto ext_vector_it = extraction_cache_.find(next_id);
      RTC_CHECK(ext_vector_it != extraction_cache_.end())
          << "Unknown frame_id=" << next_id;

      auto info_it = ext_vector_it->second.infos.find(sub_id);
      RTC_CHECK(info_it != ext_vector_it->second.infos.end())
          << "Unknown sub_id=" << sub_id << " for frame_id=" << next_id;
      info_it->second.received_count++;
      info = info_it->second;
      if (info.received_count == expected_receivers_count_) {
        ext_vector_it->second.infos.erase(info_it);
      }
    }
    // We need to discard encoded image only if all concatenated encoded images
    // have to be discarded.
    discard = discard && info.discard;

    extraction_infos.push_back(info);
    prev_frames_size += frame_size;
  }
  RTC_CHECK(id);

  if (discard) {
    out.set_size(0);
    for (size_t i = 0; i <= max_spatial_index; ++i) {
      out.SetSpatialLayerFrameSize(i, 0);
    }
    return EncodedImageExtractionResult{*id, out, true};
  }

  // Make a pass from begin to end to restore origin payload and erase discarded
  // encoded images.
  size_t pos = 0;
  for (size_t frame_index = 0; frame_index < frame_sizes.size();
       ++frame_index) {
    RTC_CHECK(pos < size);
    const size_t frame_size = frame_sizes[frame_index];
    const ExtractionInfo& info = extraction_infos[frame_index];
    if (info.discard) {
      // If this encoded image is marked to be discarded - erase it's payload
      // from the buffer.
      memmove(&buffer->data()[pos], &buffer->data()[pos + frame_size],
              size - pos - frame_size);
      RTC_CHECK_LT(frame_index, frame_sl_index.size())
          << "codec doesn't support discard option or the image, that was "
             "supposed to be discarded, is lost";
      out.SetSpatialLayerFrameSize(frame_sl_index[frame_index], 0);
      size -= frame_size;
    } else {
      memcpy(
          &buffer->data()[pos + frame_size - ExtractionInfo::kUsedBufferSize],
          info.origin_data, ExtractionInfo::kUsedBufferSize);
      pos += frame_size;
    }
  }
  out.set_size(pos);

  return EncodedImageExtractionResult{*id, out, discard};
}

SingleProcessEncodedImageDataInjector::ExtractionInfoVector::
    ExtractionInfoVector() = default;
SingleProcessEncodedImageDataInjector::ExtractionInfoVector::
    ~ExtractionInfoVector() = default;

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
