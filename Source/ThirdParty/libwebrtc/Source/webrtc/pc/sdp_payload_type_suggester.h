/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_SDP_PAYLOAD_TYPE_SUGGESTER_H_
#define PC_SDP_PAYLOAD_TYPE_SUGGESTER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>

#include "absl/strings/string_view.h"
#include "api/environment/environment.h"
#include "api/jsep.h"
#include "api/payload_type.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_header_extension_id.h"
#include "api/rtp_parameters.h"
#include "call/payload_type.h"
#include "call/payload_type_picker.h"
#include "media/base/codec.h"
#include "pc/jsep_transport_collection.h"
#include "pc/session_description.h"

namespace webrtc {
// Helper class to assist in payload type assignment.
// This class lives on the signaling thread.
class SdpPayloadTypeSuggester : public PayloadTypeSuggester {
 public:
  explicit SdpPayloadTypeSuggester(
      PeerConnectionInterface::BundlePolicy bundle_policy,
      const Environment& env)
      : env_(env), bundle_manager_(bundle_policy) {}
  SdpPayloadTypeSuggester(const SdpPayloadTypeSuggester&) = delete;
  SdpPayloadTypeSuggester& operator=(const SdpPayloadTypeSuggester&) = delete;
  SdpPayloadTypeSuggester(SdpPayloadTypeSuggester&&) = delete;
  SdpPayloadTypeSuggester& operator=(SdpPayloadTypeSuggester&&) = delete;

  // Implementation of PayloadTypeSuggester
  RTCErrorOr<PayloadType> SuggestPayloadType(
      absl::string_view mid,
      const Codec& codec,
      bool pick_from_top_of_range = false) override;
  RTCError AddLocalMapping(absl::string_view mid,
                           PayloadType payload_type,
                           const Codec& codec) override;
  // Suggest an ID for a given RTP header extension on a given media section.
  RTCErrorOr<RtpHeaderExtensionId> SuggestRtpHeaderExtensionId(
      absl::string_view mid,
      const RtpExtension& extension,
      RtpTransceiverIdDomain id_domain) override;
  // Register an RTP header extension ID as mapped to a specific extension.
  [[nodiscard]] RTCError AddRtpHeaderExtensionMapping(
      absl::string_view mid,
      const RtpExtension& extension,
      bool local) override;
  // Updating the bundle mappings and recording PT assignments
  RTCError Update(const SessionDescription* description,
                  bool local,
                  SdpType type);

 private:
  // Records the association of local and remote payload types with a bundle.
  class BundleTypeRecorder {
   public:
    explicit BundleTypeRecorder(PayloadTypePicker& picker,
                                const Environment& env)
        : local_payload_types_(picker),
          remote_payload_types_(picker),
          header_extensions_(env) {}

    PayloadTypeRecorder& local_payload_types() { return local_payload_types_; }
    PayloadTypeRecorder& remote_payload_types() {
      return remote_payload_types_;
    }
    RtpHeaderExtensionRecorder& header_extensions() {
      return header_extensions_;
    }

   private:
    PayloadTypeRecorder local_payload_types_;
    PayloadTypeRecorder remote_payload_types_;
    RtpHeaderExtensionRecorder header_extensions_;
  };
  PayloadTypeRecorder& LookupRecorder(absl::string_view mid, bool local);
  BundleTypeRecorder& LookupBundleRecorder(absl::string_view mid);
  PayloadTypePicker payload_type_picker_;
  RtpHeaderExtensionPicker rtp_header_extension_picker_;
  const Environment env_;
  // Record of bundle groups, used for looking up payload type suggesters.
  // This class also exists on the network thread, in JsepTransportController.
  BundleManager bundle_manager_;
  std::map<std::string, BundleTypeRecorder> recorder_by_mid_;
};

}  // namespace webrtc

#endif  // PC_SDP_PAYLOAD_TYPE_SUGGESTER_H_
