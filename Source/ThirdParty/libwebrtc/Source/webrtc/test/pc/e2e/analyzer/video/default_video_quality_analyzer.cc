/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "api/array_view.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/units/time_delta.h"
#include "api/video/i420_buffer.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/cpu_time.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/time_utils.h"
#include "rtc_tools/frame_analyzer/video_geometry_aligner.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

constexpr int kMaxActiveComparisons = 10;
constexpr int kFreezeThresholdMs = 150;
constexpr int kMicrosPerSecond = 1000000;
constexpr int kBitsInByte = 8;

void LogFrameCounters(const std::string& name, const FrameCounters& counters) {
  RTC_LOG(INFO) << "[" << name << "] Captured    : " << counters.captured;
  RTC_LOG(INFO) << "[" << name << "] Pre encoded : " << counters.pre_encoded;
  RTC_LOG(INFO) << "[" << name << "] Encoded     : " << counters.encoded;
  RTC_LOG(INFO) << "[" << name << "] Received    : " << counters.received;
  RTC_LOG(INFO) << "[" << name << "] Decoded     : " << counters.decoded;
  RTC_LOG(INFO) << "[" << name << "] Rendered    : " << counters.rendered;
  RTC_LOG(INFO) << "[" << name << "] Dropped     : " << counters.dropped;
}

void LogStreamInternalStats(const std::string& name, const StreamStats& stats) {
  RTC_LOG(INFO) << "[" << name
                << "] Dropped by encoder     : " << stats.dropped_by_encoder;
  RTC_LOG(INFO) << "[" << name << "] Dropped before encoder : "
                << stats.dropped_before_encoder;
}

template <typename T>
absl::optional<T> MaybeGetValue(const std::map<size_t, T>& map, size_t key) {
  auto it = map.find(key);
  if (it == map.end()) {
    return absl::nullopt;
  }
  return it->second;
}

SamplesStatsCounter::StatsSample StatsSample(double value,
                                             Timestamp sampling_time) {
  return SamplesStatsCounter::StatsSample{value, sampling_time};
}

}  // namespace

void RateCounter::AddEvent(Timestamp event_time) {
  if (event_first_time_.IsMinusInfinity()) {
    event_first_time_ = event_time;
  }
  event_last_time_ = event_time;
  event_count_++;
}

double RateCounter::GetEventsPerSecond() const {
  RTC_DCHECK(!IsEmpty());
  // Divide on us and multiply on kMicrosPerSecond to correctly process cases
  // where there were too small amount of events, so difference is less then 1
  // sec. We can use us here, because Timestamp has us resolution.
  return static_cast<double>(event_count_) /
         (event_last_time_ - event_first_time_).us() * kMicrosPerSecond;
}

std::string StatsKey::ToString() const {
  rtc::StringBuilder out;
  out << stream_label << "_" << sender << "_" << receiver;
  return out.str();
}

bool operator<(const StatsKey& a, const StatsKey& b) {
  if (a.stream_label != b.stream_label) {
    return a.stream_label < b.stream_label;
  }
  if (a.sender != b.sender) {
    return a.sender < b.sender;
  }
  return a.receiver < b.receiver;
}

bool operator==(const StatsKey& a, const StatsKey& b) {
  return a.stream_label == b.stream_label && a.sender == b.sender &&
         a.receiver == b.receiver;
}

std::string InternalStatsKey::ToString() const {
  rtc::StringBuilder out;
  out << "stream=" << stream << "_sender=" << sender
      << "_receiver=" << receiver;
  return out.str();
}

bool operator<(const InternalStatsKey& a, const InternalStatsKey& b) {
  if (a.stream != b.stream) {
    return a.stream < b.stream;
  }
  if (a.sender != b.sender) {
    return a.sender < b.sender;
  }
  return a.receiver < b.receiver;
}

bool operator==(const InternalStatsKey& a, const InternalStatsKey& b) {
  return a.stream == b.stream && a.sender == b.sender &&
         a.receiver == b.receiver;
}

DefaultVideoQualityAnalyzer::DefaultVideoQualityAnalyzer(
    webrtc::Clock* clock,
    DefaultVideoQualityAnalyzerOptions options)
    : options_(options), clock_(clock) {}
DefaultVideoQualityAnalyzer::~DefaultVideoQualityAnalyzer() {
  Stop();
}

void DefaultVideoQualityAnalyzer::Start(
    std::string test_case_name,
    rtc::ArrayView<const std::string> peer_names,
    int max_threads_count) {
  test_label_ = std::move(test_case_name);
  for (int i = 0; i < max_threads_count; i++) {
    thread_pool_.push_back(rtc::PlatformThread::SpawnJoinable(
        [this] { ProcessComparisons(); },
        "DefaultVideoQualityAnalyzerWorker-" + std::to_string(i)));
  }
  {
    MutexLock lock(&lock_);
    peers_ = std::make_unique<NamesCollection>(peer_names);
    RTC_CHECK(start_time_.IsMinusInfinity());

    state_ = State::kActive;
    start_time_ = Now();
  }
  StartMeasuringCpuProcessTime();
}

uint16_t DefaultVideoQualityAnalyzer::OnFrameCaptured(
    absl::string_view peer_name,
    const std::string& stream_label,
    const webrtc::VideoFrame& frame) {
  // |next_frame_id| is atomic, so we needn't lock here.
  uint16_t frame_id = next_frame_id_++;
  Timestamp start_time = Timestamp::MinusInfinity();
  size_t peer_index = -1;
  size_t peers_count = -1;
  size_t stream_index;
  {
    MutexLock lock(&lock_);
    // Create a local copy of |start_time_|, peer's index and total peers count
    // to access it under |comparison_lock_| without holding a |lock_|
    start_time = start_time_;
    peer_index = peers_->index(peer_name);
    peers_count = peers_->size();
    stream_index = streams_.AddIfAbsent(stream_label);
  }
  {
    // Ensure stats for this stream exists.
    MutexLock lock(&comparison_lock_);
    for (size_t i = 0; i < peers_count; ++i) {
      if (i == peer_index) {
        continue;
      }
      InternalStatsKey stats_key(stream_index, peer_index, i);
      if (stream_stats_.find(stats_key) == stream_stats_.end()) {
        stream_stats_.insert({stats_key, StreamStats()});
        // Assume that the first freeze was before first stream frame captured.
        // This way time before the first freeze would be counted as time
        // between freezes.
        stream_last_freeze_end_time_.insert({stats_key, start_time});
      } else {
        // When we see some |stream_label| for the first time we need to create
        // stream stats object for it and set up some states, but we need to do
        // it only once and for all receivers, so on the next frame on the same
        // |stream_label| we can be sure, that it's already done and we needn't
        // to scan though all peers again.
        break;
      }
    }
  }
  {
    MutexLock lock(&lock_);
    stream_to_sender_[stream_index] = peer_index;
    frame_counters_.captured++;
    for (size_t i = 0; i < peers_->size(); ++i) {
      if (i != peer_index) {
        InternalStatsKey key(stream_index, peer_index, i);
        stream_frame_counters_[key].captured++;
      }
    }

    auto state_it = stream_states_.find(stream_index);
    if (state_it == stream_states_.end()) {
      stream_states_.emplace(stream_index,
                             StreamState(peer_index, peers_->size()));
    }
    StreamState* state = &stream_states_.at(stream_index);
    state->PushBack(frame_id);
    // Update frames in flight info.
    auto it = captured_frames_in_flight_.find(frame_id);
    if (it != captured_frames_in_flight_.end()) {
      // If we overflow uint16_t and hit previous frame id and this frame is
      // still in flight, it means that this stream wasn't rendered for long
      // time and we need to process existing frame as dropped.
      for (size_t i = 0; i < peers_->size(); ++i) {
        if (i == peer_index) {
          continue;
        }

        uint16_t oldest_frame_id = state->PopFront(i);
        RTC_DCHECK_EQ(frame_id, oldest_frame_id);
        frame_counters_.dropped++;
        InternalStatsKey key(stream_index, peer_index, i);
        stream_frame_counters_.at(key).dropped++;

        MutexLock lock1(&comparison_lock_);
        analyzer_stats_.frames_in_flight_left_count.AddSample(
            StatsSample(captured_frames_in_flight_.size(), Now()));
        AddComparison(InternalStatsKey(stream_index, peer_index, i),
                      it->second.frame(), absl::nullopt, true,
                      it->second.GetStatsForPeer(i));
      }

      captured_frames_in_flight_.erase(it);
    }
    captured_frames_in_flight_.emplace(
        frame_id,
        FrameInFlight(stream_index, frame,
                      /*captured_time=*/Now(), peer_index, peers_->size()));
    // Set frame id on local copy of the frame
    captured_frames_in_flight_.at(frame_id).SetFrameId(frame_id);

    // Update history stream<->frame mapping
    for (auto it = stream_to_frame_id_history_.begin();
         it != stream_to_frame_id_history_.end(); ++it) {
      it->second.erase(frame_id);
    }
    stream_to_frame_id_history_[stream_index].insert(frame_id);

    // If state has too many frames that are in flight => remove the oldest
    // queued frame in order to avoid to use too much memory.
    if (state->GetAliveFramesCount() >
        options_.max_frames_in_flight_per_stream_count) {
      uint16_t frame_id_to_remove = state->MarkNextAliveFrameAsDead();
      auto it = captured_frames_in_flight_.find(frame_id_to_remove);
      RTC_CHECK(it != captured_frames_in_flight_.end())
          << "Frame with ID " << frame_id_to_remove
          << " is expected to be in flight, but hasn't been found in "
          << "|captured_frames_in_flight_|";
      bool is_removed = it->second.RemoveFrame();
      RTC_DCHECK(is_removed)
          << "Invalid stream state: alive frame is removed already";
    }
  }
  return frame_id;
}

void DefaultVideoQualityAnalyzer::OnFramePreEncode(
    absl::string_view peer_name,
    const webrtc::VideoFrame& frame) {
  MutexLock lock(&lock_);
  auto it = captured_frames_in_flight_.find(frame.id());
  RTC_DCHECK(it != captured_frames_in_flight_.end())
      << "Frame id=" << frame.id() << " not found";
  frame_counters_.pre_encoded++;
  size_t peer_index = peers_->index(peer_name);
  for (size_t i = 0; i < peers_->size(); ++i) {
    if (i != peer_index) {
      InternalStatsKey key(it->second.stream(), peer_index, i);
      stream_frame_counters_.at(key).pre_encoded++;
    }
  }
  it->second.SetPreEncodeTime(Now());
}

void DefaultVideoQualityAnalyzer::OnFrameEncoded(
    absl::string_view peer_name,
    uint16_t frame_id,
    const webrtc::EncodedImage& encoded_image,
    const EncoderStats& stats) {
  MutexLock lock(&lock_);
  auto it = captured_frames_in_flight_.find(frame_id);
  RTC_DCHECK(it != captured_frames_in_flight_.end());
  // For SVC we can receive multiple encoded images for one frame, so to cover
  // all cases we have to pick the last encode time.
  if (!it->second.HasEncodedTime()) {
    // Increase counters only when we meet this frame first time.
    frame_counters_.encoded++;
    size_t peer_index = peers_->index(peer_name);
    for (size_t i = 0; i < peers_->size(); ++i) {
      if (i != peer_index) {
        InternalStatsKey key(it->second.stream(), peer_index, i);
        stream_frame_counters_.at(key).encoded++;
      }
    }
  }
  it->second.OnFrameEncoded(Now(), encoded_image.size(),
                            stats.target_encode_bitrate);
}

void DefaultVideoQualityAnalyzer::OnFrameDropped(
    absl::string_view peer_name,
    webrtc::EncodedImageCallback::DropReason reason) {
  // Here we do nothing, because we will see this drop on renderer side.
}

void DefaultVideoQualityAnalyzer::OnFramePreDecode(
    absl::string_view peer_name,
    uint16_t frame_id,
    const webrtc::EncodedImage& input_image) {
  MutexLock lock(&lock_);
  size_t peer_index = peers_->index(peer_name);

  auto it = captured_frames_in_flight_.find(frame_id);
  if (it == captured_frames_in_flight_.end() ||
      it->second.HasReceivedTime(peer_index)) {
    // It means this frame was predecoded before, so we can skip it. It may
    // happen when we have multiple simulcast streams in one track and received
    // the same picture from two different streams because SFU can't reliably
    // correlate two simulcast streams and started relaying the second stream
    // from the same frame it has relayed right before for the first stream.
    return;
  }

  frame_counters_.received++;
  InternalStatsKey key(it->second.stream(),
                       stream_to_sender_.at(it->second.stream()), peer_index);
  stream_frame_counters_.at(key).received++;
  // Determine the time of the last received packet of this video frame.
  RTC_DCHECK(!input_image.PacketInfos().empty());
  Timestamp last_receive_time =
      std::max_element(input_image.PacketInfos().cbegin(),
                       input_image.PacketInfos().cend(),
                       [](const RtpPacketInfo& a, const RtpPacketInfo& b) {
                         return a.receive_time() < b.receive_time();
                       })
          ->receive_time();
  it->second.OnFramePreDecode(peer_index,
                              /*received_time=*/last_receive_time,
                              /*decode_start_time=*/Now());
}

void DefaultVideoQualityAnalyzer::OnFrameDecoded(
    absl::string_view peer_name,
    const webrtc::VideoFrame& frame,
    const DecoderStats& stats) {
  MutexLock lock(&lock_);
  size_t peer_index = peers_->index(peer_name);

  auto it = captured_frames_in_flight_.find(frame.id());
  if (it == captured_frames_in_flight_.end() ||
      it->second.HasDecodeEndTime(peer_index)) {
    // It means this frame was decoded before, so we can skip it. It may happen
    // when we have multiple simulcast streams in one track and received
    // the same picture from two different streams because SFU can't reliably
    // correlate two simulcast streams and started relaying the second stream
    // from the same frame it has relayed right before for the first stream.
    return;
  }
  frame_counters_.decoded++;
  InternalStatsKey key(it->second.stream(),
                       stream_to_sender_.at(it->second.stream()), peer_index);
  stream_frame_counters_.at(key).decoded++;
  it->second.SetDecodeEndTime(peer_index, Now());
}

void DefaultVideoQualityAnalyzer::OnFrameRendered(
    absl::string_view peer_name,
    const webrtc::VideoFrame& frame) {
  MutexLock lock(&lock_);
  size_t peer_index = peers_->index(peer_name);

  auto frame_it = captured_frames_in_flight_.find(frame.id());
  if (frame_it == captured_frames_in_flight_.end() ||
      frame_it->second.HasRenderedTime(peer_index)) {
    // It means this frame was rendered before, so we can skip it. It may happen
    // when we have multiple simulcast streams in one track and received
    // the same picture from two different streams because SFU can't reliably
    // correlate two simulcast streams and started relaying the second stream
    // from the same frame it has relayed right before for the first stream.
    return;
  }

  // Find corresponding captured frame.
  FrameInFlight* frame_in_flight = &frame_it->second;
  absl::optional<VideoFrame> captured_frame = frame_in_flight->frame();

  const size_t stream_index = frame_in_flight->stream();
  StreamState* state = &stream_states_.at(stream_index);
  const InternalStatsKey stats_key(stream_index, state->owner(), peer_index);

  // Update frames counters.
  frame_counters_.rendered++;
  stream_frame_counters_.at(stats_key).rendered++;

  // Update current frame stats.
  frame_in_flight->OnFrameRendered(peer_index, Now(), frame.width(),
                                   frame.height());

  // After we received frame here we need to check if there are any dropped
  // frames between this one and last one, that was rendered for this video
  // stream.
  int dropped_count = 0;
  while (!state->IsEmpty(peer_index) &&
         state->Front(peer_index) != frame.id()) {
    dropped_count++;
    uint16_t dropped_frame_id = state->PopFront(peer_index);
    // Frame with id |dropped_frame_id| was dropped. We need:
    // 1. Update global and stream frame counters
    // 2. Extract corresponding frame from |captured_frames_in_flight_|
    // 3. Send extracted frame to comparison with dropped=true
    // 4. Cleanup dropped frame
    frame_counters_.dropped++;
    stream_frame_counters_.at(stats_key).dropped++;

    auto dropped_frame_it = captured_frames_in_flight_.find(dropped_frame_id);
    RTC_DCHECK(dropped_frame_it != captured_frames_in_flight_.end());
    absl::optional<VideoFrame> dropped_frame = dropped_frame_it->second.frame();
    dropped_frame_it->second.MarkDropped(peer_index);

    {
      MutexLock lock1(&comparison_lock_);
      analyzer_stats_.frames_in_flight_left_count.AddSample(
          StatsSample(captured_frames_in_flight_.size(), Now()));
      AddComparison(stats_key, dropped_frame, absl::nullopt, true,
                    dropped_frame_it->second.GetStatsForPeer(peer_index));
    }

    if (dropped_frame_it->second.HaveAllPeersReceived()) {
      captured_frames_in_flight_.erase(dropped_frame_it);
    }
  }
  RTC_DCHECK(!state->IsEmpty(peer_index));
  state->PopFront(peer_index);

  if (state->last_rendered_frame_time(peer_index)) {
    frame_in_flight->SetPrevFrameRenderedTime(
        peer_index, state->last_rendered_frame_time(peer_index).value());
  }
  state->SetLastRenderedFrameTime(peer_index,
                                  frame_in_flight->rendered_time(peer_index));
  {
    MutexLock cr(&comparison_lock_);
    stream_stats_.at(stats_key).skipped_between_rendered.AddSample(
        StatsSample(dropped_count, Now()));
  }

  {
    MutexLock lock(&comparison_lock_);
    analyzer_stats_.frames_in_flight_left_count.AddSample(
        StatsSample(captured_frames_in_flight_.size(), Now()));
    AddComparison(stats_key, captured_frame, frame, false,
                  frame_in_flight->GetStatsForPeer(peer_index));
  }

  if (frame_it->second.HaveAllPeersReceived()) {
    captured_frames_in_flight_.erase(frame_it);
  }
}

void DefaultVideoQualityAnalyzer::OnEncoderError(
    absl::string_view peer_name,
    const webrtc::VideoFrame& frame,
    int32_t error_code) {
  RTC_LOG(LS_ERROR) << "Encoder error for frame.id=" << frame.id()
                    << ", code=" << error_code;
}

void DefaultVideoQualityAnalyzer::OnDecoderError(absl::string_view peer_name,
                                                 uint16_t frame_id,
                                                 int32_t error_code) {
  RTC_LOG(LS_ERROR) << "Decoder error for frame_id=" << frame_id
                    << ", code=" << error_code;
}

void DefaultVideoQualityAnalyzer::RegisterParticipantInCall(
    absl::string_view peer_name) {
  MutexLock lock1(&lock_);
  MutexLock lock2(&comparison_lock_);
  RTC_CHECK(!peers_->HasName(peer_name));
  peers_->AddIfAbsent(peer_name);

  // Ensure stats for receiving (for frames from other peers to this one)
  // streams exists. Since in flight frames will be sent to the new peer
  // as well. Sending stats (from this peer to others) will be added by
  // DefaultVideoQualityAnalyzer::OnFrameCaptured.
  for (auto& key_val : stream_to_sender_) {
    InternalStatsKey key(key_val.first, key_val.second,
                         peers_->index(peer_name));
    const int64_t frames_count = captured_frames_in_flight_.size();
    FrameCounters counters;
    counters.captured = frames_count;
    counters.pre_encoded = frames_count;
    counters.encoded = frames_count;
    stream_frame_counters_.insert({key, std::move(counters)});

    stream_stats_.insert({key, StreamStats()});
    stream_last_freeze_end_time_.insert({key, start_time_});
  }
  // Ensure, that frames states are handled correctly
  // (e.g. dropped frames tracking).
  for (auto& key_val : stream_states_) {
    key_val.second.AddPeer();
  }
  // Register new peer for every frame in flight.
  // It is guaranteed, that no garbadge FrameInFlight objects will stay in
  // memory because of adding new peer. Even if the new peer won't receive the
  // frame, the frame will be removed by OnFrameRendered after next frame comes
  // for the new peer. It is important because FrameInFlight is a large object.
  for (auto& key_val : captured_frames_in_flight_) {
    key_val.second.AddPeer();
  }
}

void DefaultVideoQualityAnalyzer::Stop() {
  {
    MutexLock lock(&lock_);
    if (state_ == State::kStopped) {
      return;
    }
    state_ = State::kStopped;
  }
  StopMeasuringCpuProcessTime();
  comparison_available_event_.Set();
  thread_pool_.clear();

  // Perform final Metrics update. On this place analyzer is stopped and no one
  // holds any locks.
  {
    // Time between freezes.
    // Count time since the last freeze to the end of the call as time
    // between freezes.
    MutexLock lock1(&lock_);
    MutexLock lock2(&comparison_lock_);
    for (auto& state_entry : stream_states_) {
      const size_t stream_index = state_entry.first;
      const StreamState& stream_state = state_entry.second;
      for (size_t i = 0; i < peers_->size(); ++i) {
        if (i == static_cast<size_t>(stream_state.owner())) {
          continue;
        }

        InternalStatsKey stats_key(stream_index, stream_state.owner(), i);

        // If there are no freezes in the call we have to report
        // time_between_freezes_ms as call duration and in such case
        // |stream_last_freeze_end_time_| for this stream will be |start_time_|.
        // If there is freeze, then we need add time from last rendered frame
        // to last freeze end as time between freezes.
        if (stream_state.last_rendered_frame_time(i)) {
          stream_stats_[stats_key].time_between_freezes_ms.AddSample(
              StatsSample(
                  stream_state.last_rendered_frame_time(i).value().ms() -
                      stream_last_freeze_end_time_.at(stats_key).ms(),
                  Now()));
        }
      }
    }
    analyzer_stats_.frames_in_flight_left_count.AddSample(
        StatsSample(captured_frames_in_flight_.size(), Now()));
  }
  ReportResults();
}

std::string DefaultVideoQualityAnalyzer::GetStreamLabel(uint16_t frame_id) {
  MutexLock lock1(&lock_);
  auto it = captured_frames_in_flight_.find(frame_id);
  if (it != captured_frames_in_flight_.end()) {
    return streams_.name(it->second.stream());
  }
  for (auto hist_it = stream_to_frame_id_history_.begin();
       hist_it != stream_to_frame_id_history_.end(); ++hist_it) {
    auto hist_set_it = hist_it->second.find(frame_id);
    if (hist_set_it != hist_it->second.end()) {
      return streams_.name(hist_it->first);
    }
  }
  RTC_CHECK(false) << "Unknown frame_id=" << frame_id;
}

std::set<StatsKey> DefaultVideoQualityAnalyzer::GetKnownVideoStreams() const {
  MutexLock lock1(&lock_);
  MutexLock lock2(&comparison_lock_);
  std::set<StatsKey> out;
  for (auto& item : stream_stats_) {
    RTC_LOG(INFO) << item.first.ToString() << " ==> "
                  << ToStatsKey(item.first).ToString();
    out.insert(ToStatsKey(item.first));
  }
  return out;
}

const FrameCounters& DefaultVideoQualityAnalyzer::GetGlobalCounters() const {
  MutexLock lock(&lock_);
  return frame_counters_;
}

std::map<StatsKey, FrameCounters>
DefaultVideoQualityAnalyzer::GetPerStreamCounters() const {
  MutexLock lock(&lock_);
  std::map<StatsKey, FrameCounters> out;
  for (auto& item : stream_frame_counters_) {
    out.emplace(ToStatsKey(item.first), item.second);
  }
  return out;
}

std::map<StatsKey, StreamStats> DefaultVideoQualityAnalyzer::GetStats() const {
  MutexLock lock1(&lock_);
  MutexLock lock2(&comparison_lock_);
  std::map<StatsKey, StreamStats> out;
  for (auto& item : stream_stats_) {
    out.emplace(ToStatsKey(item.first), item.second);
  }
  return out;
}

AnalyzerStats DefaultVideoQualityAnalyzer::GetAnalyzerStats() const {
  MutexLock lock(&comparison_lock_);
  return analyzer_stats_;
}

void DefaultVideoQualityAnalyzer::AddComparison(
    InternalStatsKey stats_key,
    absl::optional<VideoFrame> captured,
    absl::optional<VideoFrame> rendered,
    bool dropped,
    FrameStats frame_stats) {
  StartExcludingCpuThreadTime();
  analyzer_stats_.comparisons_queue_size.AddSample(
      StatsSample(comparisons_.size(), Now()));
  // If there too many computations waiting in the queue, we won't provide
  // frames itself to make future computations lighter.
  if (comparisons_.size() >= kMaxActiveComparisons) {
    comparisons_.emplace_back(std::move(stats_key), absl::nullopt,
                              absl::nullopt, dropped, std::move(frame_stats),
                              OverloadReason::kCpu);
  } else {
    OverloadReason overload_reason = OverloadReason::kNone;
    if (!captured && !dropped) {
      overload_reason = OverloadReason::kMemory;
    }
    comparisons_.emplace_back(std::move(stats_key), std::move(captured),
                              std::move(rendered), dropped,
                              std::move(frame_stats), overload_reason);
  }
  comparison_available_event_.Set();
  StopExcludingCpuThreadTime();
}

void DefaultVideoQualityAnalyzer::ProcessComparisons() {
  while (true) {
    // Try to pick next comparison to perform from the queue.
    absl::optional<FrameComparison> comparison = absl::nullopt;
    {
      MutexLock lock(&comparison_lock_);
      if (!comparisons_.empty()) {
        comparison = comparisons_.front();
        comparisons_.pop_front();
        if (!comparisons_.empty()) {
          comparison_available_event_.Set();
        }
      }
    }
    if (!comparison) {
      bool more_frames_expected;
      {
        // If there are no comparisons and state is stopped =>
        // no more frames expected.
        MutexLock lock(&lock_);
        more_frames_expected = state_ != State::kStopped;
      }
      if (!more_frames_expected) {
        comparison_available_event_.Set();
        return;
      }
      comparison_available_event_.Wait(1000);
      continue;
    }

    StartExcludingCpuThreadTime();
    ProcessComparison(comparison.value());
    StopExcludingCpuThreadTime();
  }
}

void DefaultVideoQualityAnalyzer::ProcessComparison(
    const FrameComparison& comparison) {
  // Perform expensive psnr and ssim calculations while not holding lock.
  double psnr = -1.0;
  double ssim = -1.0;
  if (options_.heavy_metrics_computation_enabled && comparison.captured &&
      !comparison.dropped) {
    rtc::scoped_refptr<I420BufferInterface> reference_buffer =
        comparison.captured->video_frame_buffer()->ToI420();
    rtc::scoped_refptr<I420BufferInterface> test_buffer =
        comparison.rendered->video_frame_buffer()->ToI420();
    if (options_.adjust_cropping_before_comparing_frames) {
      test_buffer =
          ScaleVideoFrameBuffer(*test_buffer.get(), reference_buffer->width(),
                                reference_buffer->height());
      reference_buffer = test::AdjustCropping(reference_buffer, test_buffer);
    }
    psnr = I420PSNR(*reference_buffer.get(), *test_buffer.get());
    ssim = I420SSIM(*reference_buffer.get(), *test_buffer.get());
  }

  const FrameStats& frame_stats = comparison.frame_stats;

  MutexLock lock(&comparison_lock_);
  auto stats_it = stream_stats_.find(comparison.stats_key);
  RTC_CHECK(stats_it != stream_stats_.end()) << comparison.stats_key.ToString();
  StreamStats* stats = &stats_it->second;
  analyzer_stats_.comparisons_done++;
  if (comparison.overload_reason == OverloadReason::kCpu) {
    analyzer_stats_.cpu_overloaded_comparisons_done++;
  } else if (comparison.overload_reason == OverloadReason::kMemory) {
    analyzer_stats_.memory_overloaded_comparisons_done++;
  }
  if (psnr > 0) {
    stats->psnr.AddSample(StatsSample(psnr, frame_stats.rendered_time));
  }
  if (ssim > 0) {
    stats->ssim.AddSample(StatsSample(ssim, frame_stats.received_time));
  }
  if (frame_stats.encoded_time.IsFinite()) {
    stats->encode_time_ms.AddSample(StatsSample(
        (frame_stats.encoded_time - frame_stats.pre_encode_time).ms(),
        frame_stats.encoded_time));
    stats->encode_frame_rate.AddEvent(frame_stats.encoded_time);
    stats->total_encoded_images_payload += frame_stats.encoded_image_size;
    stats->target_encode_bitrate.AddSample(StatsSample(
        frame_stats.target_encode_bitrate, frame_stats.encoded_time));
  } else {
    if (frame_stats.pre_encode_time.IsFinite()) {
      stats->dropped_by_encoder++;
    } else {
      stats->dropped_before_encoder++;
    }
  }
  // Next stats can be calculated only if frame was received on remote side.
  if (!comparison.dropped) {
    stats->resolution_of_rendered_frame.AddSample(
        StatsSample(*comparison.frame_stats.rendered_frame_width *
                        *comparison.frame_stats.rendered_frame_height,
                    frame_stats.rendered_time));
    stats->transport_time_ms.AddSample(StatsSample(
        (frame_stats.decode_start_time - frame_stats.encoded_time).ms(),
        frame_stats.received_time));
    stats->total_delay_incl_transport_ms.AddSample(StatsSample(
        (frame_stats.rendered_time - frame_stats.captured_time).ms(),
        frame_stats.received_time));
    stats->decode_time_ms.AddSample(StatsSample(
        (frame_stats.decode_end_time - frame_stats.decode_start_time).ms(),
        frame_stats.decode_end_time));
    stats->receive_to_render_time_ms.AddSample(StatsSample(
        (frame_stats.rendered_time - frame_stats.received_time).ms(),
        frame_stats.rendered_time));

    if (frame_stats.prev_frame_rendered_time.IsFinite()) {
      TimeDelta time_between_rendered_frames =
          frame_stats.rendered_time - frame_stats.prev_frame_rendered_time;
      stats->time_between_rendered_frames_ms.AddSample(StatsSample(
          time_between_rendered_frames.ms(), frame_stats.rendered_time));
      double average_time_between_rendered_frames_ms =
          stats->time_between_rendered_frames_ms.GetAverage();
      if (time_between_rendered_frames.ms() >
          std::max(kFreezeThresholdMs + average_time_between_rendered_frames_ms,
                   3 * average_time_between_rendered_frames_ms)) {
        stats->freeze_time_ms.AddSample(StatsSample(
            time_between_rendered_frames.ms(), frame_stats.rendered_time));
        auto freeze_end_it =
            stream_last_freeze_end_time_.find(comparison.stats_key);
        RTC_DCHECK(freeze_end_it != stream_last_freeze_end_time_.end());
        stats->time_between_freezes_ms.AddSample(StatsSample(
            (frame_stats.prev_frame_rendered_time - freeze_end_it->second).ms(),
            frame_stats.rendered_time));
        freeze_end_it->second = frame_stats.rendered_time;
      }
    }
  }
}

void DefaultVideoQualityAnalyzer::ReportResults() {
  using ::webrtc::test::ImproveDirection;

  MutexLock lock1(&lock_);
  MutexLock lock2(&comparison_lock_);
  for (auto& item : stream_stats_) {
    ReportResults(GetTestCaseName(StatsKeyToMetricName(ToStatsKey(item.first))),
                  item.second, stream_frame_counters_.at(item.first));
  }
  test::PrintResult("cpu_usage", "", test_label_.c_str(), GetCpuUsagePercent(),
                    "%", false, ImproveDirection::kSmallerIsBetter);
  LogFrameCounters("Global", frame_counters_);
  for (auto& item : stream_stats_) {
    LogFrameCounters(ToStatsKey(item.first).ToString(),
                     stream_frame_counters_.at(item.first));
    LogStreamInternalStats(ToStatsKey(item.first).ToString(), item.second);
  }
  if (!analyzer_stats_.comparisons_queue_size.IsEmpty()) {
    RTC_LOG(INFO) << "comparisons_queue_size min="
                  << analyzer_stats_.comparisons_queue_size.GetMin()
                  << "; max=" << analyzer_stats_.comparisons_queue_size.GetMax()
                  << "; 99%="
                  << analyzer_stats_.comparisons_queue_size.GetPercentile(0.99);
  }
  RTC_LOG(INFO) << "comparisons_done=" << analyzer_stats_.comparisons_done;
  RTC_LOG(INFO) << "cpu_overloaded_comparisons_done="
                << analyzer_stats_.cpu_overloaded_comparisons_done;
  RTC_LOG(INFO) << "memory_overloaded_comparisons_done="
                << analyzer_stats_.memory_overloaded_comparisons_done;
}

void DefaultVideoQualityAnalyzer::ReportResults(
    const std::string& test_case_name,
    const StreamStats& stats,
    const FrameCounters& frame_counters) {
  using ::webrtc::test::ImproveDirection;
  TimeDelta test_duration = Now() - start_time_;

  double sum_squared_interframe_delays_secs = 0;
  Timestamp video_start_time = Timestamp::PlusInfinity();
  Timestamp video_end_time = Timestamp::MinusInfinity();
  for (const SamplesStatsCounter::StatsSample& sample :
       stats.time_between_rendered_frames_ms.GetTimedSamples()) {
    double interframe_delay_ms = sample.value;
    const double interframe_delays_secs = interframe_delay_ms / 1000.0;
    // Sum of squared inter frame intervals is used to calculate the harmonic
    // frame rate metric. The metric aims to reflect overall experience related
    // to smoothness of video playback and includes both freezes and pauses.
    sum_squared_interframe_delays_secs +=
        interframe_delays_secs * interframe_delays_secs;
    if (sample.time < video_start_time) {
      video_start_time = sample.time;
    }
    if (sample.time > video_end_time) {
      video_end_time = sample.time;
    }
  }
  double harmonic_framerate_fps = 0;
  TimeDelta video_duration = video_end_time - video_start_time;
  if (sum_squared_interframe_delays_secs > 0.0 && video_duration.IsFinite()) {
    harmonic_framerate_fps = static_cast<double>(video_duration.us()) /
                             static_cast<double>(kMicrosPerSecond) /
                             sum_squared_interframe_delays_secs;
  }

  ReportResult("psnr", test_case_name, stats.psnr, "dB",
               ImproveDirection::kBiggerIsBetter);
  ReportResult("ssim", test_case_name, stats.ssim, "unitless",
               ImproveDirection::kBiggerIsBetter);
  ReportResult("transport_time", test_case_name, stats.transport_time_ms, "ms",
               ImproveDirection::kSmallerIsBetter);
  ReportResult("total_delay_incl_transport", test_case_name,
               stats.total_delay_incl_transport_ms, "ms",
               ImproveDirection::kSmallerIsBetter);
  ReportResult("time_between_rendered_frames", test_case_name,
               stats.time_between_rendered_frames_ms, "ms",
               ImproveDirection::kSmallerIsBetter);
  test::PrintResult("harmonic_framerate", "", test_case_name,
                    harmonic_framerate_fps, "Hz", /*important=*/false,
                    ImproveDirection::kBiggerIsBetter);
  test::PrintResult("encode_frame_rate", "", test_case_name,
                    stats.encode_frame_rate.IsEmpty()
                        ? 0
                        : stats.encode_frame_rate.GetEventsPerSecond(),
                    "Hz", /*important=*/false,
                    ImproveDirection::kBiggerIsBetter);
  ReportResult("encode_time", test_case_name, stats.encode_time_ms, "ms",
               ImproveDirection::kSmallerIsBetter);
  ReportResult("time_between_freezes", test_case_name,
               stats.time_between_freezes_ms, "ms",
               ImproveDirection::kBiggerIsBetter);
  ReportResult("freeze_time_ms", test_case_name, stats.freeze_time_ms, "ms",
               ImproveDirection::kSmallerIsBetter);
  ReportResult("pixels_per_frame", test_case_name,
               stats.resolution_of_rendered_frame, "count",
               ImproveDirection::kBiggerIsBetter);
  test::PrintResult("min_psnr", "", test_case_name,
                    stats.psnr.IsEmpty() ? 0 : stats.psnr.GetMin(), "dB",
                    /*important=*/false, ImproveDirection::kBiggerIsBetter);
  ReportResult("decode_time", test_case_name, stats.decode_time_ms, "ms",
               ImproveDirection::kSmallerIsBetter);
  ReportResult("receive_to_render_time", test_case_name,
               stats.receive_to_render_time_ms, "ms",
               ImproveDirection::kSmallerIsBetter);
  test::PrintResult("dropped_frames", "", test_case_name,
                    frame_counters.dropped, "count",
                    /*important=*/false, ImproveDirection::kSmallerIsBetter);
  test::PrintResult("frames_in_flight", "", test_case_name,
                    frame_counters.captured - frame_counters.rendered -
                        frame_counters.dropped,
                    "count",
                    /*important=*/false, ImproveDirection::kSmallerIsBetter);
  test::PrintResult("rendered_frames", "", test_case_name,
                    frame_counters.rendered, "count", /*important=*/false,
                    ImproveDirection::kBiggerIsBetter);
  ReportResult("max_skipped", test_case_name, stats.skipped_between_rendered,
               "count", ImproveDirection::kSmallerIsBetter);
  ReportResult("target_encode_bitrate", test_case_name,
               stats.target_encode_bitrate / kBitsInByte, "bytesPerSecond",
               ImproveDirection::kNone);
  test::PrintResult(
      "actual_encode_bitrate", "", test_case_name,
      static_cast<double>(stats.total_encoded_images_payload) /
          static_cast<double>(test_duration.us()) * kMicrosPerSecond,
      "bytesPerSecond", /*important=*/false, ImproveDirection::kNone);
}

void DefaultVideoQualityAnalyzer::ReportResult(
    const std::string& metric_name,
    const std::string& test_case_name,
    const SamplesStatsCounter& counter,
    const std::string& unit,
    webrtc::test::ImproveDirection improve_direction) {
  test::PrintResult(metric_name, /*modifier=*/"", test_case_name, counter, unit,
                    /*important=*/false, improve_direction);
}

std::string DefaultVideoQualityAnalyzer::GetTestCaseName(
    const std::string& stream_label) const {
  return test_label_ + "/" + stream_label;
}

Timestamp DefaultVideoQualityAnalyzer::Now() {
  return clock_->CurrentTime();
}

StatsKey DefaultVideoQualityAnalyzer::ToStatsKey(
    const InternalStatsKey& key) const {
  return StatsKey(streams_.name(key.stream), peers_->name(key.sender),
                  peers_->name(key.receiver));
}

std::string DefaultVideoQualityAnalyzer::StatsKeyToMetricName(
    const StatsKey& key) const {
  if (peers_->size() <= 2) {
    return key.stream_label;
  }
  return key.ToString();
}

void DefaultVideoQualityAnalyzer::StartMeasuringCpuProcessTime() {
  MutexLock lock(&cpu_measurement_lock_);
  cpu_time_ -= rtc::GetProcessCpuTimeNanos();
  wallclock_time_ -= rtc::SystemTimeNanos();
}

void DefaultVideoQualityAnalyzer::StopMeasuringCpuProcessTime() {
  MutexLock lock(&cpu_measurement_lock_);
  cpu_time_ += rtc::GetProcessCpuTimeNanos();
  wallclock_time_ += rtc::SystemTimeNanos();
}

void DefaultVideoQualityAnalyzer::StartExcludingCpuThreadTime() {
  MutexLock lock(&cpu_measurement_lock_);
  cpu_time_ += rtc::GetThreadCpuTimeNanos();
}

void DefaultVideoQualityAnalyzer::StopExcludingCpuThreadTime() {
  MutexLock lock(&cpu_measurement_lock_);
  cpu_time_ -= rtc::GetThreadCpuTimeNanos();
}

double DefaultVideoQualityAnalyzer::GetCpuUsagePercent() {
  MutexLock lock(&cpu_measurement_lock_);
  return static_cast<double>(cpu_time_) / wallclock_time_ * 100.0;
}

DefaultVideoQualityAnalyzer::FrameComparison::FrameComparison(
    InternalStatsKey stats_key,
    absl::optional<VideoFrame> captured,
    absl::optional<VideoFrame> rendered,
    bool dropped,
    FrameStats frame_stats,
    OverloadReason overload_reason)
    : stats_key(std::move(stats_key)),
      captured(std::move(captured)),
      rendered(std::move(rendered)),
      dropped(dropped),
      frame_stats(std::move(frame_stats)),
      overload_reason(overload_reason) {}

uint16_t DefaultVideoQualityAnalyzer::StreamState::PopFront(size_t peer) {
  absl::optional<uint16_t> frame_id = frame_ids_.PopFront(peer);
  RTC_DCHECK(frame_id.has_value());

  // If alive's frame queue is longer than all others, than also pop frame from
  // it, because that frame is received by all receivers.
  size_t owner_size = frame_ids_.size(owner_);
  size_t other_size = 0;
  for (size_t i = 0; i < frame_ids_.readers_count(); ++i) {
    size_t cur_size = frame_ids_.size(i);
    if (i != owner_ && cur_size > other_size) {
      other_size = cur_size;
    }
  }
  if (owner_size > other_size) {
    absl::optional<uint16_t> alive_frame_id = frame_ids_.PopFront(owner_);
    RTC_DCHECK(alive_frame_id.has_value());
    RTC_DCHECK_EQ(frame_id.value(), alive_frame_id.value());
  }

  return frame_id.value();
}

uint16_t DefaultVideoQualityAnalyzer::StreamState::MarkNextAliveFrameAsDead() {
  absl::optional<uint16_t> frame_id = frame_ids_.PopFront(owner_);
  RTC_DCHECK(frame_id.has_value());
  return frame_id.value();
}

void DefaultVideoQualityAnalyzer::StreamState::SetLastRenderedFrameTime(
    size_t peer,
    Timestamp time) {
  auto it = last_rendered_frame_time_.find(peer);
  if (it == last_rendered_frame_time_.end()) {
    last_rendered_frame_time_.insert({peer, time});
  } else {
    it->second = time;
  }
}

absl::optional<Timestamp>
DefaultVideoQualityAnalyzer::StreamState::last_rendered_frame_time(
    size_t peer) const {
  return MaybeGetValue(last_rendered_frame_time_, peer);
}

bool DefaultVideoQualityAnalyzer::FrameInFlight::RemoveFrame() {
  if (!frame_) {
    return false;
  }
  frame_ = absl::nullopt;
  return true;
}

void DefaultVideoQualityAnalyzer::FrameInFlight::SetFrameId(uint16_t id) {
  if (frame_) {
    frame_->set_id(id);
  }
}

std::vector<size_t>
DefaultVideoQualityAnalyzer::FrameInFlight::GetPeersWhichDidntReceive() const {
  std::vector<size_t> out;
  for (size_t i = 0; i < peers_count_; ++i) {
    auto it = receiver_stats_.find(i);
    if (i != owner_ && it != receiver_stats_.end() &&
        it->second.rendered_time.IsInfinite()) {
      out.push_back(i);
    }
  }
  return out;
}

bool DefaultVideoQualityAnalyzer::FrameInFlight::HaveAllPeersReceived() const {
  for (size_t i = 0; i < peers_count_; ++i) {
    if (i == owner_) {
      continue;
    }

    auto it = receiver_stats_.find(i);
    if (it == receiver_stats_.end()) {
      return false;
    }

    if (!it->second.dropped && it->second.rendered_time.IsInfinite()) {
      return false;
    }
  }
  return true;
}

void DefaultVideoQualityAnalyzer::FrameInFlight::OnFrameEncoded(
    webrtc::Timestamp time,
    int64_t encoded_image_size,
    uint32_t target_encode_bitrate) {
  encoded_time_ = time;
  encoded_image_size_ = encoded_image_size;
  target_encode_bitrate_ += target_encode_bitrate;
}

void DefaultVideoQualityAnalyzer::FrameInFlight::OnFramePreDecode(
    size_t peer,
    webrtc::Timestamp received_time,
    webrtc::Timestamp decode_start_time) {
  receiver_stats_[peer].received_time = received_time;
  receiver_stats_[peer].decode_start_time = decode_start_time;
}

bool DefaultVideoQualityAnalyzer::FrameInFlight::HasReceivedTime(
    size_t peer) const {
  auto it = receiver_stats_.find(peer);
  if (it == receiver_stats_.end()) {
    return false;
  }
  return it->second.received_time.IsFinite();
}

bool DefaultVideoQualityAnalyzer::FrameInFlight::HasDecodeEndTime(
    size_t peer) const {
  auto it = receiver_stats_.find(peer);
  if (it == receiver_stats_.end()) {
    return false;
  }
  return it->second.decode_end_time.IsFinite();
}

void DefaultVideoQualityAnalyzer::FrameInFlight::OnFrameRendered(
    size_t peer,
    webrtc::Timestamp time,
    int width,
    int height) {
  receiver_stats_[peer].rendered_time = time;
  receiver_stats_[peer].rendered_frame_width = width;
  receiver_stats_[peer].rendered_frame_height = height;
}

bool DefaultVideoQualityAnalyzer::FrameInFlight::HasRenderedTime(
    size_t peer) const {
  auto it = receiver_stats_.find(peer);
  if (it == receiver_stats_.end()) {
    return false;
  }
  return it->second.rendered_time.IsFinite();
}

DefaultVideoQualityAnalyzer::FrameStats
DefaultVideoQualityAnalyzer::FrameInFlight::GetStatsForPeer(size_t peer) const {
  FrameStats stats(captured_time_);
  stats.pre_encode_time = pre_encode_time_;
  stats.encoded_time = encoded_time_;
  stats.target_encode_bitrate = target_encode_bitrate_;
  stats.encoded_image_size = encoded_image_size_;

  absl::optional<ReceiverFrameStats> receiver_stats =
      MaybeGetValue<ReceiverFrameStats>(receiver_stats_, peer);
  if (receiver_stats.has_value()) {
    stats.received_time = receiver_stats->received_time;
    stats.decode_start_time = receiver_stats->decode_start_time;
    stats.decode_end_time = receiver_stats->decode_end_time;
    stats.rendered_time = receiver_stats->rendered_time;
    stats.prev_frame_rendered_time = receiver_stats->prev_frame_rendered_time;
    stats.rendered_frame_width = receiver_stats->rendered_frame_width;
    stats.rendered_frame_height = receiver_stats->rendered_frame_height;
  }
  return stats;
}

size_t DefaultVideoQualityAnalyzer::NamesCollection::AddIfAbsent(
    absl::string_view name) {
  auto it = index_.find(name);
  if (it != index_.end()) {
    return it->second;
  }
  size_t out = names_.size();
  size_t old_capacity = names_.capacity();
  names_.emplace_back(name);
  size_t new_capacity = names_.capacity();

  if (old_capacity == new_capacity) {
    index_.emplace(names_[out], out);
  } else {
    // Reallocation happened in the vector, so we need to rebuild |index_|
    index_.clear();
    for (size_t i = 0; i < names_.size(); ++i) {
      index_.emplace(names_[i], i);
    }
  }
  return out;
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
