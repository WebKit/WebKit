/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RTP_TRANSPORT_CONTROLLER_SEND_INTERFACE_H_
#define CALL_RTP_TRANSPORT_CONTROLLER_SEND_INTERFACE_H_

namespace webrtc {

class PacedSender;
class PacketRouter;
class RtpPacketSender;
struct RtpKeepAliveConfig;
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
  virtual PacedSender* pacer() = 0;
  // Currently returning the same pointer, but with different types.
  virtual SendSideCongestionController* send_side_cc() = 0;
  virtual TransportFeedbackObserver* transport_feedback_observer() = 0;

  virtual RtpPacketSender* packet_sender() = 0;
  virtual const RtpKeepAliveConfig& keepalive_config() const = 0;

  // SetAllocatedSendBitrateLimits sets bitrates limits imposed by send codec
  // settings.
  // |min_send_bitrate_bps| is the total minimum send bitrate required by all
  // sending streams.  This is the minimum bitrate the PacedSender will use.
  // Note that SendSideCongestionController::OnNetworkChanged can still be
  // called with a lower bitrate estimate. |max_padding_bitrate_bps| is the max
  // bitrate the send streams request for padding. This can be higher than the
  // current network estimate and tells the PacedSender how much it should max
  // pad unless there is real packets to send.
  virtual void SetAllocatedSendBitrateLimits(int min_send_bitrate_bps,
                                             int max_padding_bitrate_bps) = 0;
};

}  // namespace webrtc

#endif  // CALL_RTP_TRANSPORT_CONTROLLER_SEND_INTERFACE_H_
