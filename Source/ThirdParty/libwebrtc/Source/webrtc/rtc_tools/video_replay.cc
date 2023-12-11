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
#include "api/field_trials.h"
#include "api/media_types.h"
#include "api/rtc_event_log/rtc_event_log.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/test/video/function_video_decoder_factory.h"
#include "api/transport/field_trial_based_config.h"
#include "api/units/timestamp.h"
#include "api/video/video_codec_type.h"
#include "api/video_codecs/video_decoder.h"
#include "call/call.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "media/engine/internal_decoder_factory.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_util.h"
#include "modules/video_coding/utility/ivf_file_writer.h"
#include "rtc_base/checks.h"
#include "rtc_base/string_to_number.h"
#include "rtc_base/strings/json.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/sleep.h"
#include "test/call_config_utils.h"
#include "test/encoder_settings.h"
#include "test/fake_decoder.h"
#include "test/gtest.h"
#include "test/null_transport.h"
#include "test/rtp_file_reader.h"
#include "test/run_loop.h"
#include "test/run_test.h"
#include "test/test_video_capturer.h"
#include "test/testsupport/frame_writer.h"
#include "test/time_controller/simulated_time_controller.h"
#include "test/video_renderer.h"
#include "test/video_test_constants.h"

// Flag for payload type.
ABSL_FLAG(int,
          media_payload_type,
          webrtc::test::VideoTestConstants::kPayloadTypeVP8,
          "Media payload type");

// Flag for RED payload type.
ABSL_FLAG(int,
          red_payload_type,
          webrtc::test::VideoTestConstants::kRedPayloadType,
          "RED payload type");

// Flag for ULPFEC payload type.
ABSL_FLAG(int,
          ulpfec_payload_type,
          webrtc::test::VideoTestConstants::kUlpfecPayloadType,
          "ULPFEC payload type");

// Flag for FLEXFEC payload type.
ABSL_FLAG(int,
          flexfec_payload_type,
          webrtc::test::VideoTestConstants::kFlexfecPayloadType,
          "FLEXFEC payload type");

ABSL_FLAG(int,
          media_payload_type_rtx,
          webrtc::test::VideoTestConstants::kSendRtxPayloadType,
          "Media over RTX payload type");

ABSL_FLAG(int,
          red_payload_type_rtx,
          webrtc::test::VideoTestConstants::kRtxRedPayloadType,
          "RED over RTX payload type");

// Flag for SSRC and RTX SSRC.
ABSL_FLAG(uint32_t,
          ssrc,
          webrtc::test::VideoTestConstants::kVideoSendSsrcs[0],
          "Incoming SSRC");
ABSL_FLAG(uint32_t,
          ssrc_rtx,
          webrtc::test::VideoTestConstants::kSendRtxSsrcs[0],
          "Incoming RTX SSRC");

ABSL_FLAG(uint32_t,
          ssrc_flexfec,
          webrtc::test::VideoTestConstants::kFlexfecSendSsrc,
          "Incoming FLEXFEC SSRC");

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

ABSL_FLAG(std::string, decoder_ivf_filename, "", "Decoder ivf output file");

// Flag for video codec.
ABSL_FLAG(std::string, codec, "VP8", "Video codec");

// Flags for rtp start and stop timestamp.
ABSL_FLAG(uint32_t,
          start_timestamp,
          0,
          "RTP start timestamp, packets with smaller timestamp will be ignored "
          "(no wraparound)");
ABSL_FLAG(uint32_t,
          stop_timestamp,
          4294967295,
          "RTP stop timestamp, packets with larger timestamp will be ignored "
          "(no wraparound)");

// Flags for render window width and height
ABSL_FLAG(uint32_t, render_width, 640, "Width of render window");
ABSL_FLAG(uint32_t, render_height, 480, "Height of render window");

ABSL_FLAG(
    std::string,
    force_fieldtrials,
    "",
    "Field trials control experimental feature code which can be forced. "
    "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enabled/"
    " will assign the group Enable to field trial WebRTC-FooFeature. Multiple "
    "trials are separated by \"/\"");

ABSL_FLAG(bool, simulated_time, false, "Run in simulated time");

ABSL_FLAG(bool, disable_preview, false, "Disable decoded video preview.");

ABSL_FLAG(bool, disable_decoding, false, "Disable video decoding.");

ABSL_FLAG(int,
          extend_run_time_duration,
          0,
          "Extends the run time of the receiving client after the last RTP "
          "packet has been delivered. Typically useful to let the last few "
          "frames be decoded and rendered. Duration given in seconds.");

namespace {
bool ValidatePayloadType(int32_t payload_type) {
  return payload_type > 0 && payload_type <= 127;
}

bool ValidateOptionalPayloadType(int32_t payload_type) {
  return payload_type == -1 || ValidatePayloadType(payload_type);
}

bool ValidateRtpHeaderExtensionId(int32_t extension_id) {
  return extension_id >= -1 && extension_id < 15;
}

bool ValidateInputFilenameNotEmpty(const std::string& string) {
  return !string.empty();
}
}  // namespace

namespace webrtc {
namespace {

const uint32_t kReceiverLocalSsrc = 0x123456;

class NullRenderer : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  void OnFrame(const VideoFrame& frame) override {}
};

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

class DecoderIvfFileWriter : public test::FakeDecoder {
 public:
  explicit DecoderIvfFileWriter(const char* filename, const std::string& codec)
      : file_writer_(
            IvfFileWriter::Wrap(FileWrapper::OpenWriteOnly(filename), 0)) {
    RTC_DCHECK(file_writer_.get());
    if (codec == "VP8") {
      video_codec_type_ = VideoCodecType::kVideoCodecVP8;
    } else if (codec == "VP9") {
      video_codec_type_ = VideoCodecType::kVideoCodecVP9;
    } else if (codec == "H264") {
      video_codec_type_ = VideoCodecType::kVideoCodecH264;
    } else if (codec == "AV1") {
      video_codec_type_ = VideoCodecType::kVideoCodecAV1;
    } else if (codec == "H265") {
      video_codec_type_ = VideoCodecType::kVideoCodecH265;
    } else {
      RTC_LOG(LS_ERROR) << "Unsupported video codec " << codec;
      RTC_DCHECK_NOTREACHED();
    }
  }
  ~DecoderIvfFileWriter() override { file_writer_->Close(); }

  int32_t Decode(const EncodedImage& encoded_frame,
                 int64_t render_time_ms) override {
    if (!file_writer_->WriteFrame(encoded_frame, video_codec_type_)) {
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
    return WEBRTC_VIDEO_CODEC_OK;
  }

 private:
  std::unique_ptr<IvfFileWriter> file_writer_;
  VideoCodecType video_codec_type_;
};

// Holds all the shared memory structures required for a receive stream. This
// structure is used to prevent members being deallocated before the replay
// has been finished.
struct StreamState {
  test::NullTransport transport;
  std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>> sinks;
  std::vector<VideoReceiveStreamInterface*> receive_streams;
  std::vector<FlexfecReceiveStream*> flexfec_streams;
  std::unique_ptr<VideoDecoderFactory> decoder_factory;
};

// Loads multiple configurations from the provided configuration file.
std::unique_ptr<StreamState> ConfigureFromFile(const std::string& config_path,
                                               Call* call) {
  auto stream_state = std::make_unique<StreamState>();
  // Parse the configuration file.
  std::ifstream config_file(config_path);
  std::stringstream raw_json_buffer;
  raw_json_buffer << config_file.rdbuf();
  std::string raw_json = raw_json_buffer.str();
  Json::CharReaderBuilder builder;
  Json::Value json_configs;
  std::string error_message;
  std::unique_ptr<Json::CharReader> json_reader(builder.newCharReader());
  if (!json_reader->parse(raw_json.data(), raw_json.data() + raw_json.size(),
                          &json_configs, &error_message)) {
    fprintf(stderr, "Error parsing JSON config\n");
    fprintf(stderr, "%s\n", error_message.c_str());
    return nullptr;
  }

  if (absl::GetFlag(FLAGS_disable_decoding)) {
    stream_state->decoder_factory =
        std::make_unique<test::FunctionVideoDecoderFactory>(
            []() { return std::make_unique<test::FakeDecoder>(); });
  } else {
    stream_state->decoder_factory = std::make_unique<InternalDecoderFactory>();
  }
  size_t config_count = 0;
  for (const auto& json : json_configs) {
    // Create the configuration and parse the JSON into the config.
    auto receive_config =
        ParseVideoReceiveStreamJsonConfig(&(stream_state->transport), json);
    // Instantiate the underlying decoder.
    for (auto& decoder : receive_config.decoders) {
      decoder = test::CreateMatchingDecoder(decoder.payload_type,
                                            decoder.video_format.name);
    }
    // Create a window for this config.
    std::stringstream window_title;
    window_title << "Playback Video (" << config_count++ << ")";
    if (absl::GetFlag(FLAGS_disable_preview)) {
      stream_state->sinks.emplace_back(std::make_unique<NullRenderer>());
    } else {
      stream_state->sinks.emplace_back(test::VideoRenderer::Create(
          window_title.str().c_str(), absl::GetFlag(FLAGS_render_width),
          absl::GetFlag(FLAGS_render_height)));
    }
    // Create a receive stream for this config.
    receive_config.renderer = stream_state->sinks.back().get();
    receive_config.decoder_factory = stream_state->decoder_factory.get();
    stream_state->receive_streams.emplace_back(
        call->CreateVideoReceiveStream(std::move(receive_config)));
  }
  return stream_state;
}

// Loads the base configuration from flags passed in on the commandline.
std::unique_ptr<StreamState> ConfigureFromFlags(
    const std::string& rtp_dump_path,
    Call* call) {
  auto stream_state = std::make_unique<StreamState>();
  // Create the video renderers. We must add both to the stream state to keep
  // them from deallocating.
  std::stringstream window_title;
  window_title << "Playback Video (" << rtp_dump_path << ")";
  std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>> playback_video;
  if (absl::GetFlag(FLAGS_disable_preview)) {
    playback_video = std::make_unique<NullRenderer>();
  } else {
    playback_video.reset(test::VideoRenderer::Create(
        window_title.str().c_str(), absl::GetFlag(FLAGS_render_width),
        absl::GetFlag(FLAGS_render_height)));
  }
  auto file_passthrough = std::make_unique<FileRenderPassthrough>(
      absl::GetFlag(FLAGS_out_base), playback_video.get());
  stream_state->sinks.push_back(std::move(playback_video));
  stream_state->sinks.push_back(std::move(file_passthrough));
  // Setup the configuration from the flags.
  VideoReceiveStreamInterface::Config receive_config(
      &(stream_state->transport));
  receive_config.rtp.remote_ssrc = absl::GetFlag(FLAGS_ssrc);
  receive_config.rtp.local_ssrc = kReceiverLocalSsrc;
  receive_config.rtp.rtx_ssrc = absl::GetFlag(FLAGS_ssrc_rtx);
  receive_config.rtp.rtx_associated_payload_types[absl::GetFlag(
      FLAGS_media_payload_type_rtx)] = absl::GetFlag(FLAGS_media_payload_type);
  receive_config.rtp
      .rtx_associated_payload_types[absl::GetFlag(FLAGS_red_payload_type_rtx)] =
      absl::GetFlag(FLAGS_red_payload_type);
  receive_config.rtp.ulpfec_payload_type =
      absl::GetFlag(FLAGS_ulpfec_payload_type);
  receive_config.rtp.red_payload_type = absl::GetFlag(FLAGS_red_payload_type);
  receive_config.rtp.nack.rtp_history_ms = 1000;

  if (absl::GetFlag(FLAGS_flexfec_payload_type) != -1) {
    receive_config.rtp.protected_by_flexfec = true;
    FlexfecReceiveStream::Config flexfec_config(&(stream_state->transport));
    flexfec_config.payload_type = absl::GetFlag(FLAGS_flexfec_payload_type);
    flexfec_config.protected_media_ssrcs.push_back(absl::GetFlag(FLAGS_ssrc));
    flexfec_config.rtp.remote_ssrc = absl::GetFlag(FLAGS_ssrc_flexfec);
    FlexfecReceiveStream* flexfec_stream =
        call->CreateFlexfecReceiveStream(flexfec_config);
    receive_config.rtp.packet_sink_ = flexfec_stream;
    stream_state->flexfec_streams.push_back(flexfec_stream);
  }

  receive_config.renderer = stream_state->sinks.back().get();

  // Setup the receiving stream
  VideoReceiveStreamInterface::Decoder decoder;
  decoder = test::CreateMatchingDecoder(absl::GetFlag(FLAGS_media_payload_type),
                                        absl::GetFlag(FLAGS_codec));
  if (!absl::GetFlag(FLAGS_decoder_bitstream_filename).empty()) {
    // Replace decoder with file writer if we're writing the bitstream to a
    // file instead.
    stream_state->decoder_factory =
        std::make_unique<test::FunctionVideoDecoderFactory>([]() {
          return std::make_unique<DecoderBitstreamFileWriter>(
              absl::GetFlag(FLAGS_decoder_bitstream_filename).c_str());
        });
  } else if (!absl::GetFlag(FLAGS_decoder_ivf_filename).empty()) {
    // Replace decoder with file writer if we're writing the ivf to a
    // file instead.
    stream_state->decoder_factory =
        std::make_unique<test::FunctionVideoDecoderFactory>([]() {
          return std::make_unique<DecoderIvfFileWriter>(
              absl::GetFlag(FLAGS_decoder_ivf_filename).c_str(),
              absl::GetFlag(FLAGS_codec));
        });
  } else if (absl::GetFlag(FLAGS_disable_decoding)) {
    stream_state->decoder_factory =
        std::make_unique<test::FunctionVideoDecoderFactory>(
            []() { return std::make_unique<test::FakeDecoder>(); });
  } else {
    stream_state->decoder_factory = std::make_unique<InternalDecoderFactory>();
  }
  receive_config.decoder_factory = stream_state->decoder_factory.get();
  receive_config.decoders.push_back(decoder);

  stream_state->receive_streams.emplace_back(
      call->CreateVideoReceiveStream(std::move(receive_config)));
  return stream_state;
}

std::unique_ptr<test::RtpFileReader> CreateRtpReader(
    const std::string& rtp_dump_path) {
  std::unique_ptr<test::RtpFileReader> rtp_reader(test::RtpFileReader::Create(
      test::RtpFileReader::kRtpDump, rtp_dump_path));
  if (!rtp_reader) {
    rtp_reader.reset(
        test::RtpFileReader::Create(test::RtpFileReader::kPcap, rtp_dump_path));
    if (!rtp_reader) {
      fprintf(stderr,
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

// The RtpReplayer is responsible for parsing the configuration provided by
// the user, setting up the windows, receive streams and decoders and then
// replaying the provided RTP dump.
class RtpReplayer final {
 public:
  RtpReplayer(absl::string_view replay_config_path,
              absl::string_view rtp_dump_path,
              std::unique_ptr<FieldTrialsView> field_trials,
              bool simulated_time)
      : replay_config_path_(replay_config_path),
        rtp_dump_path_(rtp_dump_path),
        field_trials_(std::move(field_trials)),
        rtp_reader_(CreateRtpReader(rtp_dump_path_)) {
    TaskQueueFactory* task_queue_factory;
    if (simulated_time) {
      time_sim_ = std::make_unique<GlobalSimulatedTimeController>(
          Timestamp::Millis(1 << 30));
      task_queue_factory = time_sim_->GetTaskQueueFactory();
    } else {
      task_queue_factory_ = CreateDefaultTaskQueueFactory(field_trials_.get()),
      task_queue_factory = task_queue_factory_.get();
    }
    worker_thread_ =
        std::make_unique<rtc::TaskQueue>(task_queue_factory->CreateTaskQueue(
            "worker_thread", TaskQueueFactory::Priority::NORMAL));
    rtc::Event event;
    worker_thread_->PostTask([&]() {
      CallConfig call_config(&event_log_);
      call_config.trials = field_trials_.get();
      call_config.task_queue_factory = task_queue_factory;
      call_ = Call::Create(call_config);

      // Creation of the streams must happen inside a task queue because it is
      // resued as a worker thread.
      if (replay_config_path_.empty()) {
        stream_state_ = ConfigureFromFlags(rtp_dump_path_, call_.get());
      } else {
        stream_state_ = ConfigureFromFile(replay_config_path_, call_.get());
      }
      event.Set();
    });
    event.Wait(/*give_up_after=*/TimeDelta::Seconds(10));

    RTC_CHECK(stream_state_);
    RTC_CHECK(rtp_reader_);
  }

  ~RtpReplayer() {
    // Destruction of streams and the call must happen on the same thread as
    // their creation.
    rtc::Event event;
    worker_thread_->PostTask([&]() {
      for (const auto& receive_stream : stream_state_->receive_streams) {
        call_->DestroyVideoReceiveStream(receive_stream);
      }
      for (const auto& flexfec_stream : stream_state_->flexfec_streams) {
        call_->DestroyFlexfecReceiveStream(flexfec_stream);
      }
      call_.reset();
      event.Set();
    });
    event.Wait(/*give_up_after=*/TimeDelta::Seconds(10));
  }

  void Run() {
    rtc::Event event;
    worker_thread_->PostTask([&]() {
      // Start replaying the provided stream now that it has been configured.
      // VideoReceiveStreams must be started on the same thread as they were
      // created on.
      for (const auto& receive_stream : stream_state_->receive_streams) {
        receive_stream->Start();
      }
      event.Set();
    });
    event.Wait(/*give_up_after=*/TimeDelta::Seconds(10));

    ReplayPackets();
  }

 private:
  void ReplayPackets() {
    enum class Result { kOk, kUnknownSsrc, kParsingFailed };
    int64_t replay_start_ms = -1;
    int num_packets = 0;
    std::map<uint32_t, int> unknown_packets;
    rtc::Event event(/*manual_reset=*/false, /*initially_signalled=*/false);
    uint32_t start_timestamp = absl::GetFlag(FLAGS_start_timestamp);
    uint32_t stop_timestamp = absl::GetFlag(FLAGS_stop_timestamp);

    RtpHeaderExtensionMap extensions;
    if (absl::GetFlag(FLAGS_transmission_offset_id) != -1) {
      extensions.RegisterByUri(absl::GetFlag(FLAGS_transmission_offset_id),
                               RtpExtension::kTimestampOffsetUri);
    }
    if (absl::GetFlag(FLAGS_abs_send_time_id) != -1) {
      extensions.RegisterByUri(absl::GetFlag(FLAGS_abs_send_time_id),
                               RtpExtension::kAbsSendTimeUri);
    }

    while (true) {
      int64_t now_ms = CurrentTimeMs();
      if (replay_start_ms == -1) {
        replay_start_ms = now_ms;
      }

      test::RtpPacket packet;
      if (!rtp_reader_->NextPacket(&packet)) {
        break;
      }
      rtc::CopyOnWriteBuffer packet_buffer(
          packet.original_length > 0 ? packet.original_length : packet.length);
      memcpy(packet_buffer.MutableData(), packet.data, packet.length);
      if (packet.length < packet.original_length) {
        // Only the RTP header was recorded in the RTP dump, payload is not
        // known and and padding length is not known, zero the payload and
        // clear the padding bit.
        memset(packet_buffer.MutableData() + packet.length, 0,
               packet.original_length - packet.length);
        packet_buffer.MutableData()[0] &= ~0x20;
      }
      RtpPacket header;
      header.Parse(packet_buffer);
      if (header.Timestamp() < start_timestamp ||
          header.Timestamp() > stop_timestamp) {
        continue;
      }

      int64_t deliver_in_ms = replay_start_ms + packet.time_ms - now_ms;
      SleepOrAdvanceTime(deliver_in_ms);

      ++num_packets;

      Result result = Result::kOk;
      worker_thread_->PostTask([&]() {
        if (IsRtcpPacket(packet_buffer)) {
          call_->Receiver()->DeliverRtcpPacket(std::move(packet_buffer));
        }
        RtpPacketReceived received_packet(&extensions,
                                          Timestamp::Millis(CurrentTimeMs()));
        if (!received_packet.Parse(std::move(packet_buffer))) {
          result = Result::kParsingFailed;
          return;
        }
        call_->Receiver()->DeliverRtpPacket(
            MediaType::VIDEO, received_packet,
            [&result](const RtpPacketReceived& parsed_packet) -> bool {
              result = Result::kUnknownSsrc;
              // No point in trying to demux again.
              return false;
            });
        event.Set();
      });
      event.Wait(/*give_up_after=*/TimeDelta::Seconds(10));

      switch (result) {
        case Result::kOk:
          break;
        case Result::kUnknownSsrc: {
          if (unknown_packets[header.Ssrc()] == 0)
            fprintf(stderr, "Unknown SSRC: %u!\n", header.Ssrc());
          ++unknown_packets[header.Ssrc()];
          break;
        }
        case Result::kParsingFailed: {
          fprintf(stderr,
                  "Packet error, corrupt packets or incorrect setup?\n");
          fprintf(stderr, "Packet len=%zu pt=%u seq=%u ts=%u ssrc=0x%8x\n",
                  packet.length, header.PayloadType(), header.SequenceNumber(),
                  header.Timestamp(), header.Ssrc());
          break;
        }
      }
    }
    // Note that even when `extend_run_time_duration` is zero
    // `SleepOrAdvanceTime` should still be called in order to process the last
    // delivered packet when running in simulated time.
    SleepOrAdvanceTime(absl::GetFlag(FLAGS_extend_run_time_duration) * 1000);

    fprintf(stderr, "num_packets: %d\n", num_packets);

    for (std::map<uint32_t, int>::const_iterator it = unknown_packets.begin();
         it != unknown_packets.end(); ++it) {
      fprintf(stderr, "Packets for unknown ssrc '%u': %d\n", it->first,
              it->second);
    }
  }

  int64_t CurrentTimeMs() {
    return time_sim_ ? time_sim_->GetClock()->TimeInMilliseconds()
                     : rtc::TimeMillis();
  }

  void SleepOrAdvanceTime(int64_t duration_ms) {
    if (time_sim_) {
      time_sim_->AdvanceTime(TimeDelta::Millis(duration_ms));
    } else if (duration_ms > 0) {
      SleepMs(duration_ms);
    }
  }

  const std::string replay_config_path_;
  const std::string rtp_dump_path_;
  RtcEventLogNull event_log_;
  std::unique_ptr<FieldTrialsView> field_trials_;
  std::unique_ptr<GlobalSimulatedTimeController> time_sim_;
  std::unique_ptr<TaskQueueFactory> task_queue_factory_;
  std::unique_ptr<rtc::TaskQueue> worker_thread_;
  std::unique_ptr<Call> call_;
  std::unique_ptr<test::RtpFileReader> rtp_reader_;
  std::unique_ptr<StreamState> stream_state_;
};

void RtpReplay() {
  RtpReplayer replayer(
      absl::GetFlag(FLAGS_config_file), absl::GetFlag(FLAGS_input_file),
      std::make_unique<FieldTrials>(absl::GetFlag(FLAGS_force_fieldtrials)),
      absl::GetFlag(FLAGS_simulated_time));
  replayer.Run();
}

}  // namespace
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
  RTC_CHECK(
      ValidateOptionalPayloadType(absl::GetFlag(FLAGS_flexfec_payload_type)));
  RTC_CHECK(
      ValidateRtpHeaderExtensionId(absl::GetFlag(FLAGS_abs_send_time_id)));
  RTC_CHECK(ValidateRtpHeaderExtensionId(
      absl::GetFlag(FLAGS_transmission_offset_id)));
  RTC_CHECK(ValidateInputFilenameNotEmpty(absl::GetFlag(FLAGS_input_file)));
  RTC_CHECK_GE(absl::GetFlag(FLAGS_extend_run_time_duration), 0);

  rtc::ThreadManager::Instance()->WrapCurrentThread();
  webrtc::test::RunTest(webrtc::RtpReplay);
  return 0;
}
