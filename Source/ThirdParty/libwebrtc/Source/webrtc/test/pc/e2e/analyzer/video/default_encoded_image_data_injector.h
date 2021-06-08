/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_ENCODED_IMAGE_DATA_INJECTOR_H_
#define TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_ENCODED_IMAGE_DATA_INJECTOR_H_

#include <cstdint>
#include <deque>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "api/video/encoded_image.h"
#include "test/pc/e2e/analyzer/video/encoded_image_data_injector.h"

namespace webrtc {
namespace webrtc_pc_e2e {

// Injects frame id and discard flag into EncodedImage payload buffer. The
// payload buffer will be appended in the injector with 2 bytes frame id and 4
// bytes original buffer length. Discarded flag will be put into the highest bit
// of the length. It is assumed, that frame's data can't be more then 2^31
// bytes. In the decoder, frame id and discard flag will be extracted and the
// length will be used to restore original buffer. We can't put this data in the
// beginning of the payload, because first bytes are used in different parts of
// WebRTC pipeline.
//
// The data in the EncodedImage on encoder side after injection will look like
// this:
//                         4 bytes frame length + discard flag
//  _________________ _ _ _↓_ _ _
// | original buffer |   |       |
//  ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯ ¯↑¯ ¯ ¯ ¯ ¯
//                     2 bytes frame id
//
// But on decoder side multiple payloads can be concatenated into single
// EncodedImage in jitter buffer and its payload will look like this:
//        _________ _ _ _ _ _ _ _________ _ _ _ _ _ _ _________ _ _ _ _ _ _
//  buf: | payload |   |       | payload |   |       | payload |   |       |
//        ¯¯¯¯¯¯¯¯¯ ¯ ¯ ¯ ¯ ¯ ¯ ¯¯¯¯¯¯¯¯¯ ¯ ¯ ¯ ¯ ¯ ¯ ¯¯¯¯¯¯¯¯¯ ¯ ¯ ¯ ¯ ¯ ¯
// To correctly restore such images we will extract id by this algorithm:
//   1. Make a pass from end to begin of the buffer to restore origin lengths,
//      frame ids and discard flags from length high bit.
//   2. If all discard flags are true - discard this encoded image
//   3. Make a pass from begin to end copying data to the output basing on
//      previously extracted length
// Also it will check, that all extracted ids are equals.
class DefaultEncodedImageDataInjector : public EncodedImageDataInjector,
                                        public EncodedImageDataExtractor {
 public:
  DefaultEncodedImageDataInjector();
  ~DefaultEncodedImageDataInjector() override;

  EncodedImage InjectData(uint16_t id,
                          bool discard,
                          const EncodedImage& source,
                          int /*coding_entity_id*/) override;

  void Start(int expected_receivers_count) override {}
  EncodedImageExtractionResult ExtractData(const EncodedImage& source,
                                           int coding_entity_id) override;
};

}  // namespace webrtc_pc_e2e
}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_ENCODED_IMAGE_DATA_INJECTOR_H_
