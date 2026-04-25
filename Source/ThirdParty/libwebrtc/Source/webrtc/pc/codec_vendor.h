/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_CODEC_VENDOR_H_
#define PC_CODEC_VENDOR_H_

#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "api/field_trials_view.h"
#include "api/rtc_error.h"
#include "api/rtp_transceiver_direction.h"
#include "api/sequence_checker.h"
#include "call/payload_type.h"
#include "media/base/codec.h"
#include "media/base/codec_list.h"
#include "media/base/media_engine.h"
#include "pc/media_options.h"
#include "pc/session_description.h"
#include "pc/typed_codec_vendor.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

// This class contains the functions required to compute the list of codecs
// for SDP offer/answer. It is exposed to MediaSessionDescriptionFactory
// for the construction of offers and answers.

// TODO: bugs.webrtc.org/360058654 - complete the architectural changes
// The list of things to be done:
// - Make as much as possible private.
// - Make state const where possible while updates related to threading are
// being done.
// - Remove test code from the implementation.
// - Split object usage into four objects: sender/receiver/audio/video.
// - Remove audio/video from the call names, merge code where possible.
// - Make the class instances owned by transceivers, so that codec
//   lists can differ per transceiver.
// For cleanliness:
// - Thread guard
// For performance:
// - Ensure that no blocking calls are made.
class CodecVendor {
 public:
  // A null media_engine is permitted in order to allow unit testing where the
  // codecs are explicitly set by the test.
  // TODO: bugs.webrtc.org/360058654 - The tests can accomplish what they need
  // by using the same interface as is used in production.
  // Update the tests instead to supply a valid MediaEngineInterface object
  // and rather test how CodecVendor works regularly.
  CodecVendor(const MediaEngineInterface* absl_nullable media_engine,
              bool rtx_enabled,
              const FieldTrialsView& trials);

  RTCErrorOr<std::vector<Codec>> GetNegotiatedCodecsForOffer(
      const MediaDescriptionOptions& media_description_options,
      const MediaSessionOptions& session_options,
      const ContentInfo* current_content,
      PayloadTypeSuggester& pt_suggester);

  RTCErrorOr<Codecs> GetNegotiatedCodecsForAnswer(
      const MediaDescriptionOptions& media_description_options,
      const MediaSessionOptions& session_options,
      RtpTransceiverDirection offer_rtd,
      RtpTransceiverDirection answer_rtd,
      const ContentInfo* current_content,
      std::vector<Codec> codecs_from_offer,
      PayloadTypeSuggester& pt_suggester);

  // Function exposed for issues.webrtc.org/412904801
  // Modify the video codecs to return on subsequent GetNegotiated* calls.
  // The input is a vector of pairs of codecs.
  // For each pair, the first element is the codec to be replaced,
  // and the second element is the codec to replace it with.
  void ModifyVideoCodecs(const std::vector<std::pair<Codec, Codec>>& changes);

  // Functions exposed for testing
  CodecList audio_sendrecv_codecs() const;
  const CodecList& audio_send_codecs() const;
  const CodecList& audio_recv_codecs() const;
  CodecList video_sendrecv_codecs() const;
  const CodecList& video_send_codecs() const;
  const CodecList& video_recv_codecs() const;

 private:
  CodecList GetAudioCodecsForOffer(
      const RtpTransceiverDirection& direction) const;
  CodecList GetAudioCodecsForAnswer(
      const RtpTransceiverDirection& offer,
      const RtpTransceiverDirection& answer) const;
  CodecList GetVideoCodecsForOffer(
      const RtpTransceiverDirection& direction) const;
  CodecList GetVideoCodecsForAnswer(
      const RtpTransceiverDirection& offer,
      const RtpTransceiverDirection& answer) const;

  // Makes sure that modifications and reading data is done on the same thread
  // and to makessure we consistently make calls to GetNegotiatedCodecsForOffer
  // and GetNegotiatedCodecsForAnswer in the same calling context.
  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_;

  const TypedCodecVendor audio_send_codecs_;
  const TypedCodecVendor audio_recv_codecs_;

  // TODO: bugs.webrtc.org/412904801 - Make const. In order to be able to do
  // that, `ModifyVideoCodecs` needs to be removed. In the meantime, codec
  // information must be read and modified on the same task queue.
  TypedCodecVendor video_send_codecs_ RTC_GUARDED_BY(sequence_checker_);
  TypedCodecVendor video_recv_codecs_ RTC_GUARDED_BY(sequence_checker_);
};

// A class to assist in looking up data for a codec mapping.
// Pure virtual to allow implementations that depend on things that
// codec_vendor.h should not depend on.
// Pointers returned are not stable, and should not be stored.
class CodecLookupHelper {
 public:
  virtual ~CodecLookupHelper() = default;
  virtual ::webrtc::PayloadTypeSuggester* PayloadTypeSuggester() = 0;
  // Look up the codec vendor to use, depending on context.
  // This call may get additional arguments in the future, to aid
  // in selection of the correct context.
  virtual CodecVendor* GetCodecVendor() = 0;
};

// A helper function to merge codecs numbered in one PT numberspace
// into a list numbered in another PT numberspace. Exposed for testing.
RTCError MergeCodecsForTesting(const CodecList& reference_codecs,
                               absl::string_view mid,
                               CodecList& offered_codecs,
                               PayloadTypeSuggester& pt_suggester);

}  //  namespace webrtc

#endif  // PC_CODEC_VENDOR_H_
