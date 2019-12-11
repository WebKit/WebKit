/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/loss_notification_controller.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {
// Keep a container's size no higher than |max_allowed_size|, by paring its size
// down to |target_size| whenever it has more than |max_allowed_size| elements.
template <typename Container>
void PareDown(Container* container,
              size_t max_allowed_size,
              size_t target_size) {
  if (container->size() > max_allowed_size) {
    const size_t entries_to_delete = container->size() - target_size;
    auto erase_to = container->begin();
    std::advance(erase_to, entries_to_delete);
    container->erase(container->begin(), erase_to);
    RTC_DCHECK_EQ(container->size(), target_size);
  }
}
}  // namespace

LossNotificationController::LossNotificationController(
    KeyFrameRequestSender* key_frame_request_sender,
    LossNotificationSender* loss_notification_sender)
    : key_frame_request_sender_(key_frame_request_sender),
      loss_notification_sender_(loss_notification_sender),
      current_frame_potentially_decodable_(true) {
  RTC_DCHECK(key_frame_request_sender_);
  RTC_DCHECK(loss_notification_sender_);
}

LossNotificationController::~LossNotificationController() = default;

void LossNotificationController::OnReceivedPacket(const VCMPacket& packet) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  if (!packet.generic_descriptor) {
    RTC_LOG(LS_WARNING) << "Generic frame descriptor missing. Buggy remote? "
                           "Misconfigured local?";
    return;
  }

  // Ignore repeated or reordered packets.
  // TODO(bugs.webrtc.org/10336): Handle packet reordering.
  if (last_received_seq_num_ &&
      !AheadOf(packet.seqNum, *last_received_seq_num_)) {
    return;
  }

  DiscardOldInformation();  // Prevent memory overconsumption.

  const bool seq_num_gap =
      last_received_seq_num_ &&
      packet.seqNum != static_cast<uint16_t>(*last_received_seq_num_ + 1u);

  last_received_seq_num_ = packet.seqNum;

  if (packet.generic_descriptor->FirstPacketInSubFrame()) {
    const uint16_t frame_id = packet.generic_descriptor->FrameId();
    const int64_t unwrapped_frame_id = frame_id_unwrapper_.Unwrap(frame_id);

    // Ignore repeated or reordered frames.
    // TODO(TODO(bugs.webrtc.org/10336): Handle frame reordering.
    if (last_received_unwrapped_frame_id_ &&
        unwrapped_frame_id <= *last_received_unwrapped_frame_id_) {
      RTC_LOG(LS_WARNING) << "Repeated or reordered frame ID (" << frame_id
                          << ").";
      return;
    }

    last_received_unwrapped_frame_id_ = unwrapped_frame_id;

    const bool intra_frame =
        packet.generic_descriptor->FrameDependenciesDiffs().empty();
    // Generic Frame Descriptor does not current allow us to distinguish
    // whether an intra frame is a key frame.
    // We therefore assume all intra frames are key frames.
    const bool key_frame = intra_frame;
    if (key_frame) {
      // Subsequent frames may not rely on frames before the key frame.
      // Note that upon receiving a key frame, we do not issue a loss
      // notification on RTP sequence number gap, unless that gap spanned
      // the key frame itself. This is because any loss which occurred before
      // the key frame is no longer relevant.
      decodable_unwrapped_frame_ids_.clear();
      current_frame_potentially_decodable_ = true;
    } else {
      const bool all_dependencies_decodable = AllDependenciesDecodable(
          unwrapped_frame_id,
          packet.generic_descriptor->FrameDependenciesDiffs());
      current_frame_potentially_decodable_ = all_dependencies_decodable;
      if (seq_num_gap || !current_frame_potentially_decodable_) {
        HandleLoss(packet.seqNum, current_frame_potentially_decodable_);
      }
    }
  } else if (seq_num_gap || !current_frame_potentially_decodable_) {
    current_frame_potentially_decodable_ = false;
    // We allow sending multiple loss notifications for a single frame
    // even if only one of its packets is lost. We do this because the bigger
    // the frame, the more likely it is to be non-discardable, and therefore
    // the more robust we wish to be to loss of the feedback messages.
    HandleLoss(packet.seqNum, false);
  }
}

void LossNotificationController::OnAssembledFrame(
    uint16_t first_seq_num,
    uint16_t frame_id,
    bool discardable,
    rtc::ArrayView<const uint16_t> frame_dependency_diffs) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  DiscardOldInformation();  // Prevent memory overconsumption.

  if (discardable) {
    return;
  }

  const int64_t unwrapped_frame_id = frame_id_unwrapper_.Unwrap(frame_id);
  if (!AllDependenciesDecodable(unwrapped_frame_id, frame_dependency_diffs)) {
    return;
  }

  last_decodable_non_discardable_.emplace(first_seq_num);
  const auto it = decodable_unwrapped_frame_ids_.insert(unwrapped_frame_id);
  RTC_DCHECK(it.second);
}

void LossNotificationController::DiscardOldInformation() {
  constexpr size_t kExpectedKeyFrameIntervalFrames = 3000;
  constexpr size_t kMaxSize = 2 * kExpectedKeyFrameIntervalFrames;
  constexpr size_t kTargetSize = kExpectedKeyFrameIntervalFrames;
  PareDown(&decodable_unwrapped_frame_ids_, kMaxSize, kTargetSize);
}

bool LossNotificationController::AllDependenciesDecodable(
    int64_t unwrapped_frame_id,
    rtc::ArrayView<const uint16_t> frame_dependency_diffs) const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  // Due to packet reordering, frame buffering and asynchronous decoders, it is
  // infeasible to make reliable conclusions on the decodability of a frame
  // immediately when it arrives. We use the following assumptions:
  // * Intra frames are decodable.
  // * Inter frames are decodable if all of their references were decodable.
  // One possibility that is ignored, is that the packet may be corrupt.

  for (uint16_t frame_dependency_diff : frame_dependency_diffs) {
    const int64_t unwrapped_ref_frame_id =
        unwrapped_frame_id - frame_dependency_diff;

    const auto ref_frame_it =
        decodable_unwrapped_frame_ids_.find(unwrapped_ref_frame_id);
    if (ref_frame_it == decodable_unwrapped_frame_ids_.end()) {
      // Reference frame not decodable.
      return false;
    }
  }

  return true;
}

void LossNotificationController::HandleLoss(uint16_t last_received_seq_num,
                                            bool decodability_flag) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  if (last_decodable_non_discardable_) {
    RTC_DCHECK(AheadOf(last_received_seq_num,
                       last_decodable_non_discardable_->first_seq_num));
    loss_notification_sender_->SendLossNotification(
        last_decodable_non_discardable_->first_seq_num, last_received_seq_num,
        decodability_flag, /*buffering_allowed=*/true);
  } else {
    key_frame_request_sender_->RequestKeyFrame();
  }
}
}  //  namespace webrtc
