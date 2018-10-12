/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/random.h"

#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/experiments/field_trial_units.h"
#include "test/field_trial.h"
#include "test/gtest.h"
#include "test/scenario/scenario.h"

namespace webrtc {
namespace test {
namespace {
constexpr int64_t kRunTimeMs = 60000;

using ::testing::Values;
using ::testing::Combine;
using ::testing::tuple;
using ::testing::make_tuple;

using Codec = VideoStreamConfig::Encoder::Codec;
using CodecImpl = VideoStreamConfig::Encoder::Implementation;

struct CallTestConfig {
  struct Scenario {
    FieldTrialParameter<int> random_seed;
    FieldTrialFlag return_traffic;
    FieldTrialParameter<DataRate> capacity;
    FieldTrialParameter<TimeDelta> propagation_delay;
    FieldTrialParameter<DataRate> cross_traffic;
    FieldTrialParameter<TimeDelta> delay_noise;
    FieldTrialParameter<double> loss_rate;
    Scenario()
        : random_seed("rs", 1),
          return_traffic("ret"),
          capacity("bw", DataRate::kbps(300)),
          propagation_delay("dl", TimeDelta::ms(100)),
          cross_traffic("ct", DataRate::Zero()),
          delay_noise("dn", TimeDelta::Zero()),
          loss_rate("pl", 0) {}
    void Parse(std::string config_str) {
      ParseFieldTrial(
          {&random_seed, &return_traffic, &capacity, &propagation_delay,
           &cross_traffic, &delay_noise, &loss_rate},
          config_str);
    }
  } scenario;
  struct Tuning {
    FieldTrialFlag use_bbr;
    FieldTrialFlag bbr_no_target_rate;
    FieldTrialOptional<DataSize> bbr_initial_window;
    FieldTrialParameter<double> bbr_encoder_gain;
    Tuning()
        : use_bbr("bbr"),
          bbr_no_target_rate("notr"),
          bbr_initial_window("iw", DataSize::bytes(8000)),
          bbr_encoder_gain("eg", 0.8) {}
    void Parse(std::string config_str) {
      ParseFieldTrial(
          {
              &use_bbr, &bbr_no_target_rate, &bbr_initial_window,
              &bbr_encoder_gain,
          },
          config_str);
    }
  } tuning;

  void Parse(std::string scenario_string, std::string tuning_string) {
    scenario.Parse(scenario_string);
    tuning.Parse(tuning_string);
    scenario_str = scenario_string;
    tuning_str = tuning_string;
  }
  std::string scenario_str;
  std::string tuning_str;

  std::string BbrTrial() const {
    char trial_buf[1024];
    rtc::SimpleStringBuilder trial(trial_buf);
    trial << "WebRTC-BweBbrConfig/";
    trial << "encoder_rate_gain_in_probe_rtt:0.5";
    trial.AppendFormat(",encoder_rate_gain:%.1lf",
                       tuning.bbr_encoder_gain.Get());
    if (tuning.bbr_no_target_rate)
      trial << ",pacing_rate_as_target:1";
    if (tuning.bbr_initial_window)
      trial << ",initial_cwin:" << tuning.bbr_initial_window->bytes();
    trial << "/";
    return trial.str();
  }
  std::string FieldTrials() const {
    std::string trials = "WebRTC-TaskQueueCongestionControl/Enabled/";
    if (tuning.use_bbr) {
      trials +=
          "WebRTC-BweCongestionController/Enabled,BBR/"
          "WebRTC-PacerPushbackExperiment/Enabled/"
          "WebRTC-Pacer-DrainQueue/Disabled/"
          "WebRTC-Pacer-PadInSilence/Enabled/"
          "WebRTC-Pacer-BlockAudio/Disabled/"
          "WebRTC-Audio-SendSideBwe/Enabled/"
          "WebRTC-SendSideBwe-WithOverhead/Enabled/";
      trials += BbrTrial();
    }
    return trials;
  }

  std::string Name() const {
    char raw_name[1024];
    rtc::SimpleStringBuilder name(raw_name);
    for (char c : scenario_str + "__tun__" + tuning_str) {
      if (c == ':') {
        continue;
      } else if (c == ',') {
        name << "_";
      } else if (c == '%') {
        name << "p";
      } else {
        name << c;
      }
    }
    return name.str();
  }
};
}  // namespace
class BbrScenarioTest
    : public ::testing::Test,
      public testing::WithParamInterface<tuple<std::string, std::string>> {
 public:
  BbrScenarioTest() {
    conf_.Parse(::testing::get<0>(GetParam()), ::testing::get<1>(GetParam()));
    field_trial_.reset(new test::ScopedFieldTrials(conf_.FieldTrials()));
  }
  CallTestConfig conf_;

 private:
  std::unique_ptr<test::ScopedFieldTrials> field_trial_;
};

TEST_P(BbrScenarioTest, ReceivesVideo) {
  Scenario s("bbr_test_gen/bbr__" + conf_.Name());

  CallClient* alice = s.CreateClient("send", [&](CallClientConfig* c) {
    if (conf_.tuning.use_bbr)
      c->transport.cc = TransportControllerConfig::CongestionController::kBbr;
    c->transport.state_log_interval = TimeDelta::ms(100);
    c->transport.rates.min_rate = DataRate::kbps(30);
    c->transport.rates.max_rate = DataRate::kbps(1800);
  });
  CallClient* bob = s.CreateClient("return", [&](CallClientConfig* c) {
    if (conf_.tuning.use_bbr && conf_.scenario.return_traffic)
      c->transport.cc = TransportControllerConfig::CongestionController::kBbr;
    c->transport.state_log_interval = TimeDelta::ms(100);
    c->transport.rates.min_rate = DataRate::kbps(30);
    c->transport.rates.max_rate = DataRate::kbps(1800);
  });
  NetworkNodeConfig net_conf;
  net_conf.simulation.bandwidth = conf_.scenario.capacity;
  net_conf.simulation.delay = conf_.scenario.propagation_delay;
  net_conf.simulation.loss_rate = conf_.scenario.loss_rate;
  net_conf.simulation.delay_std_dev = conf_.scenario.delay_noise;
  SimulationNode* send_net = s.CreateSimulationNode(net_conf);
  SimulationNode* ret_net = s.CreateSimulationNode(net_conf);
  VideoStreamPair* alice_video = s.CreateVideoStream(
      alice, {send_net}, bob, {ret_net}, [&](VideoStreamConfig* c) {
        c->encoder.fake.max_rate = DataRate::kbps(1800);
      });
  s.CreateAudioStream(alice, {send_net}, bob, {ret_net},
                      [&](AudioStreamConfig* c) {
                        if (conf_.tuning.use_bbr) {
                          c->stream.in_bandwidth_estimation = true;
                          c->encoder.fixed_rate = DataRate::kbps(31);
                        }
                      });

  VideoStreamPair* bob_video = nullptr;
  if (conf_.scenario.return_traffic) {
    bob_video = s.CreateVideoStream(
        bob, {ret_net}, alice, {send_net}, [&](VideoStreamConfig* c) {
          c->encoder.fake.max_rate = DataRate::kbps(1800);
        });
    s.CreateAudioStream(bob, {ret_net}, alice, {send_net},
                        [&](AudioStreamConfig* c) {
                          if (conf_.tuning.use_bbr) {
                            c->stream.in_bandwidth_estimation = true;
                            c->encoder.fixed_rate = DataRate::kbps(31);
                          }
                        });
  }
  CrossTrafficConfig cross_config;
  cross_config.peak_rate = conf_.scenario.cross_traffic;
  cross_config.random_seed = conf_.scenario.random_seed;
  CrossTrafficSource* cross_traffic =
      s.CreateCrossTraffic({send_net}, cross_config);

  s.CreatePrinter("send.stats.txt", TimeDelta::ms(100),
                  {alice->StatsPrinter(), alice_video->send()->StatsPrinter(),
                   cross_traffic->StatsPrinter(), send_net->ConfigPrinter()});

  std::vector<ColumnPrinter> return_printers{
      bob->StatsPrinter(), ColumnPrinter::Fixed("cross_traffic_rate", "0"),
      ret_net->ConfigPrinter()};
  if (bob_video)
    return_printers.push_back(bob_video->send()->StatsPrinter());
  s.CreatePrinter("return.stats.txt", TimeDelta::ms(100), return_printers);

  s.RunFor(TimeDelta::ms(kRunTimeMs));
}

INSTANTIATE_TEST_CASE_P(Selected,
                        BbrScenarioTest,
                        Values(make_tuple("rs:1,bw:150,dl:100,ct:100", "bbr")));

INSTANTIATE_TEST_CASE_P(
    OneWayTuning,
    BbrScenarioTest,
    Values(make_tuple("bw:150,dl:100", "bbr,iw:,eg:100%,notr"),
           make_tuple("bw:150,dl:100", "bbr,iw:8000,eg:100%,notr"),
           make_tuple("bw:150,dl:100", "bbr,iw:8000,eg:100%"),
           make_tuple("bw:150,dl:100", "bbr,iw:8000,eg:80%")));

INSTANTIATE_TEST_CASE_P(OneWayTuned,
                        BbrScenarioTest,
                        Values(make_tuple("bw:150,dl:100", "bbr"),
                               make_tuple("bw:150,dl:100", ""),
                               make_tuple("bw:800,dl:100", "bbr"),
                               make_tuple("bw:800,dl:100", "")));

INSTANTIATE_TEST_CASE_P(OneWayDegraded,
                        BbrScenarioTest,
                        Values(make_tuple("bw:150,dl:100,dn:30,pl:5%", "bbr"),
                               make_tuple("bw:150,dl:100,dn:30,pl:5%", ""),

                               make_tuple("bw:150,ct:100,dl:100", "bbr"),
                               make_tuple("bw:150,ct:100,dl:100", ""),

                               make_tuple("bw:800,dl:100,dn:30,pl:5%", "bbr"),
                               make_tuple("bw:800,dl:100,dn:30,pl:5%", ""),

                               make_tuple("bw:800,ct:600,dl:100", "bbr"),
                               make_tuple("bw:800,ct:600,dl:100", "")));

INSTANTIATE_TEST_CASE_P(TwoWay,
                        BbrScenarioTest,
                        Values(make_tuple("ret,bw:150,dl:100", "bbr"),
                               make_tuple("ret,bw:150,dl:100", ""),
                               make_tuple("ret,bw:800,dl:100", "bbr"),
                               make_tuple("ret,bw:800,dl:100", ""),
                               make_tuple("ret,bw:150,dl:50", "bbr"),
                               make_tuple("ret,bw:150,dl:50", "")));
}  // namespace test
}  // namespace webrtc
