/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/array_view.h"
#include "api/video/encoded_frame.h"
#include "api/video/frame_buffer.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"
#include "test/fuzzers/fuzz_data_helper.h"
#include "test/scoped_key_value_config.h"

namespace webrtc {
namespace {
class FuzzyFrameObject : public EncodedFrame {
 public:
  int64_t ReceivedTime() const override { return 0; }
  int64_t RenderTime() const override { return 0; }
};

constexpr int kFrameIdLength = 1 << 15;

}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size > 10000) {
    return;
  }

  test::ScopedKeyValueConfig field_trials;
  FrameBuffer buffer(/*max_frame_slots=*/100, /*max_decode_history=*/1000,
                     field_trials);
  test::FuzzDataHelper helper(rtc::MakeArrayView(data, size));
  SeqNumUnwrapper<uint16_t, kFrameIdLength> unwrapper;

  while (helper.BytesLeft() > 0) {
    int action = helper.ReadOrDefaultValue<uint8_t>(0) % 6;

    switch (action) {
      case 0: {
        buffer.LastContinuousFrameId();
        break;
      }
      case 1: {
        buffer.LastContinuousTemporalUnitFrameId();
        break;
      }
      case 2: {
        buffer.DecodableTemporalUnitsInfo();
        break;
      }
      case 3: {
        buffer.ExtractNextDecodableTemporalUnit();
        break;
      }
      case 4: {
        buffer.DropNextDecodableTemporalUnit();
        break;
      }
      case 5: {
        auto frame = std::make_unique<FuzzyFrameObject>();
        frame->SetRtpTimestamp(helper.ReadOrDefaultValue<uint32_t>(0));
        int64_t wire_id =
            helper.ReadOrDefaultValue<uint16_t>(0) & (kFrameIdLength - 1);
        frame->SetId(unwrapper.Unwrap(wire_id));
        frame->is_last_spatial_layer = helper.ReadOrDefaultValue<bool>(false);

        frame->num_references = helper.ReadOrDefaultValue<uint8_t>(0) %
                                EncodedFrame::kMaxFrameReferences;

        for (uint8_t i = 0; i < frame->num_references; ++i) {
          frame->references[i] = helper.ReadOrDefaultValue<int64_t>(0);
        }

        buffer.InsertFrame(std::move(frame));
        break;
      }
    }
  }
}

}  // namespace webrtc
