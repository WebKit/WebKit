/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_RTPTRANSCEIVER_H_
#define PC_RTPTRANSCEIVER_H_

#include <string>
#include <vector>

#include "api/rtptransceiverinterface.h"
#include "pc/rtpreceiver.h"
#include "pc/rtpsender.h"

namespace webrtc {

// Implementation of the public RtpTransceiverInterface.
//
// The RtpTransceiverInterface is only intended to be used with a PeerConnection
// that enables Unified Plan SDP. Thus, the methods that only need to implement
// public API features and are not used internally can assume exactly one sender
// and receiver.
//
// Since the RtpTransceiver is used internally by PeerConnection for tracking
// RtpSenders, RtpReceivers, and BaseChannels, and PeerConnection needs to be
// backwards compatible with Plan B SDP, this implementation is more flexible
// than that required by the WebRTC specification.
//
// With Plan B SDP, an RtpTransceiver can have any number of senders and
// receivers which map to a=ssrc lines in the m= section.
// With Unified Plan SDP, an RtpTransceiver will have exactly one sender and one
// receiver which are encapsulated by the m= section.
//
// This class manages the RtpSenders, RtpReceivers, and BaseChannel associated
// with this m= section. Since the transceiver, senders, and receivers are
// reference counted and can be referenced from JavaScript (in Chromium), these
// objects must be ready to live for an arbitrary amount of time. The
// BaseChannel is not reference counted and is owned by the ChannelManager, so
// the PeerConnection must take care of creating/deleting the BaseChannel and
// setting the channel reference in the transceiver to null when it has been
// deleted.
//
// The RtpTransceiver is specialized to either audio or video according to the
// MediaType specified in the constructor. Audio RtpTransceivers will have
// AudioRtpSenders, AudioRtpReceivers, and a VoiceChannel. Video RtpTransceivers
// will have VideoRtpSenders, VideoRtpReceivers, and a VideoChannel.
class RtpTransceiver final
    : public rtc::RefCountedObject<RtpTransceiverInterface> {
 public:
  // Construct a Plan B-style RtpTransceiver with no senders, receivers, or
  // channel set.
  // |media_type| specifies the type of RtpTransceiver (and, by transitivity,
  // the type of senders, receivers, and channel). Can either by audio or video.
  explicit RtpTransceiver(cricket::MediaType media_type);
  // Construct a Unified Plan-style RtpTransceiver with the given sender and
  // receiver. The media type will be derived from the media types of the sender
  // and receiver. The sender and receiver should have the same media type.
  RtpTransceiver(
      rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender,
      rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
          receiver);
  ~RtpTransceiver() override;

  cricket::MediaType media_type() const { return media_type_; }

  // Returns the Voice/VideoChannel set for this transceiver. May be null if
  // the transceiver is not in the currently set local/remote description.
  cricket::BaseChannel* channel() const { return channel_; }

  // Sets the Voice/VideoChannel. The caller must pass in the correct channel
  // implementation based on the type of the transceiver.
  void SetChannel(cricket::BaseChannel* channel);

  // Adds an RtpSender of the appropriate type to be owned by this transceiver.
  // Must not be null.
  void AddSender(
      rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>> sender);

  // Removes the given RtpSender. Returns false if the sender is not owned by
  // this transceiver.
  bool RemoveSender(RtpSenderInterface* sender);

  // Returns a vector of the senders owned by this transceiver.
  std::vector<rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>>
  senders() const {
    return senders_;
  }

  // Adds an RtpReceiver of the appropriate type to be owned by this
  // transceiver. Must not be null.
  void AddReceiver(
      rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>
          receiver);

  // Removes the given RtpReceiver. Returns false if the sender is not owned by
  // this transceiver.
  bool RemoveReceiver(RtpReceiverInterface* receiver);

  // Returns a vector of the receivers owned by this transceiver.
  std::vector<
      rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>>
  receivers() const {
    return receivers_;
  }

  // RtpTransceiverInterface implementation.
  rtc::Optional<std::string> mid() const override;
  rtc::scoped_refptr<RtpSenderInterface> sender() const override;
  rtc::scoped_refptr<RtpReceiverInterface> receiver() const override;
  bool stopped() const override;
  RtpTransceiverDirection direction() const override;
  void SetDirection(RtpTransceiverDirection new_direction) override;
  rtc::Optional<RtpTransceiverDirection> current_direction() const override;
  void Stop() override;
  void SetCodecPreferences(rtc::ArrayView<RtpCodecCapability> codecs) override;

 private:
  const bool unified_plan_;
  const cricket::MediaType media_type_;
  std::vector<rtc::scoped_refptr<RtpSenderProxyWithInternal<RtpSenderInternal>>>
      senders_;
  std::vector<
      rtc::scoped_refptr<RtpReceiverProxyWithInternal<RtpReceiverInternal>>>
      receivers_;

  bool stopped_ = false;
  RtpTransceiverDirection direction_ = RtpTransceiverDirection::kInactive;
  rtc::Optional<RtpTransceiverDirection> current_direction_;
  rtc::Optional<std::string> mid_;

  cricket::BaseChannel* channel_ = nullptr;
};

BEGIN_SIGNALING_PROXY_MAP(RtpTransceiver)
PROXY_SIGNALING_THREAD_DESTRUCTOR()
PROXY_CONSTMETHOD0(rtc::Optional<std::string>, mid);
PROXY_CONSTMETHOD0(rtc::scoped_refptr<RtpSenderInterface>, sender);
PROXY_CONSTMETHOD0(rtc::scoped_refptr<RtpReceiverInterface>, receiver);
PROXY_CONSTMETHOD0(bool, stopped);
PROXY_CONSTMETHOD0(RtpTransceiverDirection, direction);
PROXY_METHOD1(void, SetDirection, RtpTransceiverDirection);
PROXY_CONSTMETHOD0(rtc::Optional<RtpTransceiverDirection>, current_direction);
PROXY_METHOD0(void, Stop);
PROXY_METHOD1(void, SetCodecPreferences, rtc::ArrayView<RtpCodecCapability>);
END_PROXY_MAP();

}  // namespace webrtc

#endif  // PC_RTPTRANSCEIVER_H_
