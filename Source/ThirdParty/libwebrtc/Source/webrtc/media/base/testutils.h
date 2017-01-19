/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_BASE_TESTUTILS_H_
#define WEBRTC_MEDIA_BASE_TESTUTILS_H_

#include <string>
#include <vector>

#include "libyuv/compare.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/window.h"
#include "webrtc/media/base/mediachannel.h"
#include "webrtc/media/base/videocapturer.h"
#include "webrtc/media/base/videocommon.h"

namespace rtc {
class ByteBufferReader;
class ByteBufferWriter;
class StreamInterface;
}

namespace webrtc {
class VideoFrame;
}

namespace cricket {

// Returns size of 420 image with rounding on chroma for odd sizes.
#define I420_SIZE(w, h) (w * h + (((w + 1) / 2) * ((h + 1) / 2)) * 2)
// Returns size of ARGB image.
#define ARGB_SIZE(w, h) (w * h * 4)

template <class T> inline std::vector<T> MakeVector(const T a[], size_t s) {
  return std::vector<T>(a, a + s);
}
#define MAKE_VECTOR(a) cricket::MakeVector(a, arraysize(a))

struct RtpDumpPacket;
class RtpDumpWriter;

struct RawRtpPacket {
  void WriteToByteBuffer(uint32_t in_ssrc, rtc::ByteBufferWriter* buf) const;
  bool ReadFromByteBuffer(rtc::ByteBufferReader* buf);
  // Check if this packet is the same as the specified packet except the
  // sequence number and timestamp, which should be the same as the specified
  // parameters.
  bool SameExceptSeqNumTimestampSsrc(const RawRtpPacket& packet,
                                     uint16_t seq,
                                     uint32_t ts,
                                     uint32_t ssc) const;
  int size() const { return 28; }

  uint8_t ver_to_cc;
  uint8_t m_to_pt;
  uint16_t sequence_number;
  uint32_t timestamp;
  uint32_t ssrc;
  char payload[16];
};

struct RawRtcpPacket {
  void WriteToByteBuffer(rtc::ByteBufferWriter* buf) const;
  bool ReadFromByteBuffer(rtc::ByteBufferReader* buf);
  bool EqualsTo(const RawRtcpPacket& packet) const;

  uint8_t ver_to_count;
  uint8_t type;
  uint16_t length;
  char payload[16];
};

class RtpTestUtility {
 public:
  static size_t GetTestPacketCount();

  // Write the first count number of kTestRawRtcpPackets or kTestRawRtpPackets,
  // depending on the flag rtcp. If it is RTP, use the specified SSRC. Return
  // true if successful.
  static bool WriteTestPackets(size_t count,
                               bool rtcp,
                               uint32_t rtp_ssrc,
                               RtpDumpWriter* writer);

  // Loop read the first count number of packets from the specified stream.
  // Verify the elapsed time of the dump packets increase monotonically. If the
  // stream is a RTP stream, verify the RTP sequence number, timestamp, and
  // payload. If the stream is a RTCP stream, verify the RTCP header and
  // payload.
  static bool VerifyTestPacketsFromStream(size_t count,
                                          rtc::StreamInterface* stream,
                                          uint32_t ssrc);

  // Verify the dump packet is the same as the raw RTP packet.
  static bool VerifyPacket(const RtpDumpPacket* dump,
                           const RawRtpPacket* raw,
                           bool header_only);

  static const uint32_t kDefaultSsrc = 1;
  static const uint32_t kRtpTimestampIncrease = 90;
  static const uint32_t kDefaultTimeIncrease = 30;
  static const uint32_t kElapsedTimeInterval = 10;
  static const RawRtpPacket kTestRawRtpPackets[];
  static const RawRtcpPacket kTestRawRtcpPackets[];

 private:
  RtpTestUtility() {}
};

// Test helper for testing VideoCapturer implementations.
class VideoCapturerListener
    : public sigslot::has_slots<>,
      public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  explicit VideoCapturerListener(VideoCapturer* cap);
  ~VideoCapturerListener();

  CaptureState last_capture_state() const { return last_capture_state_; }
  int frame_count() const { return frame_count_; }
  int frame_width() const { return frame_width_; }
  int frame_height() const { return frame_height_; }
  bool resolution_changed() const { return resolution_changed_; }

  void OnStateChange(VideoCapturer* capturer, CaptureState state);
  void OnFrame(const webrtc::VideoFrame& frame) override;

 private:
  VideoCapturer* capturer_;
  CaptureState last_capture_state_;
  int frame_count_;
  int frame_width_;
  int frame_height_;
  bool resolution_changed_;
};

class ScreencastEventCatcher : public sigslot::has_slots<> {
 public:
  ScreencastEventCatcher() : ssrc_(0), ev_(rtc::WE_RESIZE) { }
  uint32_t ssrc() const { return ssrc_; }
  rtc::WindowEvent event() const { return ev_; }
  void OnEvent(uint32_t ssrc, rtc::WindowEvent ev) {
    ssrc_ = ssrc;
    ev_ = ev;
  }
 private:
  uint32_t ssrc_;
  rtc::WindowEvent ev_;
};

class VideoMediaErrorCatcher : public sigslot::has_slots<> {
 public:
  VideoMediaErrorCatcher() : ssrc_(0), error_(VideoMediaChannel::ERROR_NONE) { }
  uint32_t ssrc() const { return ssrc_; }
  VideoMediaChannel::Error error() const { return error_; }
  void OnError(uint32_t ssrc, VideoMediaChannel::Error error) {
    ssrc_ = ssrc;
    error_ = error;
  }
 private:
  uint32_t ssrc_;
  VideoMediaChannel::Error error_;
};

// Checks whether |codecs| contains |codec|; checks using Codec::Matches().
template <class C>
bool ContainsMatchingCodec(const std::vector<C>& codecs, const C& codec) {
  typename std::vector<C>::const_iterator it;
  for (it = codecs.begin(); it != codecs.end(); ++it) {
    if (it->Matches(codec)) {
      return true;
    }
  }
  return false;
}

// Create Simulcast StreamParams with given |ssrcs| and |cname|.
cricket::StreamParams CreateSimStreamParams(const std::string& cname,
                                            const std::vector<uint32_t>& ssrcs);
// Create Simulcast stream with given |ssrcs| and |rtx_ssrcs|.
// The number of |rtx_ssrcs| must match number of |ssrcs|.
cricket::StreamParams CreateSimWithRtxStreamParams(
    const std::string& cname,
    const std::vector<uint32_t>& ssrcs,
    const std::vector<uint32_t>& rtx_ssrcs);

}  // namespace cricket

#endif  // WEBRTC_MEDIA_BASE_TESTUTILS_H_
