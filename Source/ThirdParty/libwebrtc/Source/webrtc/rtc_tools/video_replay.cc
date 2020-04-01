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

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/test/video/function_video_decoder_factory.h"
#include "api/transport/field_trial_based_config.h"
#include "api/video_codecs/video_decoder.h"
#include "call/call.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "media/engine/internal_decoder_factory.h"
#include "rtc_base/checks.h"
#include "rtc_base/string_to_number.h"
#include "rtc_base/strings/json.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/sleep.h"
#include "test/call_config_utils.h"
#include "test/call_test.h"
#include "test/encoder_settings.h"
#include "test/fake_decoder.h"
#include "test/gtest.h"
#include "test/null_transport.h"
#include "test/rtp_file_reader.h"
#include "test/rtp_header_parser.h"
#include "test/run_loop.h"
#include "test/run_test.h"
#include "test/test_video_capturer.h"
#include "test/testsupport/frame_writer.h"
#include "test/video_renderer.h"

// Flag for payload type.
ABSL_FLAG(int,
          media_payload_type,
          webrtc::test::CallTest::kPayloadTypeVP8,
          "Media payload type");

// Flag for RED payload type.
ABSL_FLAG(int,
          red_payload_type,
          webrtc::test::CallTest::kRedPayloadType,
          "RED payload type");

// Flag for ULPFEC payload type.
ABSL_FLAG(int,
          ulpfec_payload_type,
          webrtc::test::CallTest::kUlpfecPayloadType,
          "ULPFEC payload type");

ABSL_FLAG(int,
          media_payload_type_rtx,
          webrtc::test::CallTest::kSendRtxPayloadType,
          "Media over RTX payload type");

ABSL_FLAG(int,
          red_payload_type_rtx,
          webrtc::test::CallTest::kRtxRedPayloadType,
          "RED over RTX payload type");

// Flag for SSRC.
const std::string& DefaultSsrc() {
  static const std::string ssrc =
      std::to_string(webrtc::test::CallTest::kVideoSendSsrcs[0]);
  return ssrc;
}
ABSL_FLAG(std::string, ssrc, DefaultSsrc().c_str(), "Incoming SSRC");

const std::string& DefaultSsrcRtx() {
  static const std::string ssrc_rtx =
      std::to_string(webrtc::test::CallTest::kSendRtxSsrcs[0]);
  return ssrc_rtx;
}
ABSL_FLAG(std::string, ssrc_rtx, DefaultSsrcRtx().c_str(), "Incoming RTX SSRC");

// Flag for abs-send-time id.
ABSL_FLAG(int, abs_send_time_id, -1, "RTP extension ID for abs-send-time");

// Flag for transmission-offset id.
ABSL_FLAG(int,
          transmission_offset_id,
          -1,
          "RTP extension ID for transmission-offset");

// Flag for rtpdump input file.
ABSL_FLAG(std::string, input_file, "", "input file");

ABSL_FLAG(std::string, config_file, "", "config file");

// Flag for raw output files.
ABSL_FLAG(std::string,
          out_base,
          "",
          "Basename (excluding .jpg) for raw output");

ABSL_FLAG(std::string,
          decoder_bitstream_filename,
          "",
          "Decoder bitstream output file");

// Flag for video codec.
ABSL_FLAG(std::string, codec, "VP8", "Video codec");

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

static int MediaPayloadType() {
  return absl::GetFlag(FLAGS_media_payload_type);
}

static int RedPayloadType() {
  return absl::GetFlag(FLAGS_red_payload_type);
}

static int UlpfecPayloadType() {
  return absl::GetFlag(FLAGS_ulpfec_payload_type);
}

static int MediaPayloadTypeRtx() {
  return absl::GetFlag(FLAGS_media_payload_type_rtx);
}

static int RedPayloadTypeRtx() {
  return absl::GetFlag(FLAGS_red_payload_type_rtx);
}

static uint32_t Ssrc() {
  return rtc::StringToNumber<uint32_t>(absl::GetFlag(FLAGS_ssrc)).value();
}

static uint32_t SsrcRtx() {
  return rtc::StringToNumber<uint32_t>(absl::GetFlag(FLAGS_ssrc_rtx)).value();
}

static int AbsSendTimeId() {
  return absl::GetFlag(FLAGS_abs_send_time_id);
}

static int TransmissionOffsetId() {
  return absl::GetFlag(FLAGS_transmission_offset_id);
}

static std::string InputFile() {
  return absl::GetFlag(FLAGS_input_file);
}

static std::string ConfigFile() {
  return absl::GetFlag(FLAGS_config_file);
}

static std::string OutBase() {
  return absl::GetFlag(FLAGS_out_base);
}

static std::string DecoderBitstreamFilename() {
  return absl::GetFlag(FLAGS_decoder_bitstream_filename);
}

static std::string Codec() {
  return absl::GetFlag(FLAGS_codec);
}

}  // namespace

namespace webrtc {

static const uint32_t kReceiverLocalSsrc = 0x123456;

class FileRenderPassthrough : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  FileRenderPassthrough(const std::string& basename,
                        rtc::VideoSinkInterface<VideoFrame>* renderer)
      : basename_(basename), renderer_(renderer), file_(nullptr), count_(0) {}

  ~FileRenderPassthrough() override {
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
  ~DecoderBitstreamFileWriter() override { fclose(file_); }

  int32_t Decode(const EncodedImage& encoded_frame,
                 bool /* missing_frames */,
                 int64_t /* render_time_ms */) override {
    if (fwrite(encoded_frame.data(), 1, encoded_frame.size(), file_) <
        encoded_frame.size()) {
      RTC_LOG_ERR(LS_ERROR) << "fwrite of encoded frame failed.";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    return WEBRTC_VIDEO_CODEC_OK;
  }

 private:
  FILE* file_;
};

// The RtpReplayer is responsible for parsing the configuration provided by the
// user, setting up the windows, recieve streams and decoders and then replaying
// the provided RTP dump.
class RtpReplayer final {
 public:
  // Replay a rtp dump with an optional json configuration.
  static void Replay(const std::string& replay_config_path,
                     const std::string& rtp_dump_path) {
    std::unique_ptr<webrtc::TaskQueueFactory> task_queue_factory =
        webrtc::CreateDefaultTaskQueueFactory();
    webrtc::RtcEventLogNull event_log;
    Call::Config call_config(&event_log);
    call_config.task_queue_factory = task_queue_factory.get();
    call_config.trials = new FieldTrialBasedConfig();
    std::unique_ptr<Call> call(Call::Create(call_config));
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
    auto stream_state = std::make_unique<StreamState>();
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

    stream_state->decoder_factory = std::make_unique<InternalDecoderFactory>();
    size_t config_count = 0;
    for (const auto& json : json_configs) {
      // Create the configuration and parse the JSON into the config.
      auto receive_config =
          ParseVideoReceiveStreamJsonConfig(&(stream_state->transport), json);
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
    auto stream_state = std::make_unique<StreamState>();
    // Create the video renderers. We must add both to the stream state to keep
    // them from deallocating.
    std::stringstream window_title;
    window_title << "Playback Video (" << rtp_dump_path << ")";
    std::unique_ptr<test::VideoRenderer> playback_video(
        test::VideoRenderer::Create(window_title.str().c_str(), 640, 480));
    auto file_passthrough = std::make_unique<FileRenderPassthrough>(
        OutBase(), playback_video.get());
    stream_state->sinks.push_back(std::move(playback_video));
    stream_state->sinks.push_back(std::move(file_passthrough));
    // Setup the configuration from the flags.
    VideoReceiveStream::Config receive_config(&(stream_state->transport));
    receive_config.rtp.remote_ssrc = Ssrc();
    receive_config.rtp.local_ssrc = kReceiverLocalSsrc;
    receive_config.rtp.rtx_ssrc = SsrcRtx();
    receive_config.rtp.rtx_associated_payload_types[MediaPayloadTypeRtx()] =
        MediaPayloadType();
    receive_config.rtp.rtx_associated_payload_types[RedPayloadTypeRtx()] =
        RedPayloadType();
    receive_config.rtp.ulpfec_payload_type = UlpfecPayloadType();
    receive_config.rtp.red_payload_type = RedPayloadType();
    receive_config.rtp.nack.rtp_history_ms = 1000;
    if (TransmissionOffsetId() != -1) {
      receive_config.rtp.extensions.push_back(RtpExtension(
          RtpExtension::kTimestampOffsetUri, TransmissionOffsetId()));
    }
    if (AbsSendTimeId() != -1) {
      receive_config.rtp.extensions.push_back(
          RtpExtension(RtpExtension::kAbsSendTimeUri, AbsSendTimeId()));
    }
    receive_config.renderer = stream_state->sinks.back().get();

    // Setup the receiving stream
    VideoReceiveStream::Decoder decoder;
    decoder = test::CreateMatchingDecoder(MediaPayloadType(), Codec());
    if (DecoderBitstreamFilename().empty()) {
      stream_state->decoder_factory =
          std::make_unique<InternalDecoderFactory>();
    } else {
      // Replace decoder with file writer if we're writing the bitstream to a
      // file instead.
      stream_state->decoder_factory =
          std::make_unique<test::FunctionVideoDecoderFactory>([]() {
            return std::make_unique<DecoderBitstreamFileWriter>(
                DecoderBitstreamFilename().c_str());
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
          std::unique_ptr<RtpHeaderParser> parser(
              RtpHeaderParser::CreateForTest());
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
          std::unique_ptr<RtpHeaderParser> parser(
              RtpHeaderParser::CreateForTest());
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
  RtpReplayer::Replay(ConfigFile(), InputFile());
}

}  // namespace webrtc

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  absl::ParseCommandLine(argc, argv);

  RTC_CHECK(ValidatePayloadType(absl::GetFlag(FLAGS_media_payload_type)));
  RTC_CHECK(ValidatePayloadType(absl::GetFlag(FLAGS_media_payload_type_rtx)));
  RTC_CHECK(ValidateOptionalPayloadType(absl::GetFlag(FLAGS_red_payload_type)));
  RTC_CHECK(
      ValidateOptionalPayloadType(absl::GetFlag(FLAGS_red_payload_type_rtx)));
  RTC_CHECK(
      ValidateOptionalPayloadType(absl::GetFlag(FLAGS_ulpfec_payload_type)));
  RTC_CHECK(ValidateSsrc(absl::GetFlag(FLAGS_ssrc).c_str()));
  RTC_CHECK(ValidateSsrc(absl::GetFlag(FLAGS_ssrc_rtx).c_str()));
  RTC_CHECK(
      ValidateRtpHeaderExtensionId(absl::GetFlag(FLAGS_abs_send_time_id)));
  RTC_CHECK(ValidateRtpHeaderExtensionId(
      absl::GetFlag(FLAGS_transmission_offset_id)));
  RTC_CHECK(ValidateInputFilenameNotEmpty(absl::GetFlag(FLAGS_input_file)));

  webrtc::test::RunTest(webrtc::RtpReplay);
  return 0;
}
