/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_BASE_VIDEOENGINE_UNITTEST_H_  // NOLINT
#define MEDIA_BASE_VIDEOENGINE_UNITTEST_H_

#include <memory>
#include <string>
#include <vector>

#include "call/call.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "media/base/fakenetworkinterface.h"
#include "media/base/fakevideocapturer.h"
#include "media/base/fakevideorenderer.h"
#include "media/base/mediachannel.h"
#include "media/base/streamparams.h"
#include "media/engine/fakewebrtccall.h"
#include "rtc_base/bytebuffer.h"
#include "rtc_base/gunit.h"
#include "rtc_base/timeutils.h"

namespace cricket {
class WebRtcVideoEncoderFactory;
class WebRtcVideoDecoderFactory;
}  // namespace cricket

#define EXPECT_FRAME_WAIT(c, w, h, t) \
  EXPECT_EQ_WAIT((c), renderer_.num_rendered_frames(), (t)); \
  EXPECT_EQ((w), renderer_.width()); \
  EXPECT_EQ((h), renderer_.height()); \
  EXPECT_EQ(0, renderer_.errors()); \

#define EXPECT_FRAME_ON_RENDERER_WAIT(r, c, w, h, t) \
  EXPECT_EQ_WAIT((c), (r).num_rendered_frames(), (t)); \
  EXPECT_EQ((w), (r).width()); \
  EXPECT_EQ((h), (r).height()); \
  EXPECT_EQ(0, (r).errors()); \

#define EXPECT_GT_FRAME_ON_RENDERER_WAIT(r, c, w, h, t) \
  EXPECT_TRUE_WAIT((r).num_rendered_frames() >= (c) && \
                   (w) == (r).width() && \
                   (h) == (r).height(), (t)); \
  EXPECT_EQ(0, (r).errors());

static const uint32_t kTimeout = 5000U;
static const uint32_t kDefaultReceiveSsrc = 0;
static const uint32_t kSsrc = 1234u;
static const uint32_t kRtxSsrc = 4321u;
static const uint32_t kSsrcs4[] = {1, 2, 3, 4};
static const int kVideoWidth = 640;
static const int kVideoHeight = 360;
static const int kFramerate = 30;

inline bool IsEqualCodec(const cricket::VideoCodec& a,
                         const cricket::VideoCodec& b) {
  return a.id == b.id && a.name == b.name;
}

template<class E, class C>
class VideoMediaChannelTest : public testing::Test,
                              public sigslot::has_slots<> {
 protected:
  VideoMediaChannelTest<E, C>()
      : call_(webrtc::Call::Create(webrtc::Call::Config(&event_log_))),
        engine_(std::unique_ptr<cricket::WebRtcVideoEncoderFactory>(),
                std::unique_ptr<cricket::WebRtcVideoDecoderFactory>()) {}

  virtual cricket::VideoCodec DefaultCodec() = 0;

  virtual cricket::StreamParams DefaultSendStreamParams() {
    return cricket::StreamParams::CreateLegacy(kSsrc);
  }

  virtual void SetUp() {
    cricket::MediaConfig media_config;
    // Disabling cpu overuse detection actually disables quality scaling too; it
    // implies DegradationPreference kMaintainResolution. Automatic scaling
    // needs to be disabled, otherwise, tests which check the size of received
    // frames become flaky.
    media_config.video.enable_cpu_overuse_detection = false;
    channel_.reset(engine_.CreateChannel(call_.get(), media_config,
                                         cricket::VideoOptions()));
    channel_->OnReadyToSend(true);
    EXPECT_TRUE(channel_.get() != NULL);
    network_interface_.SetDestination(channel_.get());
    channel_->SetInterface(&network_interface_);
    media_error_ = cricket::VideoMediaChannel::ERROR_NONE;
    cricket::VideoRecvParameters parameters;
    parameters.codecs = engine_.codecs();
    channel_->SetRecvParameters(parameters);
    EXPECT_TRUE(channel_->AddSendStream(DefaultSendStreamParams()));
    video_capturer_.reset(CreateFakeVideoCapturer());
    cricket::VideoFormat format(640, 480,
                                cricket::VideoFormat::FpsToInterval(kFramerate),
                                cricket::FOURCC_I420);
    EXPECT_EQ(cricket::CS_RUNNING, video_capturer_->Start(format));
    EXPECT_TRUE(
        channel_->SetVideoSend(kSsrc, true, nullptr, video_capturer_.get()));
  }

  virtual cricket::FakeVideoCapturer* CreateFakeVideoCapturer() {
    return new cricket::FakeVideoCapturer();
  }

  // Utility method to setup an additional stream to send and receive video.
  // Used to test send and recv between two streams.
  void SetUpSecondStream() {
    SetUpSecondStreamWithNoRecv();
    // Setup recv for second stream.
    EXPECT_TRUE(channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(kSsrc + 2)));
    // Make the second renderer available for use by a new stream.
    EXPECT_TRUE(channel_->SetSink(kSsrc + 2, &renderer2_));
  }
  // Setup an additional stream just to send video. Defer add recv stream.
  // This is required if you want to test unsignalled recv of video rtp packets.
  void SetUpSecondStreamWithNoRecv() {
    // SetUp() already added kSsrc make sure duplicate SSRCs cant be added.
    EXPECT_TRUE(channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(kSsrc)));
    EXPECT_TRUE(channel_->SetSink(kSsrc, &renderer_));
    EXPECT_FALSE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kSsrc)));
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kSsrc + 2)));
    // We dont add recv for the second stream.

    // Setup the receive and renderer for second stream after send.
    video_capturer_2_.reset(CreateFakeVideoCapturer());
    cricket::VideoFormat format(640, 480,
                                cricket::VideoFormat::FpsToInterval(kFramerate),
                                cricket::FOURCC_I420);
    EXPECT_EQ(cricket::CS_RUNNING, video_capturer_2_->Start(format));

    EXPECT_TRUE(channel_->SetVideoSend(kSsrc + 2, true, nullptr,
                                       video_capturer_2_.get()));
  }
  virtual void TearDown() {
    channel_.reset();
  }
  bool SetDefaultCodec() {
    return SetOneCodec(DefaultCodec());
  }

  bool SetOneCodec(int pt, const char* name) {
    return SetOneCodec(cricket::VideoCodec(pt, name));
  }
  bool SetOneCodec(const cricket::VideoCodec& codec) {
    cricket::VideoFormat capture_format(
        kVideoWidth, kVideoHeight,
        cricket::VideoFormat::FpsToInterval(kFramerate), cricket::FOURCC_I420);

    if (video_capturer_) {
      EXPECT_EQ(cricket::CS_RUNNING, video_capturer_->Start(capture_format));
    }
    if (video_capturer_2_) {
      EXPECT_EQ(cricket::CS_RUNNING, video_capturer_2_->Start(capture_format));
    }

    bool sending = channel_->sending();
    bool success = SetSend(false);
    if (success) {
      cricket::VideoSendParameters parameters;
      parameters.codecs.push_back(codec);
      success = channel_->SetSendParameters(parameters);
    }
    if (success) {
      success = SetSend(sending);
    }
    return success;
  }
  bool SetSend(bool send) {
    return channel_->SetSend(send);
  }
  int DrainOutgoingPackets() {
    int packets = 0;
    do {
      packets = NumRtpPackets();
      // 100 ms should be long enough.
      rtc::Thread::Current()->ProcessMessages(100);
    } while (NumRtpPackets() > packets);
    return NumRtpPackets();
  }
  bool SendFrame() {
    if (video_capturer_2_) {
      video_capturer_2_->CaptureFrame();
    }
    return video_capturer_.get() &&
        video_capturer_->CaptureFrame();
  }
  bool WaitAndSendFrame(int wait_ms) {
    bool ret = rtc::Thread::Current()->ProcessMessages(wait_ms);
    ret &= SendFrame();
    return ret;
  }
  // Sends frames and waits for the decoder to be fully initialized.
  // Returns the number of frames that were sent.
  int WaitForDecoder() {
#if defined(HAVE_OPENMAX)
    // Send enough frames for the OpenMAX decoder to continue processing, and
    // return the number of frames sent.
    // Send frames for a full kTimeout's worth of 15fps video.
    int frame_count = 0;
    while (frame_count < static_cast<int>(kTimeout) / 66) {
      EXPECT_TRUE(WaitAndSendFrame(66));
      ++frame_count;
    }
    return frame_count;
#else
    return 0;
#endif
  }
  bool SendCustomVideoFrame(int w, int h) {
    if (!video_capturer_.get()) return false;
    return video_capturer_->CaptureCustomFrame(w, h, cricket::FOURCC_I420);
  }
  int NumRtpBytes() {
    return network_interface_.NumRtpBytes();
  }
  int NumRtpBytes(uint32_t ssrc) {
    return network_interface_.NumRtpBytes(ssrc);
  }
  int NumRtpPackets() {
    return network_interface_.NumRtpPackets();
  }
  int NumRtpPackets(uint32_t ssrc) {
    return network_interface_.NumRtpPackets(ssrc);
  }
  int NumSentSsrcs() {
    return network_interface_.NumSentSsrcs();
  }
  const rtc::CopyOnWriteBuffer* GetRtpPacket(int index) {
    return network_interface_.GetRtpPacket(index);
  }
  int NumRtcpPackets() {
    return network_interface_.NumRtcpPackets();
  }
  const rtc::CopyOnWriteBuffer* GetRtcpPacket(int index) {
    return network_interface_.GetRtcpPacket(index);
  }
  static int GetPayloadType(const rtc::CopyOnWriteBuffer* p) {
    int pt = -1;
    ParseRtpPacket(p, NULL, &pt, NULL, NULL, NULL, NULL);
    return pt;
  }
  static bool ParseRtpPacket(const rtc::CopyOnWriteBuffer* p,
                             bool* x,
                             int* pt,
                             int* seqnum,
                             uint32_t* tstamp,
                             uint32_t* ssrc,
                             std::string* payload) {
    rtc::ByteBufferReader buf(p->data<char>(), p->size());
    uint8_t u08 = 0;
    uint16_t u16 = 0;
    uint32_t u32 = 0;

    // Read X and CC fields.
    if (!buf.ReadUInt8(&u08)) return false;
    bool extension = ((u08 & 0x10) != 0);
    uint8_t cc = (u08 & 0x0F);
    if (x) *x = extension;

    // Read PT field.
    if (!buf.ReadUInt8(&u08)) return false;
    if (pt) *pt = (u08 & 0x7F);

    // Read Sequence Number field.
    if (!buf.ReadUInt16(&u16)) return false;
    if (seqnum) *seqnum = u16;

    // Read Timestamp field.
    if (!buf.ReadUInt32(&u32)) return false;
    if (tstamp) *tstamp = u32;

    // Read SSRC field.
    if (!buf.ReadUInt32(&u32)) return false;
    if (ssrc) *ssrc = u32;

    // Skip CSRCs.
    for (uint8_t i = 0; i < cc; ++i) {
      if (!buf.ReadUInt32(&u32)) return false;
    }

    // Skip extension header.
    if (extension) {
      // Read Profile-specific extension header ID
      if (!buf.ReadUInt16(&u16)) return false;

      // Read Extension header length
      if (!buf.ReadUInt16(&u16)) return false;
      uint16_t ext_header_len = u16;

      // Read Extension header
      for (uint16_t i = 0; i < ext_header_len; ++i) {
        if (!buf.ReadUInt32(&u32)) return false;
      }
    }

    if (payload) {
      return buf.ReadString(payload, buf.Length());
    }
    return true;
  }

  // Parse all RTCP packet, from start_index to stop_index, and count how many
  // FIR (PT=206 and FMT=4 according to RFC 5104). If successful, set the count
  // and return true.
  bool CountRtcpFir(int start_index, int stop_index, int* fir_count) {
    int count = 0;
    for (int i = start_index; i < stop_index; ++i) {
      std::unique_ptr<const rtc::CopyOnWriteBuffer> p(GetRtcpPacket(i));
      rtc::ByteBufferReader buf(p->data<char>(), p->size());
      size_t total_len = 0;
      // The packet may be a compound RTCP packet.
      while (total_len < p->size()) {
        // Read FMT, type and length.
        uint8_t fmt = 0;
        uint8_t type = 0;
        uint16_t length = 0;
        if (!buf.ReadUInt8(&fmt)) return false;
        fmt &= 0x1F;
        if (!buf.ReadUInt8(&type)) return false;
        if (!buf.ReadUInt16(&length)) return false;
        buf.Consume(length * 4);  // Skip RTCP data.
        total_len += (length + 1) * 4;
        if ((192 == type) || ((206 == type) && (4 == fmt))) {
          ++count;
        }
      }
    }

    if (fir_count) {
      *fir_count = count;
    }
    return true;
  }

  void OnVideoChannelError(uint32_t ssrc,
                           cricket::VideoMediaChannel::Error error) {
    media_error_ = error;
  }

  // Test that SetSend works.
  void SetSend() {
    EXPECT_FALSE(channel_->sending());
    EXPECT_TRUE(
        channel_->SetVideoSend(kSsrc, true, nullptr, video_capturer_.get()));
    EXPECT_TRUE(SetOneCodec(DefaultCodec()));
    EXPECT_FALSE(channel_->sending());
    EXPECT_TRUE(SetSend(true));
    EXPECT_TRUE(channel_->sending());
    EXPECT_TRUE(SendFrame());
    EXPECT_TRUE_WAIT(NumRtpPackets() > 0, kTimeout);
    EXPECT_TRUE(SetSend(false));
    EXPECT_FALSE(channel_->sending());
  }
  // Test that SetSend fails without codecs being set.
  void SetSendWithoutCodecs() {
    EXPECT_FALSE(channel_->sending());
    EXPECT_FALSE(SetSend(true));
    EXPECT_FALSE(channel_->sending());
  }
  // Test that we properly set the send and recv buffer sizes by the time
  // SetSend is called.
  void SetSendSetsTransportBufferSizes() {
    EXPECT_TRUE(SetOneCodec(DefaultCodec()));
    EXPECT_TRUE(SetSend(true));
    EXPECT_EQ(64 * 1024, network_interface_.sendbuf_size());
    EXPECT_EQ(64 * 1024, network_interface_.recvbuf_size());
  }
  // Tests that we can send frames and the right payload type is used.
  void Send(const cricket::VideoCodec& codec) {
    EXPECT_TRUE(SetOneCodec(codec));
    EXPECT_TRUE(SetSend(true));
    EXPECT_TRUE(SendFrame());
    EXPECT_TRUE_WAIT(NumRtpPackets() > 0, kTimeout);
    std::unique_ptr<const rtc::CopyOnWriteBuffer> p(GetRtpPacket(0));
    EXPECT_EQ(codec.id, GetPayloadType(p.get()));
  }
  // Tests that we can send and receive frames.
  void SendAndReceive(const cricket::VideoCodec& codec) {
    EXPECT_TRUE(SetOneCodec(codec));
    EXPECT_TRUE(SetSend(true));
    EXPECT_TRUE(channel_->SetSink(kDefaultReceiveSsrc, &renderer_));
    EXPECT_EQ(0, renderer_.num_rendered_frames());
    EXPECT_TRUE(SendFrame());
    EXPECT_FRAME_WAIT(1, kVideoWidth, kVideoHeight, kTimeout);
    std::unique_ptr<const rtc::CopyOnWriteBuffer> p(GetRtpPacket(0));
    EXPECT_EQ(codec.id, GetPayloadType(p.get()));
  }
  void SendReceiveManyAndGetStats(const cricket::VideoCodec& codec,
                                  int duration_sec, int fps) {
    EXPECT_TRUE(SetOneCodec(codec));
    EXPECT_TRUE(SetSend(true));
    EXPECT_TRUE(channel_->SetSink(kDefaultReceiveSsrc, &renderer_));
    EXPECT_EQ(0, renderer_.num_rendered_frames());
    for (int i = 0; i < duration_sec; ++i) {
      for (int frame = 1; frame <= fps; ++frame) {
        EXPECT_TRUE(WaitAndSendFrame(1000 / fps));
        EXPECT_FRAME_WAIT(frame + i * fps, kVideoWidth, kVideoHeight, kTimeout);
      }
    }
    std::unique_ptr<const rtc::CopyOnWriteBuffer> p(GetRtpPacket(0));
    EXPECT_EQ(codec.id, GetPayloadType(p.get()));
  }

  // Test that stats work properly for a 1-1 call.
  void GetStats() {
    const int kDurationSec = 3;
    const int kFps = 10;
    SendReceiveManyAndGetStats(DefaultCodec(), kDurationSec, kFps);

    cricket::VideoMediaInfo info;
    EXPECT_TRUE(channel_->GetStats(&info));

    ASSERT_EQ(1U, info.senders.size());
    // TODO(whyuan): bytes_sent and bytes_rcvd are different. Are both payload?
    // For webrtc, bytes_sent does not include the RTP header length.
    EXPECT_GT(info.senders[0].bytes_sent, 0);
    EXPECT_EQ(NumRtpPackets(), info.senders[0].packets_sent);
    EXPECT_EQ(0.0, info.senders[0].fraction_lost);
    ASSERT_TRUE(info.senders[0].codec_payload_type);
    EXPECT_EQ(DefaultCodec().id, *info.senders[0].codec_payload_type);
    EXPECT_EQ(0, info.senders[0].firs_rcvd);
    EXPECT_EQ(0, info.senders[0].plis_rcvd);
    EXPECT_EQ(0, info.senders[0].nacks_rcvd);
    EXPECT_EQ(kVideoWidth, info.senders[0].send_frame_width);
    EXPECT_EQ(kVideoHeight, info.senders[0].send_frame_height);
    EXPECT_GT(info.senders[0].framerate_input, 0);
    EXPECT_GT(info.senders[0].framerate_sent, 0);

    EXPECT_EQ(1U, info.send_codecs.count(DefaultCodec().id));
    EXPECT_EQ(DefaultCodec().ToCodecParameters(),
              info.send_codecs[DefaultCodec().id]);

    ASSERT_EQ(1U, info.receivers.size());
    EXPECT_EQ(1U, info.senders[0].ssrcs().size());
    EXPECT_EQ(1U, info.receivers[0].ssrcs().size());
    EXPECT_EQ(info.senders[0].ssrcs()[0], info.receivers[0].ssrcs()[0]);
    ASSERT_TRUE(info.receivers[0].codec_payload_type);
    EXPECT_EQ(DefaultCodec().id, *info.receivers[0].codec_payload_type);
    EXPECT_EQ(NumRtpBytes(), info.receivers[0].bytes_rcvd);
    EXPECT_EQ(NumRtpPackets(), info.receivers[0].packets_rcvd);
    EXPECT_EQ(0.0, info.receivers[0].fraction_lost);
    EXPECT_EQ(0, info.receivers[0].packets_lost);
    // TODO(asapersson): Not set for webrtc. Handle missing stats.
    // EXPECT_EQ(0, info.receivers[0].packets_concealed);
    EXPECT_EQ(0, info.receivers[0].firs_sent);
    EXPECT_EQ(0, info.receivers[0].plis_sent);
    EXPECT_EQ(0, info.receivers[0].nacks_sent);
    EXPECT_EQ(kVideoWidth, info.receivers[0].frame_width);
    EXPECT_EQ(kVideoHeight, info.receivers[0].frame_height);
    EXPECT_GT(info.receivers[0].framerate_rcvd, 0);
    EXPECT_GT(info.receivers[0].framerate_decoded, 0);
    EXPECT_GT(info.receivers[0].framerate_output, 0);

    EXPECT_EQ(1U, info.receive_codecs.count(DefaultCodec().id));
    EXPECT_EQ(DefaultCodec().ToCodecParameters(),
              info.receive_codecs[DefaultCodec().id]);
  }

  cricket::VideoSenderInfo GetSenderStats(size_t i) {
    cricket::VideoMediaInfo info;
    EXPECT_TRUE(channel_->GetStats(&info));
    return info.senders[i];
  }

  cricket::VideoReceiverInfo GetReceiverStats(size_t i) {
    cricket::VideoMediaInfo info;
    EXPECT_TRUE(channel_->GetStats(&info));
    return info.receivers[i];
  }

  // Test that stats work properly for a conf call with multiple recv streams.
  void GetStatsMultipleRecvStreams() {
    cricket::FakeVideoRenderer renderer1, renderer2;
    EXPECT_TRUE(SetOneCodec(DefaultCodec()));
    cricket::VideoSendParameters parameters;
    parameters.codecs.push_back(DefaultCodec());
    parameters.conference_mode = true;
    EXPECT_TRUE(channel_->SetSendParameters(parameters));
    EXPECT_TRUE(SetSend(true));
    EXPECT_TRUE(channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(1)));
    EXPECT_TRUE(channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(2)));
    EXPECT_TRUE(channel_->SetSink(1, &renderer1));
    EXPECT_TRUE(channel_->SetSink(2, &renderer2));
    EXPECT_EQ(0, renderer1.num_rendered_frames());
    EXPECT_EQ(0, renderer2.num_rendered_frames());
    std::vector<uint32_t> ssrcs;
    ssrcs.push_back(1);
    ssrcs.push_back(2);
    network_interface_.SetConferenceMode(true, ssrcs);
    EXPECT_TRUE(SendFrame());
    EXPECT_FRAME_ON_RENDERER_WAIT(renderer1, 1, kVideoWidth, kVideoHeight,
                                  kTimeout);
    EXPECT_FRAME_ON_RENDERER_WAIT(renderer2, 1, kVideoWidth, kVideoHeight,
                                  kTimeout);

    EXPECT_TRUE(channel_->SetSend(false));

    cricket::VideoMediaInfo info;
    EXPECT_TRUE(channel_->GetStats(&info));
    ASSERT_EQ(1U, info.senders.size());
    // TODO(whyuan): bytes_sent and bytes_rcvd are different. Are both payload?
    // For webrtc, bytes_sent does not include the RTP header length.
    EXPECT_GT(GetSenderStats(0).bytes_sent, 0);
    EXPECT_EQ_WAIT(NumRtpPackets(), GetSenderStats(0).packets_sent, kTimeout);
    EXPECT_EQ(kVideoWidth, GetSenderStats(0).send_frame_width);
    EXPECT_EQ(kVideoHeight, GetSenderStats(0).send_frame_height);

    ASSERT_EQ(2U, info.receivers.size());
    for (size_t i = 0; i < info.receivers.size(); ++i) {
      EXPECT_EQ(1U, GetReceiverStats(i).ssrcs().size());
      EXPECT_EQ(i + 1, GetReceiverStats(i).ssrcs()[0]);
      EXPECT_EQ_WAIT(NumRtpBytes(), GetReceiverStats(i).bytes_rcvd, kTimeout);
      EXPECT_EQ_WAIT(NumRtpPackets(), GetReceiverStats(i).packets_rcvd,
                     kTimeout);
      EXPECT_EQ_WAIT(kVideoWidth, GetReceiverStats(i).frame_width, kTimeout);
      EXPECT_EQ_WAIT(kVideoHeight, GetReceiverStats(i).frame_height, kTimeout);
    }
  }
  // Test that stats work properly for a conf call with multiple send streams.
  void GetStatsMultipleSendStreams() {
    // Normal setup; note that we set the SSRC explicitly to ensure that
    // it will come first in the senders map.
    EXPECT_TRUE(SetOneCodec(DefaultCodec()));
    cricket::VideoSendParameters parameters;
    parameters.codecs.push_back(DefaultCodec());
    parameters.conference_mode = true;
    EXPECT_TRUE(channel_->SetSendParameters(parameters));
    EXPECT_TRUE(channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(kSsrc)));
    EXPECT_TRUE(channel_->SetSink(kSsrc, &renderer_));
    EXPECT_TRUE(SetSend(true));
    EXPECT_TRUE(SendFrame());
    EXPECT_TRUE_WAIT(NumRtpPackets() > 0, kTimeout);
    EXPECT_FRAME_WAIT(1, kVideoWidth, kVideoHeight, kTimeout);

    // Add an additional capturer, and hook up a renderer to receive it.
    cricket::FakeVideoRenderer renderer2;
    std::unique_ptr<cricket::FakeVideoCapturer> capturer(
        CreateFakeVideoCapturer());
    const int kTestWidth = 160;
    const int kTestHeight = 120;
    cricket::VideoFormat format(kTestWidth, kTestHeight,
                                cricket::VideoFormat::FpsToInterval(5),
                                cricket::FOURCC_I420);
    EXPECT_EQ(cricket::CS_RUNNING, capturer->Start(format));
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(5678)));
    EXPECT_TRUE(channel_->SetVideoSend(5678, true, nullptr, capturer.get()));
    EXPECT_TRUE(channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(5678)));
    EXPECT_TRUE(channel_->SetSink(5678, &renderer2));
    EXPECT_TRUE(capturer->CaptureCustomFrame(
        kTestWidth, kTestHeight, cricket::FOURCC_I420));
    EXPECT_FRAME_ON_RENDERER_WAIT(
        renderer2, 1, kTestWidth, kTestHeight, kTimeout);

    // Get stats, and make sure they are correct for two senders. We wait until
    // the number of expected packets have been sent to avoid races where we
    // check stats before it has been updated.
    cricket::VideoMediaInfo info;
    for (uint32_t i = 0; i < kTimeout; ++i) {
      rtc::Thread::Current()->ProcessMessages(1);
      EXPECT_TRUE(channel_->GetStats(&info));
      ASSERT_EQ(2U, info.senders.size());
      if (info.senders[0].packets_sent + info.senders[1].packets_sent ==
          NumRtpPackets()) {
        // Stats have been updated for both sent frames, expectations can be
        // checked now.
        break;
      }
    }
    EXPECT_EQ(NumRtpPackets(),
              info.senders[0].packets_sent + info.senders[1].packets_sent)
        << "Timed out while waiting for packet counts for all sent packets.";
    EXPECT_EQ(1U, info.senders[0].ssrcs().size());
    EXPECT_EQ(1234U, info.senders[0].ssrcs()[0]);
    EXPECT_EQ(kVideoWidth, info.senders[0].send_frame_width);
    EXPECT_EQ(kVideoHeight, info.senders[0].send_frame_height);
    EXPECT_EQ(1U, info.senders[1].ssrcs().size());
    EXPECT_EQ(5678U, info.senders[1].ssrcs()[0]);
    EXPECT_EQ(kTestWidth, info.senders[1].send_frame_width);
    EXPECT_EQ(kTestHeight, info.senders[1].send_frame_height);
    // The capturer must be unregistered here as it runs out of it's scope next.
    channel_->SetVideoSend(5678, true, nullptr, nullptr);
  }

  // Test that we can set the bandwidth.
  void SetSendBandwidth() {
    cricket::VideoSendParameters parameters;
    parameters.codecs.push_back(DefaultCodec());
    parameters.max_bandwidth_bps = -1;  // <= 0 means unlimited.
    EXPECT_TRUE(channel_->SetSendParameters(parameters));
    parameters.max_bandwidth_bps = 128 * 1024;
    EXPECT_TRUE(channel_->SetSendParameters(parameters));
  }
  // Test that we can set the SSRC for the default send source.
  void SetSendSsrc() {
    EXPECT_TRUE(SetDefaultCodec());
    EXPECT_TRUE(SetSend(true));
    EXPECT_TRUE(SendFrame());
    EXPECT_TRUE_WAIT(NumRtpPackets() > 0, kTimeout);
    uint32_t ssrc = 0;
    std::unique_ptr<const rtc::CopyOnWriteBuffer> p(GetRtpPacket(0));
    ParseRtpPacket(p.get(), NULL, NULL, NULL, NULL, &ssrc, NULL);
    EXPECT_EQ(kSsrc, ssrc);
    // Packets are being paced out, so these can mismatch between the first and
    // second call to NumRtpPackets until pending packets are paced out.
    EXPECT_EQ_WAIT(NumRtpPackets(), NumRtpPackets(ssrc), kTimeout);
    EXPECT_EQ_WAIT(NumRtpBytes(), NumRtpBytes(ssrc), kTimeout);
    EXPECT_EQ(1, NumSentSsrcs());
    EXPECT_EQ(0, NumRtpPackets(kSsrc - 1));
    EXPECT_EQ(0, NumRtpBytes(kSsrc - 1));
  }
  // Test that we can set the SSRC even after codecs are set.
  void SetSendSsrcAfterSetCodecs() {
    // Remove stream added in Setup.
    EXPECT_TRUE(channel_->RemoveSendStream(kSsrc));
    EXPECT_TRUE(SetDefaultCodec());
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(999)));
    EXPECT_TRUE(
        channel_->SetVideoSend(999u, true, nullptr, video_capturer_.get()));
    EXPECT_TRUE(SetSend(true));
    EXPECT_TRUE(WaitAndSendFrame(0));
    EXPECT_TRUE_WAIT(NumRtpPackets() > 0, kTimeout);
    uint32_t ssrc = 0;
    std::unique_ptr<const rtc::CopyOnWriteBuffer> p(GetRtpPacket(0));
    ParseRtpPacket(p.get(), NULL, NULL, NULL, NULL, &ssrc, NULL);
    EXPECT_EQ(999u, ssrc);
    // Packets are being paced out, so these can mismatch between the first and
    // second call to NumRtpPackets until pending packets are paced out.
    EXPECT_EQ_WAIT(NumRtpPackets(), NumRtpPackets(ssrc), kTimeout);
    EXPECT_EQ_WAIT(NumRtpBytes(), NumRtpBytes(ssrc), kTimeout);
    EXPECT_EQ(1, NumSentSsrcs());
    EXPECT_EQ(0, NumRtpPackets(kSsrc));
    EXPECT_EQ(0, NumRtpBytes(kSsrc));
  }
  // Test that we can set the default video renderer before and after
  // media is received.
  void SetSink() {
    uint8_t data1[] = {
        0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    rtc::CopyOnWriteBuffer packet1(data1, sizeof(data1));
    rtc::SetBE32(packet1.data() + 8, kSsrc);
    channel_->SetSink(kDefaultReceiveSsrc, NULL);
    EXPECT_TRUE(SetDefaultCodec());
    EXPECT_TRUE(SetSend(true));
    EXPECT_EQ(0, renderer_.num_rendered_frames());
    channel_->OnPacketReceived(&packet1, rtc::PacketTime());
    EXPECT_TRUE(channel_->SetSink(kDefaultReceiveSsrc, &renderer_));
    EXPECT_TRUE(SendFrame());
    EXPECT_FRAME_WAIT(1, kVideoWidth, kVideoHeight, kTimeout);
  }

  // Tests empty StreamParams is rejected.
  void RejectEmptyStreamParams() {
    // Remove the send stream that was added during Setup.
    EXPECT_TRUE(channel_->RemoveSendStream(kSsrc));

    cricket::StreamParams empty;
    EXPECT_FALSE(channel_->AddSendStream(empty));
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(789u)));
  }

  // Tests setting up and configuring a send stream.
  void AddRemoveSendStreams() {
    EXPECT_TRUE(SetOneCodec(DefaultCodec()));
    EXPECT_TRUE(SetSend(true));
    EXPECT_TRUE(channel_->SetSink(kDefaultReceiveSsrc, &renderer_));
    EXPECT_TRUE(SendFrame());
    EXPECT_FRAME_WAIT(1, kVideoWidth, kVideoHeight, kTimeout);
    EXPECT_GT(NumRtpPackets(), 0);
    uint32_t ssrc = 0;
    size_t last_packet = NumRtpPackets() - 1;
    std::unique_ptr<const rtc::CopyOnWriteBuffer>
        p(GetRtpPacket(static_cast<int>(last_packet)));
    ParseRtpPacket(p.get(), NULL, NULL, NULL, NULL, &ssrc, NULL);
    EXPECT_EQ(kSsrc, ssrc);

    // Remove the send stream that was added during Setup.
    EXPECT_TRUE(channel_->RemoveSendStream(kSsrc));
    int rtp_packets = NumRtpPackets();

    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(789u)));
    EXPECT_TRUE(
        channel_->SetVideoSend(789u, true, nullptr, video_capturer_.get()));
    EXPECT_EQ(rtp_packets, NumRtpPackets());
    // Wait 30ms to guarantee the engine does not drop the frame.
    EXPECT_TRUE(WaitAndSendFrame(30));
    EXPECT_TRUE_WAIT(NumRtpPackets() > rtp_packets, kTimeout);

    last_packet = NumRtpPackets() - 1;
    p.reset(GetRtpPacket(static_cast<int>(last_packet)));
    ParseRtpPacket(p.get(), NULL, NULL, NULL, NULL, &ssrc, NULL);
    EXPECT_EQ(789u, ssrc);
  }

  // Tests the behavior of incoming streams in a conference scenario.
  void SimulateConference() {
    cricket::FakeVideoRenderer renderer1, renderer2;
    EXPECT_TRUE(SetDefaultCodec());
    cricket::VideoSendParameters parameters;
    parameters.codecs.push_back(DefaultCodec());
    parameters.conference_mode = true;
    EXPECT_TRUE(channel_->SetSendParameters(parameters));
    EXPECT_TRUE(SetSend(true));
    EXPECT_TRUE(channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(1)));
    EXPECT_TRUE(channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(2)));
    EXPECT_TRUE(channel_->SetSink(1, &renderer1));
    EXPECT_TRUE(channel_->SetSink(2, &renderer2));
    EXPECT_EQ(0, renderer1.num_rendered_frames());
    EXPECT_EQ(0, renderer2.num_rendered_frames());
    std::vector<uint32_t> ssrcs;
    ssrcs.push_back(1);
    ssrcs.push_back(2);
    network_interface_.SetConferenceMode(true, ssrcs);
    EXPECT_TRUE(SendFrame());
    EXPECT_FRAME_ON_RENDERER_WAIT(renderer1, 1, kVideoWidth, kVideoHeight,
                                  kTimeout);
    EXPECT_FRAME_ON_RENDERER_WAIT(renderer2, 1, kVideoWidth, kVideoHeight,
                                  kTimeout);

    std::unique_ptr<const rtc::CopyOnWriteBuffer> p(GetRtpPacket(0));
    EXPECT_EQ(DefaultCodec().id, GetPayloadType(p.get()));
    EXPECT_EQ(kVideoWidth, renderer1.width());
    EXPECT_EQ(kVideoHeight, renderer1.height());
    EXPECT_EQ(kVideoWidth, renderer2.width());
    EXPECT_EQ(kVideoHeight, renderer2.height());
    EXPECT_TRUE(channel_->RemoveRecvStream(2));
    EXPECT_TRUE(channel_->RemoveRecvStream(1));
  }

  // Tests that we can add and remove capturers and frames are sent out properly
  void AddRemoveCapturer() {
    cricket::VideoCodec codec = DefaultCodec();
    const int time_between_send_ms =
        cricket::VideoFormat::FpsToInterval(kFramerate);
    EXPECT_TRUE(SetOneCodec(codec));
    EXPECT_TRUE(SetSend(true));
    EXPECT_TRUE(channel_->SetSink(kDefaultReceiveSsrc, &renderer_));
    EXPECT_EQ(0, renderer_.num_rendered_frames());
    EXPECT_TRUE(SendFrame());
    EXPECT_FRAME_WAIT(1, kVideoWidth, kVideoHeight, kTimeout);
    std::unique_ptr<cricket::FakeVideoCapturer> capturer(
        CreateFakeVideoCapturer());

    // TODO(nisse): This testcase fails if we don't configure
    // screencast. It's unclear why, I see nothing obvious in this
    // test which is related to screencast logic.
    cricket::VideoOptions video_options;
    video_options.is_screencast = true;
    channel_->SetVideoSend(kSsrc, true, &video_options, nullptr);

    cricket::VideoFormat format(480, 360,
                                cricket::VideoFormat::FpsToInterval(30),
                                cricket::FOURCC_I420);
    EXPECT_EQ(cricket::CS_RUNNING, capturer->Start(format));
    // All capturers start generating frames with the same timestamp. ViE does
    // not allow the same timestamp to be used. Capture one frame before
    // associating the capturer with the channel.
    EXPECT_TRUE(capturer->CaptureCustomFrame(format.width, format.height,
                                             cricket::FOURCC_I420));

    int captured_frames = 1;
    for (int iterations = 0; iterations < 2; ++iterations) {
      EXPECT_TRUE(channel_->SetVideoSend(kSsrc, true, nullptr, capturer.get()));
      rtc::Thread::Current()->ProcessMessages(time_between_send_ms);
      EXPECT_TRUE(capturer->CaptureCustomFrame(format.width, format.height,
                                               cricket::FOURCC_I420));
      ++captured_frames;
      // Wait until frame of right size is captured.
      EXPECT_TRUE_WAIT(renderer_.num_rendered_frames() >= captured_frames &&
                       format.width == renderer_.width() &&
                       format.height == renderer_.height() &&
                       !renderer_.black_frame(), kTimeout);
      EXPECT_GE(renderer_.num_rendered_frames(), captured_frames);
      EXPECT_EQ(format.width, renderer_.width());
      EXPECT_EQ(format.height, renderer_.height());
      captured_frames = renderer_.num_rendered_frames() + 1;
      EXPECT_FALSE(renderer_.black_frame());
      EXPECT_TRUE(channel_->SetVideoSend(kSsrc, true, nullptr, nullptr));
      // Make sure a black frame is generated within the specified timeout.
      // The black frame should be the resolution of the previous frame to
      // prevent expensive encoder reconfigurations.
      EXPECT_TRUE_WAIT(renderer_.num_rendered_frames() >= captured_frames &&
                       format.width == renderer_.width() &&
                       format.height == renderer_.height() &&
                       renderer_.black_frame(), kTimeout);
      EXPECT_GE(renderer_.num_rendered_frames(), captured_frames);
      EXPECT_EQ(format.width, renderer_.width());
      EXPECT_EQ(format.height, renderer_.height());
      EXPECT_TRUE(renderer_.black_frame());

      // The black frame has the same timestamp as the next frame since it's
      // timestamp is set to the last frame's timestamp + interval. WebRTC will
      // not render a frame with the same timestamp so capture another frame
      // with the frame capturer to increment the next frame's timestamp.
      EXPECT_TRUE(capturer->CaptureCustomFrame(format.width, format.height,
                                               cricket::FOURCC_I420));
    }
  }

  // Tests that if SetVideoSend is called with a NULL capturer after the
  // capturer was already removed, the application doesn't crash (and no black
  // frame is sent).
  void RemoveCapturerWithoutAdd() {
    EXPECT_TRUE(SetOneCodec(DefaultCodec()));
    EXPECT_TRUE(SetSend(true));
    EXPECT_TRUE(channel_->SetSink(kDefaultReceiveSsrc, &renderer_));
    EXPECT_EQ(0, renderer_.num_rendered_frames());
    EXPECT_TRUE(SendFrame());
    EXPECT_FRAME_WAIT(1, kVideoWidth, kVideoHeight, kTimeout);
    // Wait for one frame so they don't get dropped because we send frames too
    // tightly.
    rtc::Thread::Current()->ProcessMessages(30);
    // Remove the capturer.
    EXPECT_TRUE(channel_->SetVideoSend(kSsrc, true, nullptr, nullptr));

    // No capturer was added, so this SetVideoSend shouldn't do anything.
    EXPECT_TRUE(channel_->SetVideoSend(kSsrc, true, nullptr, nullptr));
    rtc::Thread::Current()->ProcessMessages(300);
    // Verify no more frames were sent.
    EXPECT_EQ(1, renderer_.num_rendered_frames());
  }

  // Tests that we can add and remove capturer as unique sources.
  void AddRemoveCapturerMultipleSources() {
    // WebRTC implementation will drop frames if pushed to quickly. Wait the
    // interval time to avoid that.
    // WebRTC implementation will drop frames if pushed to quickly. Wait the
    // interval time to avoid that.
    // Set up the stream associated with the engine.
    EXPECT_TRUE(channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(kSsrc)));
    EXPECT_TRUE(channel_->SetSink(kSsrc, &renderer_));
    cricket::VideoFormat capture_format;  // default format
    capture_format.interval = cricket::VideoFormat::FpsToInterval(kFramerate);
    // Set up additional stream 1.
    cricket::FakeVideoRenderer renderer1;
    EXPECT_FALSE(channel_->SetSink(1, &renderer1));
    EXPECT_TRUE(channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(1)));
    EXPECT_TRUE(channel_->SetSink(1, &renderer1));
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(1)));
    std::unique_ptr<cricket::FakeVideoCapturer> capturer1(
        CreateFakeVideoCapturer());
    EXPECT_EQ(cricket::CS_RUNNING, capturer1->Start(capture_format));
    // Set up additional stream 2.
    cricket::FakeVideoRenderer renderer2;
    EXPECT_FALSE(channel_->SetSink(2, &renderer2));
    EXPECT_TRUE(channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(2)));
    EXPECT_TRUE(channel_->SetSink(2, &renderer2));
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(2)));
    std::unique_ptr<cricket::FakeVideoCapturer> capturer2(
        CreateFakeVideoCapturer());
    EXPECT_EQ(cricket::CS_RUNNING, capturer2->Start(capture_format));
    // State for all the streams.
    EXPECT_TRUE(SetOneCodec(DefaultCodec()));
    // A limitation in the lmi implementation requires that SetVideoSend() is
    // called after SetOneCodec().
    // TODO(hellner): this seems like an unnecessary constraint, fix it.
    EXPECT_TRUE(channel_->SetVideoSend(1, true, nullptr, capturer1.get()));
    EXPECT_TRUE(channel_->SetVideoSend(2, true, nullptr, capturer2.get()));
    EXPECT_TRUE(SetSend(true));
    // Test capturer associated with engine.
    const int kTestWidth = 160;
    const int kTestHeight = 120;
    EXPECT_TRUE(capturer1->CaptureCustomFrame(
        kTestWidth, kTestHeight, cricket::FOURCC_I420));
    EXPECT_FRAME_ON_RENDERER_WAIT(
        renderer1, 1, kTestWidth, kTestHeight, kTimeout);
    // Capture a frame with additional capturer2, frames should be received
    EXPECT_TRUE(capturer2->CaptureCustomFrame(
        kTestWidth, kTestHeight, cricket::FOURCC_I420));
    EXPECT_FRAME_ON_RENDERER_WAIT(
        renderer2, 1, kTestWidth, kTestHeight, kTimeout);
    // Successfully remove the capturer.
    EXPECT_TRUE(channel_->SetVideoSend(kSsrc, true, nullptr, nullptr));
    // The capturers must be unregistered here as it runs out of it's scope
    // next.
    EXPECT_TRUE(channel_->SetVideoSend(1, true, nullptr, nullptr));
    EXPECT_TRUE(channel_->SetVideoSend(2, true, nullptr, nullptr));
  }

  // Test that multiple send streams can be created and deleted properly.
  void MultipleSendStreams() {
    // Remove stream added in Setup. I.e. remove stream corresponding to default
    // channel.
    EXPECT_TRUE(channel_->RemoveSendStream(kSsrc));
    const unsigned int kSsrcsSize = sizeof(kSsrcs4)/sizeof(kSsrcs4[0]);
    for (unsigned int i = 0; i < kSsrcsSize; ++i) {
      EXPECT_TRUE(channel_->AddSendStream(
          cricket::StreamParams::CreateLegacy(kSsrcs4[i])));
    }
    // Delete one of the non default channel streams, let the destructor delete
    // the remaining ones.
    EXPECT_TRUE(channel_->RemoveSendStream(kSsrcs4[kSsrcsSize - 1]));
    // Stream should already be deleted.
    EXPECT_FALSE(channel_->RemoveSendStream(kSsrcs4[kSsrcsSize - 1]));
  }

  // Two streams one channel tests.

  // Tests that we can send and receive frames.
  void TwoStreamsSendAndReceive(const cricket::VideoCodec& codec) {
    SetUpSecondStream();
    // Test sending and receiving on first stream.
    SendAndReceive(codec);
    // Test sending and receiving on second stream.
    EXPECT_EQ_WAIT(1, renderer2_.num_rendered_frames(), kTimeout);
    EXPECT_GT(NumRtpPackets(), 0);
    EXPECT_EQ(1, renderer2_.num_rendered_frames());
  }

  webrtc::RtcEventLogNullImpl event_log_;
  const std::unique_ptr<webrtc::Call> call_;
  E engine_;
  std::unique_ptr<cricket::FakeVideoCapturer> video_capturer_;
  std::unique_ptr<cricket::FakeVideoCapturer> video_capturer_2_;
  std::unique_ptr<C> channel_;
  cricket::FakeNetworkInterface network_interface_;
  cricket::FakeVideoRenderer renderer_;
  cricket::VideoMediaChannel::Error media_error_;

  // Used by test cases where 2 streams are run on the same channel.
  cricket::FakeVideoRenderer renderer2_;
};

#endif  // MEDIA_BASE_VIDEOENGINE_UNITTEST_H_  NOLINT
