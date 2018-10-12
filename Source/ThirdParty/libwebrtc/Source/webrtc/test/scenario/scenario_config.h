/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef TEST_SCENARIO_SCENARIO_CONFIG_H_
#define TEST_SCENARIO_SCENARIO_CONFIG_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/rtpparameters.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/video_codecs/video_codec.h"
#include "test/frame_generator.h"

namespace webrtc {
namespace test {
struct PacketOverhead {
  static constexpr size_t kIpv4 = 20;
  static constexpr size_t kIpv6 = 40;
  static constexpr size_t kUdp = 8;
  static constexpr size_t kSrtp = 10;
  static constexpr size_t kTurn = 4;
  static constexpr size_t kDefault = kIpv4 + kUdp + kSrtp;
};
struct TransportControllerConfig {
  struct Rates {
    Rates();
    Rates(const Rates&);
    ~Rates();
    DataRate min_rate = DataRate::kbps(30);
    DataRate max_rate = DataRate::kbps(3000);
    DataRate start_rate = DataRate::kbps(300);
  } rates;
  enum CongestionController { kBbr, kGoogCc, kGoogCcFeedback } cc = kGoogCc;
  TimeDelta state_log_interval = TimeDelta::ms(100);
};

struct CallClientConfig {
  TransportControllerConfig transport;
  DataRate priority_target_rate = DataRate::Zero();
};

struct SimulatedTimeClientConfig {
  TransportControllerConfig transport;
  struct Feedback {
    TimeDelta interval = TimeDelta::ms(100);
  } feedback;
};

struct PacketStreamConfig {
  PacketStreamConfig();
  PacketStreamConfig(const PacketStreamConfig&);
  ~PacketStreamConfig();
  int frame_rate = 30;
  DataRate max_data_rate = DataRate::Infinity();
  DataSize max_packet_size = DataSize::bytes(1400);
  DataSize min_frame_size = DataSize::bytes(100);
  double keyframe_multiplier = 1;
  DataSize packet_overhead = DataSize::bytes(PacketOverhead::kDefault);
};

struct VideoStreamConfig {
  bool autostart = true;
  struct Source {
    enum Capture {
      kGenerator,
      kVideoFile,
      // Support for still images and explicit frame triggers should be added
      // here if needed.
    } capture = Capture::kGenerator;
    struct Generator {
      using PixelFormat = FrameGenerator::OutputType;
      PixelFormat pixel_format = PixelFormat::I420;
    } generator;
    struct VideoFile {
      std::string name;
    } video_file;
    int width = 320;
    int height = 180;
    int framerate = 30;
  } source;
  struct Encoder {
    Encoder();
    Encoder(const Encoder&);
    ~Encoder();
    enum Implementation { kFake, kSoftware, kHardware } implementation = kFake;
    struct Fake {
      DataRate max_rate = DataRate::Infinity();
    } fake;

    using Codec = VideoCodecType;
    Codec codec = Codec::kVideoCodecGeneric;
    bool denoising = true;
    absl::optional<int> key_frame_interval = 3000;

    absl::optional<DataRate> max_data_rate;
    size_t num_simulcast_streams = 1;
    using DegradationPreference = DegradationPreference;
    DegradationPreference degradation_preference =
        DegradationPreference::MAINTAIN_FRAMERATE;
  } encoder;
  struct Stream {
    Stream();
    Stream(const Stream&);
    ~Stream();
    bool packet_feedback = true;
    bool use_rtx = true;
    TimeDelta nack_history_time = TimeDelta::ms(1000);
    bool use_flexfec = false;
    bool use_ulpfec = false;
    DataSize packet_overhead = DataSize::bytes(PacketOverhead::kDefault);
  } stream;
  struct Renderer {
    enum Type { kFake } type = kFake;
  };
};

struct AudioStreamConfig {
  AudioStreamConfig();
  AudioStreamConfig(const AudioStreamConfig&);
  ~AudioStreamConfig();
  bool autostart = true;
  struct Source {
    int channels = 1;
  } source;
  struct Encoder {
    Encoder();
    Encoder(const Encoder&);
    ~Encoder();
    bool allocate_bitrate = false;
    absl::optional<DataRate> fixed_rate;
    absl::optional<DataRate> min_rate;
    absl::optional<DataRate> max_rate;
    TimeDelta initial_frame_length = TimeDelta::ms(20);
  } encoder;
  struct Stream {
    Stream();
    Stream(const Stream&);
    ~Stream();
    bool in_bandwidth_estimation = false;
    bool rate_allocation_priority = false;
    DataSize packet_overhead = DataSize::bytes(PacketOverhead::kDefault);
  } stream;
  struct Render {
    std::string sync_group;
  } render;
};

struct NetworkNodeConfig {
  NetworkNodeConfig();
  NetworkNodeConfig(const NetworkNodeConfig&);
  ~NetworkNodeConfig();
  enum class TrafficMode {
    kSimulation,
    kCustom
  } mode = TrafficMode::kSimulation;
  struct Simulation {
    Simulation();
    Simulation(const Simulation&);
    ~Simulation();
    DataRate bandwidth = DataRate::Infinity();
    TimeDelta delay = TimeDelta::Zero();
    TimeDelta delay_std_dev = TimeDelta::Zero();
    double loss_rate = 0;
  } simulation;
  DataSize packet_overhead = DataSize::Zero();
  TimeDelta update_frequency = TimeDelta::ms(1);
};

struct CrossTrafficConfig {
  CrossTrafficConfig();
  CrossTrafficConfig(const CrossTrafficConfig&);
  ~CrossTrafficConfig();
  enum Mode { kRandomWalk, kPulsedPeaks } mode = kRandomWalk;
  int random_seed = 1;
  DataRate peak_rate = DataRate::kbps(100);
  DataSize min_packet_size = DataSize::bytes(200);
  TimeDelta min_packet_interval = TimeDelta::ms(1);
  struct RandomWalk {
    TimeDelta update_interval = TimeDelta::ms(200);
    double variance = 0.6;
    double bias = -0.1;
  } random_walk;
  struct PulsedPeaks {
    TimeDelta send_duration = TimeDelta::ms(100);
    TimeDelta hold_duration = TimeDelta::ms(2000);
  } pulsed;
};
}  // namespace test
}  // namespace webrtc

#endif  // TEST_SCENARIO_SCENARIO_CONFIG_H_
