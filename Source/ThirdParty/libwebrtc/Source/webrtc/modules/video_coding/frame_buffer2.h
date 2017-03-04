/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_FRAME_BUFFER2_H_
#define WEBRTC_MODULES_VIDEO_CODING_FRAME_BUFFER2_H_

#include <array>
#include <map>
#include <memory>
#include <utility>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/event.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/video_coding/frame_object.h"
#include "webrtc/modules/video_coding/include/video_coding_defines.h"
#include "webrtc/modules/video_coding/inter_frame_delay.h"
#include "webrtc/modules/video_coding/sequence_number_util.h"

namespace webrtc {

class Clock;
class VCMReceiveStatisticsCallback;
class VCMJitterEstimator;
class VCMTiming;

namespace video_coding {

class FrameBuffer {
 public:
  enum ReturnReason { kFrameFound, kTimeout, kStopped };

  FrameBuffer(Clock* clock,
              VCMJitterEstimator* jitter_estimator,
              VCMTiming* timing,
              VCMReceiveStatisticsCallback* stats_proxy);

  virtual ~FrameBuffer();

  // Insert a frame into the frame buffer. Returns the picture id
  // of the last continuous frame or -1 if there is no continuous frame.
  int InsertFrame(std::unique_ptr<FrameObject> frame);

  // Get the next frame for decoding. Will return at latest after
  // |max_wait_time_ms|.
  //  - If a frame is available within |max_wait_time_ms| it will return
  //    kFrameFound and set |frame_out| to the resulting frame.
  //  - If no frame is available after |max_wait_time_ms| it will return
  //    kTimeout.
  //  - If the FrameBuffer is stopped then it will return kStopped.
  ReturnReason NextFrame(int64_t max_wait_time_ms,
                         std::unique_ptr<FrameObject>* frame_out);

  // Tells the FrameBuffer which protection mode that is in use. Affects
  // the frame timing.
  // TODO(philipel): Remove this when new timing calculations has been
  //                 implemented.
  void SetProtectionMode(VCMVideoProtection mode);

  // Start the frame buffer, has no effect if the frame buffer is started.
  // The frame buffer is started upon construction.
  void Start();

  // Stop the frame buffer, causing any sleeping thread in NextFrame to
  // return immediately.
  void Stop();

 private:
  struct FrameKey {
    FrameKey() : picture_id(0), spatial_layer(0) {}
    FrameKey(uint16_t picture_id, uint8_t spatial_layer)
        : picture_id(picture_id), spatial_layer(spatial_layer) {}

    bool operator<(const FrameKey& rhs) const {
      if (picture_id == rhs.picture_id)
        return spatial_layer < rhs.spatial_layer;
      return AheadOf(rhs.picture_id, picture_id);
    }

    bool operator<=(const FrameKey& rhs) const { return !(rhs < *this); }

    uint16_t picture_id;
    uint8_t spatial_layer;
  };

  struct FrameInfo {
    // The maximum number of frames that can depend on this frame.
    static constexpr size_t kMaxNumDependentFrames = 8;

    // Which other frames that have direct unfulfilled dependencies
    // on this frame.
    FrameKey dependent_frames[kMaxNumDependentFrames];
    size_t num_dependent_frames = 0;

    // A frame is continiuous if it has all its referenced/indirectly
    // referenced frames.
    //
    // How many unfulfilled frames this frame have until it becomes continuous.
    size_t num_missing_continuous = 0;

    // A frame is decodable if all its referenced frames have been decoded.
    //
    // How many unfulfilled frames this frame have until it becomes decodable.
    size_t num_missing_decodable = 0;

    // If this frame is continuous or not.
    bool continuous = false;

    // The actual FrameObject.
    std::unique_ptr<FrameObject> frame;
  };

  using FrameMap = std::map<FrameKey, FrameInfo>;

  // Update all directly dependent and indirectly dependent frames and mark
  // them as continuous if all their references has been fulfilled.
  void PropagateContinuity(FrameMap::iterator start)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Marks the frame as decoded and updates all directly dependent frames.
  void PropagateDecodability(const FrameInfo& info)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Advances |last_decoded_frame_it_| to |decoded| and removes old
  // frame info.
  void AdvanceLastDecodedFrame(FrameMap::iterator decoded)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Update the corresponding FrameInfo of |frame| and all FrameInfos that
  // |frame| references.
  // Return false if |frame| will never be decodable, true otherwise.
  bool UpdateFrameInfoWithIncomingFrame(const FrameObject& frame,
                                        FrameMap::iterator info)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  void UpdateJitterDelay() EXCLUSIVE_LOCKS_REQUIRED(crit_);

  void ClearFramesAndHistory() EXCLUSIVE_LOCKS_REQUIRED(crit_);

  FrameMap frames_ GUARDED_BY(crit_);

  rtc::CriticalSection crit_;
  Clock* const clock_;
  rtc::Event new_countinuous_frame_event_;
  VCMJitterEstimator* const jitter_estimator_ GUARDED_BY(crit_);
  VCMTiming* const timing_ GUARDED_BY(crit_);
  VCMInterFrameDelay inter_frame_delay_ GUARDED_BY(crit_);
  uint32_t last_decoded_frame_timestamp_ GUARDED_BY(crit_);
  FrameMap::iterator last_decoded_frame_it_ GUARDED_BY(crit_);
  FrameMap::iterator last_continuous_frame_it_ GUARDED_BY(crit_);
  FrameMap::iterator next_frame_it_ GUARDED_BY(crit_);
  int num_frames_history_ GUARDED_BY(crit_);
  int num_frames_buffered_ GUARDED_BY(crit_);
  bool stopped_ GUARDED_BY(crit_);
  VCMVideoProtection protection_mode_ GUARDED_BY(crit_);
  VCMReceiveStatisticsCallback* const stats_callback_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(FrameBuffer);
};

}  // namespace video_coding
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_FRAME_BUFFER2_H_
