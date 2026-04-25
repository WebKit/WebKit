/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_TIMING_SIMULATOR_RENDERING_SIMULATOR_H_
#define VIDEO_TIMING_SIMULATOR_RENDERING_SIMULATOR_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/array_view.h"
#include "api/environment/environment.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "modules/video_coding/timing/timing.h"
#include "video/timing/simulator/frame_base.h"
#include "video/timing/simulator/results_base.h"
#include "video/timing/simulator/stream_base.h"

namespace webrtc::video_timing_simulator {

// The `RenderingSimulator` takes an `ParsedRtcEventLog` and produces a
// sequence of metadata about decoded and rendered frames that were contained in
// the log.
class RenderingSimulator {
 public:
  struct Config {
    using VideoTimingFactory =
        std::function<std::unique_ptr<VCMTiming>(Environment)>;

    std::string name = "";
    std::string field_trials_string = "";
    VideoTimingFactory video_timing_factory = [](Environment env) {
      return std::make_unique<VCMTiming>(&env.clock(), env.field_trials());
    };

    // Whether or not to reset the stream state on newly logged streams with the
    // same SSRC.
    bool reuse_streams = false;
  };

  // Metadata about a single rendered frame.
  struct Frame : public FrameBase<Frame> {
    // -- Values --
    // Frame information.
    int num_packets = -1;
    DataSize size = DataSize::Zero();

    // RTP header information.
    int payload_type = -1;
    uint32_t rtp_timestamp = 0;
    int64_t unwrapped_rtp_timestamp = -1;

    // Dependency descriptor information.
    int64_t frame_id = -1;
    int spatial_id = -1;
    int temporal_id = -1;
    int num_references = -1;

    // Packet timestamps.
    Timestamp first_packet_arrival_timestamp = Timestamp::PlusInfinity();
    Timestamp last_packet_arrival_timestamp = Timestamp::MinusInfinity();

    // Frame timestamps.
    Timestamp assembled_timestamp = Timestamp::PlusInfinity();
    Timestamp render_timestamp = Timestamp::PlusInfinity();
    Timestamp decoded_timestamp = Timestamp::PlusInfinity();
    Timestamp rendered_timestamp = Timestamp::PlusInfinity();

    // Jitter buffer state at the time of this frame.
    int frames_dropped = -1;
    // TODO: b/423646186 - Add `current_delay_ms`.
    // The `jitter_buffer_*` metrics below are recorded by the production code,
    // and should be compatible with the `webrtc-stats` definitions. One major
    // difference is that they are _not_ cumulative.
    // https://www.w3.org/TR/webrtc-stats/#dom-rtcinboundrtpstreamstats-jitterbufferminimumdelay
    TimeDelta jitter_buffer_minimum_delay = TimeDelta::MinusInfinity();
    // https://www.w3.org/TR/webrtc-stats/#dom-rtcinboundrtpstreamstats-jitterbuffertargetdelay
    TimeDelta jitter_buffer_target_delay = TimeDelta::MinusInfinity();
    // https://www.w3.org/TR/webrtc-stats/#dom-rtcinboundrtpstreamstats-jitterbufferdelay
    TimeDelta jitter_buffer_delay = TimeDelta::MinusInfinity();

    // -- Populated values --
    // One-way delay relative some baseline.
    TimeDelta frame_delay_variation = TimeDelta::PlusInfinity();

    // -- Value accessors --
    Timestamp ArrivalTimestampInternal() const { return rendered_timestamp; }

    // -- Per-frame metrics --
    // Time spent being assembled (waiting for all packets to arrive).
    TimeDelta PacketBufferDuration() const {
      return assembled_timestamp - first_packet_arrival_timestamp;
    }
    // Time spent waiting to be decoded, after assembly. (This includes a,
    // currently, zero decode duration.) Note that this is similar to
    // `jitter_buffer_delay`, except that the latter is 1) recorded by the
    // production code; and, 2) is anchored on `first_packet_arrival_timestamp`
    // rather than `assembled_timestamp`.
    TimeDelta FrameBufferDuration() const {
      return decoded_timestamp - assembled_timestamp;
    }
    // Time spent waiting to be rendered, after decode.
    TimeDelta RenderBufferDuration() const {
      return rendered_timestamp - decoded_timestamp;
    }
    // Margin between render timestamp (target) and
    // assembled timestamp (actual):
    //   * A frame that is assembled early w.r.t. the target (<=> arriving
    //     on time from the network) has a positive margin.
    //   * A frame that is assembled on time w.r.t. the target (<=> arriving
    //     slightly late from the network) has zero margin.
    //   * A frame that is assembled late w.r.t. the target (<=> arriving
    //     very late from the network) has negative margin.
    // Positive margins mean no video freezes, at the cost of receiver delay.
    // A jitter buffer needs to strike a balance between video freezes and
    // delay. In terms of margin, that means low positive margin values, a
    // couple of frames with zero margin, and very few frames with
    // negative margin.
    TimeDelta RenderMargin() const {
      return render_timestamp - assembled_timestamp;
    }
    // Split the margin along zero: "excess margin" for positive margins and
    // "deficit margin" for negative margins.
    // A jitter buffer would generally want to minimize the number of frames
    // with a deficit margin (delayed frames/buffer underruns => video freezes),
    // while also minimizing the stream-level min/p10 of the excess margin
    // (early frames spending a long time in the buffer => high latency).
    std::optional<TimeDelta> RenderExcessMargin() const {
      TimeDelta margin = RenderMargin();
      if (margin < TimeDelta::Zero()) {
        return std::nullopt;
      }
      return margin;
    }
    std::optional<TimeDelta> RenderDeficitMargin() const {
      TimeDelta margin = RenderMargin();
      if (margin > TimeDelta::Zero()) {
        return std::nullopt;
      }
      return margin;
    }
  };

  // All frames in one stream.
  struct Stream : public StreamBase<Stream> {
    Timestamp creation_timestamp = Timestamp::PlusInfinity();
    uint32_t ssrc = 0;
    std::vector<Frame> frames;
  };

  // All streams.
  struct Results : public ResultsBase<Results> {
    std::string config_name;
    std::vector<Stream> streams;
  };

  // Static configuration.
  static constexpr TimeDelta kRenderDelay = TimeDelta::Millis(10);

  explicit RenderingSimulator(Config config);
  ~RenderingSimulator();

  RenderingSimulator(const RenderingSimulator&) = delete;
  RenderingSimulator& operator=(const RenderingSimulator&) = delete;

  Results Simulate(const ParsedRtcEventLog& parsed_log) const;

 private:
  const Config config_;
};

// -- Comparators and sorting --
inline bool RenderOrder(const RenderingSimulator::Frame& a,
                        const RenderingSimulator::Frame& b) {
  return a.render_timestamp < b.render_timestamp;
}
inline void SortByRenderOrder(ArrayView<RenderingSimulator::Frame> frames) {
  absl::c_stable_sort(frames, RenderOrder);
}

inline bool DecodedOrder(const RenderingSimulator::Frame& a,
                         const RenderingSimulator::Frame& b) {
  return a.decoded_timestamp < b.decoded_timestamp;
}
inline void SortByDecodedOrder(ArrayView<RenderingSimulator::Frame> frames) {
  absl::c_stable_sort(frames, DecodedOrder);
}

inline bool RenderedOrder(const RenderingSimulator::Frame& a,
                          const RenderingSimulator::Frame& b) {
  return a.rendered_timestamp < b.rendered_timestamp;
}
inline void SortByRenderedOrder(ArrayView<RenderingSimulator::Frame> frames) {
  absl::c_stable_sort(frames, RenderedOrder);
}

// -- Inter-frame metrics --
// Difference in render time (target) between two frames.
inline TimeDelta InterRenderTime(const RenderingSimulator::Frame& cur,
                                 const RenderingSimulator::Frame& prev) {
  return cur.render_timestamp - prev.render_timestamp;
}

// Difference in decoded time (actual) between two frames.
inline TimeDelta InterDecodedTime(const RenderingSimulator::Frame& cur,
                                  const RenderingSimulator::Frame& prev) {
  return cur.decoded_timestamp - prev.decoded_timestamp;
}

// Difference in rendered time (actual) between two frames.
inline TimeDelta InterRenderedTime(const RenderingSimulator::Frame& cur,
                                   const RenderingSimulator::Frame& prev) {
  return cur.rendered_timestamp - prev.rendered_timestamp;
}

}  // namespace webrtc::video_timing_simulator

#endif  // VIDEO_TIMING_SIMULATOR_RENDERING_SIMULATOR_H_
