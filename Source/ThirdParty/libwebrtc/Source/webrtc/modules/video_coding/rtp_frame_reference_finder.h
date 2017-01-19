/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_RTP_FRAME_REFERENCE_FINDER_H_
#define WEBRTC_MODULES_VIDEO_CODING_RTP_FRAME_REFERENCE_FINDER_H_

#include <array>
#include <map>
#include <memory>
#include <deque>
#include <set>
#include <utility>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/video_coding/sequence_number_util.h"

namespace webrtc {
namespace video_coding {

class FrameObject;
class RtpFrameObject;

// A complete frame is a frame which has received all its packets and all its
// references are known.
class OnCompleteFrameCallback {
 public:
  virtual ~OnCompleteFrameCallback() {}
  virtual void OnCompleteFrame(std::unique_ptr<FrameObject> frame) = 0;
};

class RtpFrameReferenceFinder {
 public:
  explicit RtpFrameReferenceFinder(OnCompleteFrameCallback* frame_callback);

  // Manage this frame until:
  //  - We have all information needed to determine its references, after
  //    which |frame_callback_| is called with the completed frame, or
  //  - We have too many stashed frames (determined by |kMaxStashedFrames)
  //    so we drop this frame, or
  //  - It gets cleared by ClearTo, which also means we drop it.
  void ManageFrame(std::unique_ptr<RtpFrameObject> frame);

  // Notifies that padding has been received, which the reference finder
  // might need to calculate the references of a frame.
  void PaddingReceived(uint16_t seq_num);

  // Clear all stashed frames that include packets older than |seq_num|.
  void ClearTo(uint16_t seq_num);

 private:
  static const uint16_t kPicIdLength = 1 << 15;
  static const uint8_t kMaxTemporalLayers = 5;
  static const int kMaxLayerInfo = 50;
  static const int kMaxStashedFrames = 50;
  static const int kMaxNotYetReceivedFrames = 100;
  static const int kMaxGofSaved = 50;
  static const int kMaxPaddingAge = 100;


  struct GofInfo {
    GofInfo(GofInfoVP9* gof, uint16_t last_picture_id)
        : gof(gof), last_picture_id(last_picture_id) {}
    GofInfoVP9* gof;
    uint16_t last_picture_id;
  };

  rtc::CriticalSection crit_;

  // Find the relevant group of pictures and update its "last-picture-id-with
  // padding" sequence number.
  void UpdateLastPictureIdWithPadding(uint16_t seq_num)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Retry finding references for all frames that previously didn't have
  // all information needed.
  void RetryStashedFrames() EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Find references for generic frames. If |picture_id| is unspecified
  // then packet sequence numbers will be used to determine the references
  // of the frames.
  void ManageFrameGeneric(std::unique_ptr<RtpFrameObject> frame,
                          int picture_id) EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Find references for Vp8 frames
  void ManageFrameVp8(std::unique_ptr<RtpFrameObject> frame)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Updates all necessary state used to determine frame references
  // for Vp8 and then calls the |frame_callback| callback with the
  // completed frame.
  void CompletedFrameVp8(std::unique_ptr<RtpFrameObject> frame)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Find references for Vp9 frames
  void ManageFrameVp9(std::unique_ptr<RtpFrameObject> frame)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Unwrap the picture id and the frame references  and then call the
  // |frame_callback| callback with the completed frame.
  void CompletedFrameVp9(std::unique_ptr<RtpFrameObject> frame)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Check if we are missing a frame necessary to determine the references
  // for this frame.
  bool MissingRequiredFrameVp9(uint16_t picture_id, const GofInfo& info)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Updates which frames that have been received. If there is a gap,
  // missing frames will be added to |missing_frames_for_layer_| or
  // if this is an already missing frame then it will be removed.
  void FrameReceivedVp9(uint16_t picture_id, GofInfo* info)
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Check if there is a frame with the up-switch flag set in the interval
  // (|pid_ref|, |picture_id|) with temporal layer smaller than |temporal_idx|.
  bool UpSwitchInIntervalVp9(uint16_t picture_id,
                             uint8_t temporal_idx,
                             uint16_t pid_ref) EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // All picture ids are unwrapped to 16 bits.
  uint16_t UnwrapPictureId(uint16_t picture_id) EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // Returns true if the frame is old and should be dropped.
  // TODO(philipel): Remove when VP9 PID/TL0 does not jump mid-stream (should be
  //                 around M59).
  bool Vp9PidTl0Fix(const RtpFrameObject& frame,
                    int16_t* picture_id,
                    int16_t* tl0_pic_idx) EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // TODO(philipel): Remove when VP9 PID/TL0 does not jump mid-stream (should be
  //                 around M59).
  bool DetectVp9PicIdJump(int fixed_pid,
                          int fixed_tl0,
                          uint32_t timestamp) const
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // TODO(philipel): Remove when VP9 PID/TL0 does not jump mid-stream (should be
  //                 around M59).
  bool DetectVp9Tl0PicIdxJump(int fixed_tl0, uint32_t timestamp) const
      EXCLUSIVE_LOCKS_REQUIRED(crit_);

  // For every group of pictures, hold two sequence numbers. The first being
  // the sequence number of the last packet of the last completed frame, and
  // the second being the sequence number of the last packet of the last
  // completed frame advanced by any potential continuous packets of padding.
  std::map<uint16_t,
           std::pair<uint16_t, uint16_t>,
           DescendingSeqNumComp<uint16_t>>
      last_seq_num_gop_ GUARDED_BY(crit_);

  // Save the last picture id in order to detect when there is a gap in frames
  // that have not yet been fully received.
  int last_picture_id_ GUARDED_BY(crit_);

  // Padding packets that have been received but that are not yet continuous
  // with any group of pictures.
  std::set<uint16_t, DescendingSeqNumComp<uint16_t>> stashed_padding_
      GUARDED_BY(crit_);

  // The last unwrapped picture id. Used to unwrap the picture id from a length
  // of |kPicIdLength| to 16 bits.
  int last_unwrap_ GUARDED_BY(crit_);

  // Frames earlier than the last received frame that have not yet been
  // fully received.
  std::set<uint16_t, DescendingSeqNumComp<uint16_t, kPicIdLength>>
      not_yet_received_frames_ GUARDED_BY(crit_);

  // Frames that have been fully received but didn't have all the information
  // needed to determine their references.
  std::deque<std::unique_ptr<RtpFrameObject>> stashed_frames_ GUARDED_BY(crit_);

  // Holds the information about the last completed frame for a given temporal
  // layer given a Tl0 picture index.
  std::map<uint8_t,
           std::array<int16_t, kMaxTemporalLayers>,
           DescendingSeqNumComp<uint8_t>>
      layer_info_ GUARDED_BY(crit_);

  // Where the current scalability structure is in the
  // |scalability_structures_| array.
  uint8_t current_ss_idx_;

  // Holds received scalability structures.
  std::array<GofInfoVP9, kMaxGofSaved> scalability_structures_
      GUARDED_BY(crit_);

  // Holds the the Gof information for a given TL0 picture index.
  std::map<uint8_t, GofInfo, DescendingSeqNumComp<uint8_t>> gof_info_
      GUARDED_BY(crit_);

  // Keep track of which picture id and which temporal layer that had the
  // up switch flag set.
  std::map<uint16_t, uint8_t, DescendingSeqNumComp<uint16_t, kPicIdLength>>
      up_switch_ GUARDED_BY(crit_);

  // For every temporal layer, keep a set of which frames that are missing.
  std::array<std::set<uint16_t, DescendingSeqNumComp<uint16_t, kPicIdLength>>,
             kMaxTemporalLayers>
      missing_frames_for_layer_ GUARDED_BY(crit_);

  // How far frames have been cleared by sequence number. A frame will be
  // cleared if it contains a packet with a sequence number older than
  // |cleared_to_seq_num_|.
  int cleared_to_seq_num_ GUARDED_BY(crit_);

  OnCompleteFrameCallback* frame_callback_;

  // Vp9PidFix variables
  // TODO(philipel): Remove when VP9 PID does not jump mid-stream.
  int vp9_fix_last_timestamp_ = -1;
  int vp9_fix_jump_timestamp_ = -1;
  int vp9_fix_last_picture_id_ = -1;
  int vp9_fix_pid_offset_ = 0;
  int vp9_fix_last_tl0_pic_idx_ = -1;
  int vp9_fix_tl0_pic_idx_offset_ = 0;
};

}  // namespace video_coding
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_RTP_FRAME_REFERENCE_FINDER_H_
