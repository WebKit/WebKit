/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_RTPTRANSCEIVERINTERFACE_H_
#define API_RTPTRANSCEIVERINTERFACE_H_

#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/mediatypes.h"
#include "api/rtpparameters.h"
#include "api/rtpreceiverinterface.h"
#include "api/rtpsenderinterface.h"
#include "rtc_base/refcount.h"
#include "rtc_base/scoped_ref_ptr.h"

namespace webrtc {

// https://w3c.github.io/webrtc-pc/#dom-rtcrtptransceiverdirection
enum class RtpTransceiverDirection {
  kSendRecv,
  kSendOnly,
  kRecvOnly,
  kInactive
};

// Structure for initializing an RtpTransceiver in a call to
// PeerConnectionInterface::AddTransceiver.
// https://w3c.github.io/webrtc-pc/#dom-rtcrtptransceiverinit
struct RtpTransceiverInit final {
  RtpTransceiverInit();
  RtpTransceiverInit(const RtpTransceiverInit&);
  ~RtpTransceiverInit();
  // Direction of the RtpTransceiver. See RtpTransceiverInterface::direction().
  RtpTransceiverDirection direction = RtpTransceiverDirection::kSendRecv;

  // The added RtpTransceiver will be added to these streams.
  std::vector<std::string> stream_ids;

  // TODO(bugs.webrtc.org/7600): Not implemented.
  std::vector<RtpEncodingParameters> send_encodings;
};

// The RtpTransceiverInterface maps to the RTCRtpTransceiver defined by the
// WebRTC specification. A transceiver represents a combination of an RtpSender
// and an RtpReceiver than share a common mid. As defined in JSEP, an
// RtpTransceiver is said to be associated with a media description if its mid
// property is non-null; otherwise, it is said to be disassociated.
// JSEP: https://tools.ietf.org/html/draft-ietf-rtcweb-jsep-24
//
// Note that RtpTransceivers are only supported when using PeerConnection with
// Unified Plan SDP.
//
// This class is thread-safe.
//
// WebRTC specification for RTCRtpTransceiver, the JavaScript analog:
// https://w3c.github.io/webrtc-pc/#dom-rtcrtptransceiver
class RtpTransceiverInterface : public rtc::RefCountInterface {
 public:
  // Media type of the transceiver. Any sender(s)/receiver(s) will have this
  // type as well.
  virtual cricket::MediaType media_type() const = 0;

  // The mid attribute is the mid negotiated and present in the local and
  // remote descriptions. Before negotiation is complete, the mid value may be
  // null. After rollbacks, the value may change from a non-null value to null.
  // https://w3c.github.io/webrtc-pc/#dom-rtcrtptransceiver-mid
  virtual absl::optional<std::string> mid() const = 0;

  // The sender attribute exposes the RtpSender corresponding to the RTP media
  // that may be sent with the transceiver's mid. The sender is always present,
  // regardless of the direction of media.
  // https://w3c.github.io/webrtc-pc/#dom-rtcrtptransceiver-sender
  virtual rtc::scoped_refptr<RtpSenderInterface> sender() const = 0;

  // The receiver attribute exposes the RtpReceiver corresponding to the RTP
  // media that may be received with the transceiver's mid. The receiver is
  // always present, regardless of the direction of media.
  // https://w3c.github.io/webrtc-pc/#dom-rtcrtptransceiver-receiver
  virtual rtc::scoped_refptr<RtpReceiverInterface> receiver() const = 0;

  // The stopped attribute indicates that the sender of this transceiver will no
  // longer send, and that the receiver will no longer receive. It is true if
  // either stop has been called or if setting the local or remote description
  // has caused the RtpTransceiver to be stopped.
  // https://w3c.github.io/webrtc-pc/#dom-rtcrtptransceiver-stopped
  virtual bool stopped() const = 0;

  // The direction attribute indicates the preferred direction of this
  // transceiver, which will be used in calls to CreateOffer and CreateAnswer.
  // https://w3c.github.io/webrtc-pc/#dom-rtcrtptransceiver-direction
  virtual RtpTransceiverDirection direction() const = 0;

  // Sets the preferred direction of this transceiver. An update of
  // directionality does not take effect immediately. Instead, future calls to
  // CreateOffer and CreateAnswer mark the corresponding media descriptions as
  // sendrecv, sendonly, recvonly, or inactive.
  // https://w3c.github.io/webrtc-pc/#dom-rtcrtptransceiver-direction
  virtual void SetDirection(RtpTransceiverDirection new_direction) = 0;

  // The current_direction attribute indicates the current direction negotiated
  // for this transceiver. If this transceiver has never been represented in an
  // offer/answer exchange, or if the transceiver is stopped, the value is null.
  // https://w3c.github.io/webrtc-pc/#dom-rtcrtptransceiver-currentdirection
  virtual absl::optional<RtpTransceiverDirection> current_direction() const = 0;

  // An internal slot designating for which direction the relevant
  // PeerConnection events have been fired. This is to ensure that events like
  // OnAddTrack only get fired once even if the same session description is
  // applied again.
  // Exposed in the public interface for use by Chromium.
  virtual absl::optional<RtpTransceiverDirection> fired_direction() const;

  // The Stop method irreversibly stops the RtpTransceiver. The sender of this
  // transceiver will no longer send, the receiver will no longer receive.
  // https://w3c.github.io/webrtc-pc/#dom-rtcrtptransceiver-stop
  virtual void Stop() = 0;

  // The SetCodecPreferences method overrides the default codec preferences used
  // by WebRTC for this transceiver.
  // https://w3c.github.io/webrtc-pc/#dom-rtcrtptransceiver-setcodecpreferences
  // TODO(steveanton): Not implemented.
  virtual void SetCodecPreferences(rtc::ArrayView<RtpCodecCapability> codecs);

 protected:
  ~RtpTransceiverInterface() override = default;
};

}  // namespace webrtc

#endif  // API_RTPTRANSCEIVERINTERFACE_H_
