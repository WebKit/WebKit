/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_TEST_FAKE_CODEC_LOOKUP_HELPER_H_
#define PC_TEST_FAKE_CODEC_LOOKUP_HELPER_H_

#include <memory>

#include "call/payload_type.h"
#include "pc/codec_vendor.h"
#include "pc/connection_context.h"
#include "rtc_base/checks.h"

namespace webrtc {

class FakeCodecLookupHelper : public CodecLookupHelper {
 public:
  explicit FakeCodecLookupHelper(ConnectionContext* context,
                                 const FieldTrialsView& field_trials)
      : context_(context),
        field_trials_(&field_trials),
        codec_vendor_(
            std::make_unique<::webrtc::CodecVendor>(context->media_engine(),
                                                    context->use_rtx(),
                                                    *field_trials_)) {}
  webrtc::PayloadTypeSuggester* PayloadTypeSuggester() override {
    // Not used in this test.
    RTC_CHECK_NOTREACHED();
    return nullptr;
  }

  CodecVendor* GetCodecVendor() override { return codec_vendor_.get(); }
  // Recreate the codec vendor.
  // Used by tests that manipulate the factory's codecs and expect the
  // result to show up in the codec vendor's output.
  void Reset() {
    codec_vendor_ = std::make_unique<::webrtc::CodecVendor>(
        context_->media_engine(), context_->use_rtx(), *field_trials_);
  }

 private:
  ConnectionContext* const context_;
  const FieldTrialsView* absl_nonnull field_trials_;
  std::unique_ptr<::webrtc::CodecVendor> codec_vendor_;
};

}  // namespace webrtc

#endif  // PC_TEST_FAKE_CODEC_LOOKUP_HELPER_H_
