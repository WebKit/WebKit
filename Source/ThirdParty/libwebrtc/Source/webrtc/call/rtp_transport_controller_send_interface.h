/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_CALL_RTP_TRANSPORT_CONTROLLER_SEND_INTERFACE_H_
#define WEBRTC_CALL_RTP_TRANSPORT_CONTROLLER_SEND_INTERFACE_H_

namespace webrtc {

class PacketRouter;
class RtpPacketSender;
class SendSideCongestionController;
class TransportFeedbackObserver;

// An RtpTransportController should own everything related to the RTP
// transport to/from a remote endpoint. We should have separate
// interfaces for send and receive side, even if they are implemented
// by the same class. This is an ongoing refactoring project. At some
// point, this class should be promoted to a public api under
// webrtc/api/rtp/.
//
// For a start, this object is just a collection of the objects needed
// by the VideoSendStream constructor. The plan is to move ownership
// of all RTP-related objects here, and add methods to create per-ssrc
// objects which would then be passed to VideoSendStream. Eventually,
// direct accessors like packet_router() should be removed.
//
// This should also have a reference to the underlying
// webrtc::Transport(s). Currently, webrtc::Transport is implemented by
// WebRtcVideoChannel and WebRtcVoiceMediaChannel, and owned by
// WebrtcSession. Video and audio always uses different transport
// objects, even in the common case where they are bundled over the
// same underlying transport.
//
// Extracting the logic of the webrtc::Transport from BaseChannel and
// subclasses into a separate class seems to be a prerequesite for
// moving the transport here.
class RtpTransportControllerSendInterface {
 public:
  virtual ~RtpTransportControllerSendInterface() {}
  virtual PacketRouter* packet_router() = 0;
  // Currently returning the same pointer, but with different types.
  virtual SendSideCongestionController* send_side_cc() = 0;
  virtual TransportFeedbackObserver* transport_feedback_observer() = 0;

  virtual RtpPacketSender* packet_sender() = 0;
};

}  // namespace webrtc

#endif  // WEBRTC_CALL_RTP_TRANSPORT_CONTROLLER_SEND_INTERFACE_H_
