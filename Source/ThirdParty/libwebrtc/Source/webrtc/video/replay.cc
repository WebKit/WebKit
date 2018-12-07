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

#include <fstream>
#include <map>
#include <memory>
#include <sstream>

#include "api/test/video/function_video_decoder_factory.h"
#include "api/video_codecs/video_decoder.h"
#include "call/call.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "logging/rtc_event_log/rtc_event_log.h"
#include "media/engine/internaldecoderfactory.h"
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "rtc_base/checks.h"
#include "rtc_base/file.h"
#include "rtc_base/flags.h"
#include "rtc_base/string_to_number.h"
#include "rtc_base/strings/json.h"
#include "rtc_base/timeutils.h"
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
#include "test/test_video_capturer.h"
#include "test/testsupport/frame_writer.h"
#include "test/video_renderer.h"

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
WEBRTC_DEFINE_int(media_payload_type,
                  test::CallTest::kPayloadTypeVP8,
                  "Media payload type");
static int MediaPayloadType() {
  return static_cast<int>(FLAG_media_payload_type);
}

// Flag for RED payload type.
WEBRTC_DEFINE_int(red_payload_type,
                  test::CallTest::kRedPayloadType,
                  "RED payload type");
static int RedPayloadType() {
  return static_cast<int>(FLAG_red_payload_type);
}

// Flag for ULPFEC payload type.
WEBRTC_DEFINE_int(ulpfec_payload_type,
                  test::CallTest::kUlpfecPayloadType,
                  "ULPFEC payload type");
static int UlpfecPayloadType() {
  return static_cast<int>(FLAG_ulpfec_payload_type);
}

WEBRTC_DEFINE_int(media_payload_type_rtx,
                  test::CallTest::kSendRtxPayloadType,
                  "Media over RTX payload type");
static int MediaPayloadTypeRtx() {
  return static_cast<int>(FLAG_media_payload_type_rtx);
}

WEBRTC_DEFINE_int(red_payload_type_rtx,
                  test::CallTest::kRtxRedPayloadType,
                  "RED over RTX payload type");
static int RedPayloadTypeRtx() {
  return static_cast<int>(FLAG_red_payload_type_rtx);
}

// Flag for SSRC.
const std::string& DefaultSsrc() {
  static const std::string ssrc =
      std::to_string(test::CallTest::kVideoSendSsrcs[0]);
  return ssrc;
}
WEBRTC_DEFINE_string(ssrc, DefaultSsrc().c_str(), "Incoming SSRC");
static uint32_t Ssrc() {
  return rtc::StringToNumber<uint32_t>(FLAG_ssrc).value();
}

const std::string& DefaultSsrcRtx() {
  static const std::string ssrc_rtx =
      std::to_string(test::CallTest::kSendRtxSsrcs[0]);
  return ssrc_rtx;
}
WEBRTC_DEFINE_string(ssrc_rtx, DefaultSsrcRtx().c_str(), "Incoming RTX SSRC");
static uint32_t SsrcRtx() {
  return rtc::StringToNumber<uint32_t>(FLAG_ssrc_rtx).value();
}

// Flag for abs-send-time id.
WEBRTC_DEFINE_int(abs_send_time_id, -1, "RTP extension ID for abs-send-time");
static int AbsSendTimeId() {
  return static_cast<int>(FLAG_abs_send_time_id);
}

// Flag for transmission-offset id.
WEBRTC_DEFINE_int(transmission_offset_id,
                  -1,
                  "RTP extension ID for transmission-offset");
static int TransmissionOffsetId() {
  return static_cast<int>(FLAG_transmission_offset_id);
}

// Flag for rtpdump input file.
WEBRTC_DEFINE_string(input_file, "", "input file");
static std::string InputFile() {
  return static_cast<std::string>(FLAG_input_file);
}

WEBRTC_DEFINE_string(config_file, "", "config file");
static std::string ConfigFile() {
  return static_cast<std::string>(FLAG_config_file);
}

// Flag for raw output files.
WEBRTC_DEFINE_string(out_base, "", "Basename (excluding .jpg) for raw output");
static std::string OutBase() {
  return static_cast<std::string>(FLAG_out_base);
}

WEBRTC_DEFINE_string(decoder_bitstream_filename,
                     "",
                     "Decoder bitstream output file");
static std::string DecoderBitstreamFilename() {
  return static_cast<std::string>(FLAG_decoder_bitstream_filename);
}

// Flag for video codec.
WEBRTC_DEFINE_string(codec, "VP8", "Video codec");
static std::string Codec() {
  return static_cast<std::string>(FLAG_codec);
}

WEBRTC_DEFINE_bool(help, false, "Print this message.");
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

class DecoderBitstreamFileWriter : public test::FakeDecoder {
 public:
  explicit DecoderBitstreamFileWriter(const char* filename)
      : file_(fopen(filename, "wb")) {
    RTC_DCHECK(file_);
  }
  ~DecoderBitstreamFileWriter() { fclose(file_); }

  int32_t Decode(const EncodedImage& encoded_frame,
                      bool /* missing_frames */,
                      const CodecSpecificInfo* /* codec_specific_info */,
                      int64_t /* render_time_ms */) override {
    if (fwrite(encoded_frame._buffer, 1, encoded_frame._length, file_)
        < encoded_frame._length) {
      RTC_LOG_ERR(LS_ERROR) << "fwrite of encoded frame failed.";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    return WEBRTC_VIDEO_CODEC_OK;
  }

 private:
  FILE* file_;
};

// Deserializes a JSON representation of the VideoReceiveStream::Config back
// into a valid object. This will not initialize the decoders or the renderer.
class VideoReceiveStreamConfigDeserializer final {
 public:
  static VideoReceiveStream::Config Deserialize(webrtc::Transport* transport,
                                                const Json::Value& json) {
    auto receive_config = VideoReceiveStream::Config(transport);
    for (const auto decoder_json : json["decoders"]) {
      VideoReceiveStream::Decoder decoder;
      decoder.video_format =
          SdpVideoFormat(decoder_json["payload_name"].asString());
      decoder.payload_type = decoder_json["payload_type"].asInt64();
      for (const auto& params_json : decoder_json["codec_params"]) {
        std::vector<std::string> members = params_json.getMemberNames();
        RTC_CHECK_EQ(members.size(), 1);
        decoder.video_format.parameters[members[0]] =
            params_json[members[0]].asString();
      }
      receive_config.decoders.push_back(decoder);
    }
    receive_config.render_delay_ms = json["render_delay_ms"].asInt64();
    receive_config.target_delay_ms = json["target_delay_ms"].asInt64();
    receive_config.rtp.remote_ssrc = json["remote_ssrc"].asInt64();
    receive_config.rtp.local_ssrc = json["local_ssrc"].asInt64();
    receive_config.rtp.rtcp_mode =
        json["rtcp_mode"].asString() == "RtcpMode::kCompound"
            ? RtcpMode::kCompound
            : RtcpMode::kReducedSize;
    receive_config.rtp.remb = json["remb"].asBool();
    receive_config.rtp.transport_cc = json["transport_cc"].asBool();
    receive_config.rtp.nack.rtp_history_ms =
        json["nack"]["rtp_history_ms"].asInt64();
    receive_config.rtp.ulpfec_payload_type =
        json["ulpfec_payload_type"].asInt64();
    receive_config.rtp.red_payload_type = json["red_payload_type"].asInt64();
    receive_config.rtp.rtx_ssrc = json["rtx_ssrc"].asInt64();

    for (const auto& pl_json : json["rtx_payload_types"]) {
      std::vector<std::string> members = pl_json.getMemberNames();
      RTC_CHECK_EQ(members.size(), 1);
      Json::Value rtx_payload_type = pl_json[members[0]];
      receive_config.rtp.rtx_associated_payload_types[std::stoi(members[0])] =
          rtx_payload_type.asInt64();
    }
    for (const auto& ext_json : json["extensions"]) {
      receive_config.rtp.extensions.emplace_back(ext_json["uri"].asString(),
                                                 ext_json["id"].asInt64(),
                                                 ext_json["encrypt"].asBool());
    }
    return receive_config;
  }
};

// The RtpReplayer is responsible for parsing the configuration provided by the
// user, setting up the windows, recieve streams and decoders and then replaying
// the provided RTP dump.
class RtpReplayer final {
 public:
  // Replay a rtp dump with an optional json configuration.
  static void Replay(const std::string& replay_config_path,
                     const std::string& rtp_dump_path) {
    webrtc::RtcEventLogNullImpl event_log;
    Call::Config call_config(&event_log);
    std::unique_ptr<Call> call(Call::Create(std::move(call_config)));
    std::unique_ptr<StreamState> stream_state;
    // Attempt to load the configuration
    if (replay_config_path.empty()) {
      stream_state = ConfigureFromFlags(rtp_dump_path, call.get());
    } else {
      stream_state = ConfigureFromFile(replay_config_path, call.get());
    }
    if (stream_state == nullptr) {
      return;
    }
    // Attempt to create an RtpReader from the input file.
    std::unique_ptr<test::RtpFileReader> rtp_reader =
        CreateRtpReader(rtp_dump_path);
    if (rtp_reader == nullptr) {
      return;
    }
    // Start replaying the provided stream now that it has been configured.
    for (const auto& receive_stream : stream_state->receive_streams) {
      receive_stream->Start();
    }
    ReplayPackets(call.get(), rtp_reader.get());
    for (const auto& receive_stream : stream_state->receive_streams) {
      call->DestroyVideoReceiveStream(receive_stream);
    }
  }

 private:
  // Holds all the shared memory structures required for a recieve stream. This
  // structure is used to prevent members being deallocated before the replay
  // has been finished.
  struct StreamState {
    test::NullTransport transport;
    std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>> sinks;
    std::vector<VideoReceiveStream*> receive_streams;
    std::unique_ptr<VideoDecoderFactory> decoder_factory;
  };

  // Loads multiple configurations from the provided configuration file.
  static std::unique_ptr<StreamState> ConfigureFromFile(
      const std::string& config_path,
      Call* call) {
    auto stream_state = absl::make_unique<StreamState>();
    // Parse the configuration file.
    std::ifstream config_file(config_path);
    std::stringstream raw_json_buffer;
    raw_json_buffer << config_file.rdbuf();
    std::string raw_json = raw_json_buffer.str();
    Json::Reader json_reader;
    Json::Value json_configs;
    if (!json_reader.parse(raw_json, json_configs)) {
      fprintf(stderr, "Error parsing JSON config\n");
      fprintf(stderr, "%s\n", json_reader.getFormatedErrorMessages().c_str());
      return nullptr;
    }

    stream_state->decoder_factory = absl::make_unique<InternalDecoderFactory>();
    size_t config_count = 0;
    for (const auto& json : json_configs) {
      // Create the configuration and parse the JSON into the config.
      auto receive_config = VideoReceiveStreamConfigDeserializer::Deserialize(
          &(stream_state->transport), json);
      // Instantiate the underlying decoder.
      for (auto& decoder : receive_config.decoders) {
        decoder = test::CreateMatchingDecoder(decoder.payload_type,
                                              decoder.video_format.name);
        decoder.decoder_factory = stream_state->decoder_factory.get();
      }
      // Create a window for this config.
      std::stringstream window_title;
      window_title << "Playback Video (" << config_count++ << ")";
      stream_state->sinks.emplace_back(
          test::VideoRenderer::Create(window_title.str().c_str(), 640, 480));
      // Create a receive stream for this config.
      receive_config.renderer = stream_state->sinks.back().get();
      stream_state->receive_streams.emplace_back(
          call->CreateVideoReceiveStream(std::move(receive_config)));
    }
    return stream_state;
  }

  // Loads the base configuration from flags passed in on the commandline.
  static std::unique_ptr<StreamState> ConfigureFromFlags(
      const std::string& rtp_dump_path,
      Call* call) {
    auto stream_state = absl::make_unique<StreamState>();
    // Create the video renderers. We must add both to the stream state to keep
    // them from deallocating.
    std::stringstream window_title;
    window_title << "Playback Video (" << rtp_dump_path << ")";
    std::unique_ptr<test::VideoRenderer> playback_video(
        test::VideoRenderer::Create(window_title.str().c_str(), 640, 480));
    auto file_passthrough = absl::make_unique<FileRenderPassthrough>(
        flags::OutBase(), playback_video.get());
    stream_state->sinks.push_back(std::move(playback_video));
    stream_state->sinks.push_back(std::move(file_passthrough));
    // Setup the configuration from the flags.
    VideoReceiveStream::Config receive_config(&(stream_state->transport));
    receive_config.rtp.remote_ssrc = flags::Ssrc();
    receive_config.rtp.local_ssrc = kReceiverLocalSsrc;
    receive_config.rtp.rtx_ssrc = flags::SsrcRtx();
    receive_config.rtp
        .rtx_associated_payload_types[flags::MediaPayloadTypeRtx()] =
        flags::MediaPayloadType();
    receive_config.rtp
        .rtx_associated_payload_types[flags::RedPayloadTypeRtx()] =
        flags::RedPayloadType();
    receive_config.rtp.ulpfec_payload_type = flags::UlpfecPayloadType();
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
    receive_config.renderer = stream_state->sinks.back().get();

    // Setup the receiving stream
    VideoReceiveStream::Decoder decoder;
    decoder =
        test::CreateMatchingDecoder(flags::MediaPayloadType(), flags::Codec());
    if (flags::DecoderBitstreamFilename().empty()) {
      stream_state->decoder_factory =
          absl::make_unique<InternalDecoderFactory>();
    } else {
      // Replace decoder with file writer if we're writing the bitstream to a
      // file instead.
      stream_state->decoder_factory =
          absl::make_unique<test::FunctionVideoDecoderFactory>([]() {
            return absl::make_unique<DecoderBitstreamFileWriter>(
                flags::DecoderBitstreamFilename().c_str());
          });
    }
    decoder.decoder_factory = stream_state->decoder_factory.get();
    receive_config.decoders.push_back(decoder);

    stream_state->receive_streams.emplace_back(
        call->CreateVideoReceiveStream(std::move(receive_config)));
    return stream_state;
  }

  static std::unique_ptr<test::RtpFileReader> CreateRtpReader(
      const std::string& rtp_dump_path) {
    std::unique_ptr<test::RtpFileReader> rtp_reader(test::RtpFileReader::Create(
        test::RtpFileReader::kRtpDump, rtp_dump_path));
    if (!rtp_reader) {
      rtp_reader.reset(test::RtpFileReader::Create(test::RtpFileReader::kPcap,
                                                   rtp_dump_path));
      if (!rtp_reader) {
        fprintf(
            stderr,
            "Couldn't open input file as either a rtpdump or .pcap. Note "
            "that .pcapng is not supported.\nTrying to interpret the file as "
            "length/packet interleaved.\n");
        rtp_reader.reset(test::RtpFileReader::Create(
            test::RtpFileReader::kLengthPacketInterleaved, rtp_dump_path));
        if (!rtp_reader) {
          fprintf(stderr,
                  "Unable to open input file with any supported format\n");
          return nullptr;
        }
      }
    }
    return rtp_reader;
  }

  static void ReplayPackets(Call* call, test::RtpFileReader* rtp_reader) {
    int64_t replay_start_ms = -1;
    int num_packets = 0;
    std::map<uint32_t, int> unknown_packets;
    while (true) {
      int64_t now_ms = rtc::TimeMillis();
      if (replay_start_ms == -1) {
        replay_start_ms = now_ms;
      }

      test::RtpPacket packet;
      if (!rtp_reader->NextPacket(&packet)) {
        break;
      }

      int64_t deliver_in_ms = replay_start_ms + packet.time_ms - now_ms;
      if (deliver_in_ms > 0) {
        SleepMs(deliver_in_ms);
      }

      ++num_packets;
      switch (call->Receiver()->DeliverPacket(
          webrtc::MediaType::VIDEO,
          rtc::CopyOnWriteBuffer(packet.data, packet.length),
          /* packet_time_us */ -1)) {
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
          fprintf(stderr,
                  "Packet error, corrupt packets or incorrect setup?\n");
          RTPHeader header;
          std::unique_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
          parser->Parse(packet.data, packet.length, &header);
          fprintf(stderr, "Packet len=%zu pt=%u seq=%u ts=%u ssrc=0x%8x\n",
                  packet.length, header.payloadType, header.sequenceNumber,
                  header.timestamp, header.ssrc);
          break;
        }
      }
    }
    fprintf(stderr, "num_packets: %d\n", num_packets);

    for (std::map<uint32_t, int>::const_iterator it = unknown_packets.begin();
         it != unknown_packets.end(); ++it) {
      fprintf(stderr, "Packets for unknown ssrc '%u': %d\n", it->first,
              it->second);
    }
  }
};  // class RtpReplayer

void RtpReplay() {
  RtpReplayer::Replay(flags::ConfigFile(), flags::InputFile());
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

  RTC_CHECK(ValidatePayloadType(webrtc::flags::FLAG_media_payload_type));
  RTC_CHECK(ValidatePayloadType(webrtc::flags::FLAG_media_payload_type_rtx));
  RTC_CHECK(ValidateOptionalPayloadType(webrtc::flags::FLAG_red_payload_type));
  RTC_CHECK(
      ValidateOptionalPayloadType(webrtc::flags::FLAG_red_payload_type_rtx));
  RTC_CHECK(
      ValidateOptionalPayloadType(webrtc::flags::FLAG_ulpfec_payload_type));
  RTC_CHECK(ValidateSsrc(webrtc::flags::FLAG_ssrc));
  RTC_CHECK(ValidateSsrc(webrtc::flags::FLAG_ssrc_rtx));
  RTC_CHECK(ValidateRtpHeaderExtensionId(webrtc::flags::FLAG_abs_send_time_id));
  RTC_CHECK(
      ValidateRtpHeaderExtensionId(webrtc::flags::FLAG_transmission_offset_id));
  RTC_CHECK(ValidateInputFilenameNotEmpty(webrtc::flags::FLAG_input_file));

  webrtc::test::RunTest(webrtc::RtpReplay);
  return 0;
}
