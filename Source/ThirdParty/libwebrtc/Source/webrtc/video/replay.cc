/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <map>
#include <memory>
#include <sstream>

#include "api/video_codecs/video_decoder.h"
#include "call/call.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "rtc_base/checks.h"
#include "rtc_base/flags.h"
#include "rtc_base/string_to_number.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/sleep.h"
#include "test/call_test.h"
#include "test/encoder_settings.h"
#include "test/fake_decoder.h"
#include "test/gtest.h"
#include "test/null_transport.h"
#include "test/rtp_file_reader.h"
#include "test/run_loop.h"
#include "test/run_test.h"
#include "test/testsupport/frame_writer.h"
#include "test/video_capturer.h"
#include "test/video_renderer.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace {

static bool ValidatePayloadType(int32_t payload_type) {
  return payload_type > 0 && payload_type <= 127;
}

static bool ValidateSsrc(const char* ssrc_string) {
  return rtc::StringToNumber<uint32_t>(ssrc_string).has_value();
}

static bool ValidateOptionalPayloadType(int32_t payload_type) {
  return payload_type == -1 || ValidatePayloadType(payload_type);
}

static bool ValidateRtpHeaderExtensionId(int32_t extension_id) {
  return extension_id >= -1 && extension_id < 15;
}

bool ValidateInputFilenameNotEmpty(const std::string& string) {
  return !string.empty();
}

}  // namespace

namespace webrtc {
namespace flags {

// TODO(pbos): Multiple receivers.

// Flag for payload type.
DEFINE_int(payload_type, test::CallTest::kPayloadTypeVP8, "Payload type");
static int PayloadType() { return static_cast<int>(FLAG_payload_type); }

DEFINE_int(payload_type_rtx,
           test::CallTest::kSendRtxPayloadType,
           "RTX payload type");
static int PayloadTypeRtx() {
  return static_cast<int>(FLAG_payload_type_rtx);
}

// Flag for SSRC.
const std::string& DefaultSsrc() {
  static const std::string ssrc = std::to_string(
      test::CallTest::kVideoSendSsrcs[0]);
  return ssrc;
}
DEFINE_string(ssrc, DefaultSsrc().c_str(), "Incoming SSRC");
static uint32_t Ssrc() {
  return rtc::StringToNumber<uint32_t>(FLAG_ssrc).value();
}

const std::string& DefaultSsrcRtx() {
  static const std::string ssrc_rtx = std::to_string(
      test::CallTest::kSendRtxSsrcs[0]);
  return ssrc_rtx;
}
DEFINE_string(ssrc_rtx, DefaultSsrcRtx().c_str(), "Incoming RTX SSRC");
static uint32_t SsrcRtx() {
  return rtc::StringToNumber<uint32_t>(FLAG_ssrc_rtx).value();
}

// Flag for RED payload type.
DEFINE_int(red_payload_type, -1, "RED payload type");
static int RedPayloadType() {
  return static_cast<int>(FLAG_red_payload_type);
}

// Flag for ULPFEC payload type.
DEFINE_int(fec_payload_type, -1, "ULPFEC payload type");
static int FecPayloadType() {
  return static_cast<int>(FLAG_fec_payload_type);
}

// Flag for abs-send-time id.
DEFINE_int(abs_send_time_id, -1, "RTP extension ID for abs-send-time");
static int AbsSendTimeId() { return static_cast<int>(FLAG_abs_send_time_id); }

// Flag for transmission-offset id.
DEFINE_int(transmission_offset_id,
           -1,
           "RTP extension ID for transmission-offset");
static int TransmissionOffsetId() {
  return static_cast<int>(FLAG_transmission_offset_id);
}

// Flag for rtpdump input file.
DEFINE_string(input_file, "", "input file");
static std::string InputFile() {
  return static_cast<std::string>(FLAG_input_file);
}

// Flag for raw output files.
DEFINE_string(out_base, "", "Basename (excluding .jpg) for raw output");
static std::string OutBase() {
  return static_cast<std::string>(FLAG_out_base);
}

DEFINE_string(decoder_bitstream_filename, "", "Decoder bitstream output file");
static std::string DecoderBitstreamFilename() {
  return static_cast<std::string>(FLAG_decoder_bitstream_filename);
}

// Flag for video codec.
DEFINE_string(codec, "VP8", "Video codec");
static std::string Codec() { return static_cast<std::string>(FLAG_codec); }

DEFINE_bool(help, false, "Print this message.");
}  // namespace flags

static const uint32_t kReceiverLocalSsrc = 0x123456;

class FileRenderPassthrough : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  FileRenderPassthrough(const std::string& basename,
                        rtc::VideoSinkInterface<VideoFrame>* renderer)
      : basename_(basename), renderer_(renderer), file_(nullptr), count_(0) {}

  ~FileRenderPassthrough() {
    if (file_)
      fclose(file_);
  }

 private:
  void OnFrame(const VideoFrame& video_frame) override {
    if (renderer_)
      renderer_->OnFrame(video_frame);

    if (basename_.empty())
      return;

    std::stringstream filename;
    filename << basename_ << count_++ << "_" << video_frame.timestamp()
             << ".jpg";

    test::JpegFrameWriter frame_writer(filename.str());
    RTC_CHECK(frame_writer.WriteFrame(video_frame, 100));
  }

  const std::string basename_;
  rtc::VideoSinkInterface<VideoFrame>* const renderer_;
  FILE* file_;
  size_t count_;
};

class DecoderBitstreamFileWriter : public EncodedFrameObserver {
 public:
  explicit DecoderBitstreamFileWriter(const char* filename)
      : file_(fopen(filename, "wb")) {
    RTC_DCHECK(file_);
  }
  ~DecoderBitstreamFileWriter() { fclose(file_); }

  virtual void EncodedFrameCallback(const EncodedFrame& encoded_frame) {
    fwrite(encoded_frame.data_, 1, encoded_frame.length_, file_);
  }

 private:
  FILE* file_;
};

void RtpReplay() {
  std::stringstream window_title;
  window_title << "Playback Video (" << flags::InputFile() << ")";
  std::unique_ptr<test::VideoRenderer> playback_video(
      test::VideoRenderer::Create(window_title.str().c_str(), 640, 480));
  FileRenderPassthrough file_passthrough(flags::OutBase(),
                                         playback_video.get());

  webrtc::RtcEventLogNullImpl event_log;
  std::unique_ptr<Call> call(Call::Create(Call::Config(&event_log)));

  test::NullTransport transport;
  VideoReceiveStream::Config receive_config(&transport);
  receive_config.rtp.remote_ssrc = flags::Ssrc();
  receive_config.rtp.local_ssrc = kReceiverLocalSsrc;
  receive_config.rtp.rtx_ssrc = flags::SsrcRtx();
  receive_config.rtp.rtx_associated_payload_types[flags::PayloadTypeRtx()] =
      flags::PayloadType();
  receive_config.rtp.ulpfec_payload_type = flags::FecPayloadType();
  receive_config.rtp.red_payload_type = flags::RedPayloadType();
  receive_config.rtp.nack.rtp_history_ms = 1000;
  if (flags::TransmissionOffsetId() != -1) {
    receive_config.rtp.extensions.push_back(RtpExtension(
        RtpExtension::kTimestampOffsetUri, flags::TransmissionOffsetId()));
  }
  if (flags::AbsSendTimeId() != -1) {
    receive_config.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kAbsSendTimeUri, flags::AbsSendTimeId()));
  }
  receive_config.renderer = &file_passthrough;

  VideoSendStream::Config::EncoderSettings encoder_settings;
  encoder_settings.payload_name = flags::Codec();
  encoder_settings.payload_type = flags::PayloadType();
  VideoReceiveStream::Decoder decoder;
  std::unique_ptr<DecoderBitstreamFileWriter> bitstream_writer;
  if (!flags::DecoderBitstreamFilename().empty()) {
    bitstream_writer.reset(new DecoderBitstreamFileWriter(
        flags::DecoderBitstreamFilename().c_str()));
    receive_config.pre_decode_callback = bitstream_writer.get();
  }
  decoder = test::CreateMatchingDecoder(encoder_settings);
  if (!flags::DecoderBitstreamFilename().empty()) {
    // Replace with a null decoder if we're writing the bitstream to a file
    // instead.
    delete decoder.decoder;
    decoder.decoder = new test::FakeNullDecoder();
  }
  receive_config.decoders.push_back(decoder);

  VideoReceiveStream* receive_stream =
      call->CreateVideoReceiveStream(std::move(receive_config));

  std::unique_ptr<test::RtpFileReader> rtp_reader(test::RtpFileReader::Create(
      test::RtpFileReader::kRtpDump, flags::InputFile()));
  if (!rtp_reader) {
    rtp_reader.reset(test::RtpFileReader::Create(test::RtpFileReader::kPcap,
                                                 flags::InputFile()));
    if (!rtp_reader) {
      fprintf(stderr,
              "Couldn't open input file as either a rtpdump or .pcap. Note "
              "that .pcapng is not supported.\nTrying to interpret the file as "
              "length/packet interleaved.\n");
      rtp_reader.reset(test::RtpFileReader::Create(
          test::RtpFileReader::kLengthPacketInterleaved, flags::InputFile()));
      if (!rtp_reader) {
        fprintf(stderr,
                "Unable to open input file with any supported format\n");
        return;
      }
    }
  }
  receive_stream->Start();

  uint32_t last_time_ms = 0;
  int num_packets = 0;
  std::map<uint32_t, int> unknown_packets;
  while (true) {
    test::RtpPacket packet;
    if (!rtp_reader->NextPacket(&packet))
      break;
    ++num_packets;
    switch (call->Receiver()->DeliverPacket(
        webrtc::MediaType::VIDEO, packet.data, packet.length, PacketTime())) {
      case PacketReceiver::DELIVERY_OK:
        break;
      case PacketReceiver::DELIVERY_UNKNOWN_SSRC: {
        RTPHeader header;
        std::unique_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
        parser->Parse(packet.data, packet.length, &header);
        if (unknown_packets[header.ssrc] == 0)
          fprintf(stderr, "Unknown SSRC: %u!\n", header.ssrc);
        ++unknown_packets[header.ssrc];
        break;
      }
      case PacketReceiver::DELIVERY_PACKET_ERROR: {
        fprintf(stderr, "Packet error, corrupt packets or incorrect setup?\n");
        RTPHeader header;
        std::unique_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
        parser->Parse(packet.data, packet.length, &header);
        fprintf(stderr, "Packet len=%zu pt=%u seq=%u ts=%u ssrc=0x%8x\n",
                packet.length, header.payloadType, header.sequenceNumber,
                header.timestamp, header.ssrc);
        break;
      }
    }
    if (last_time_ms != 0 && last_time_ms != packet.time_ms) {
      SleepMs(packet.time_ms - last_time_ms);
    }
    last_time_ms = packet.time_ms;
  }
  fprintf(stderr, "num_packets: %d\n", num_packets);

  for (std::map<uint32_t, int>::const_iterator it = unknown_packets.begin();
       it != unknown_packets.end();
       ++it) {
    fprintf(
        stderr, "Packets for unknown ssrc '%u': %d\n", it->first, it->second);
  }

  call->DestroyVideoReceiveStream(receive_stream);

  delete decoder.decoder;
}
}  // namespace webrtc

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  if (rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true)) {
    return 1;
  }
  if (webrtc::flags::FLAG_help) {
    rtc::FlagList::Print(nullptr, false);
    return 0;
  }

  RTC_CHECK(ValidatePayloadType(webrtc::flags::FLAG_payload_type));
  RTC_CHECK(ValidatePayloadType(webrtc::flags::FLAG_payload_type_rtx));
  RTC_CHECK(ValidateSsrc(webrtc::flags::FLAG_ssrc));
  RTC_CHECK(ValidateSsrc(webrtc::flags::FLAG_ssrc_rtx));
  RTC_CHECK(ValidateOptionalPayloadType(webrtc::flags::FLAG_red_payload_type));
  RTC_CHECK(ValidateOptionalPayloadType(webrtc::flags::FLAG_fec_payload_type));
  RTC_CHECK(ValidateRtpHeaderExtensionId(webrtc::flags::FLAG_abs_send_time_id));
  RTC_CHECK(ValidateRtpHeaderExtensionId(
      webrtc::flags::FLAG_transmission_offset_id));
  RTC_CHECK(ValidateInputFilenameNotEmpty(webrtc::flags::FLAG_input_file));

  webrtc::test::RunTest(webrtc::RtpReplay);
  return 0;
}
