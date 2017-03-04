/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/rtp_frame_reference_finder.h"

#include <algorithm>
#include <limits>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/video_coding/frame_object.h"
#include "webrtc/modules/video_coding/packet_buffer.h"

namespace webrtc {
namespace video_coding {

RtpFrameReferenceFinder::RtpFrameReferenceFinder(
    OnCompleteFrameCallback* frame_callback)
    : last_picture_id_(-1),
      last_unwrap_(-1),
      current_ss_idx_(0),
      cleared_to_seq_num_(-1),
      frame_callback_(frame_callback) {}

void RtpFrameReferenceFinder::ManageFrame(
    std::unique_ptr<RtpFrameObject> frame) {
  rtc::CritScope lock(&crit_);

  // If we have cleared past this frame, drop it.
  if (cleared_to_seq_num_ != -1 &&
      AheadOf<uint16_t>(cleared_to_seq_num_, frame->first_seq_num())) {
    return;
  }

  switch (frame->codec_type()) {
    case kVideoCodecFlexfec:
    case kVideoCodecULPFEC:
    case kVideoCodecRED:
      RTC_NOTREACHED();
      break;
    case kVideoCodecVP8:
      ManageFrameVp8(std::move(frame));
      break;
    case kVideoCodecVP9:
      ManageFrameVp9(std::move(frame));
      break;
    // Since the EndToEndTests use kVicdeoCodecUnknow we treat it the same as
    // kVideoCodecGeneric.
    // TODO(philipel): Take a look at the EndToEndTests and see if maybe they
    //                 should be changed to use kVideoCodecGeneric instead.
    case kVideoCodecUnknown:
    case kVideoCodecH264:
    case kVideoCodecI420:
    case kVideoCodecGeneric:
      ManageFrameGeneric(std::move(frame), kNoPictureId);
      break;
  }
}

void RtpFrameReferenceFinder::PaddingReceived(uint16_t seq_num) {
  rtc::CritScope lock(&crit_);
  auto clean_padding_to =
      stashed_padding_.lower_bound(seq_num - kMaxPaddingAge);
  stashed_padding_.erase(stashed_padding_.begin(), clean_padding_to);
  stashed_padding_.insert(seq_num);
  UpdateLastPictureIdWithPadding(seq_num);
  RetryStashedFrames();
}

void RtpFrameReferenceFinder::ClearTo(uint16_t seq_num) {
  rtc::CritScope lock(&crit_);
  cleared_to_seq_num_ = seq_num;

  auto it = stashed_frames_.begin();
  while (it != stashed_frames_.end()) {
    if (AheadOf<uint16_t>(cleared_to_seq_num_, (*it)->first_seq_num())) {
      it = stashed_frames_.erase(it);
    } else {
      ++it;
    }
  }
}

void RtpFrameReferenceFinder::UpdateLastPictureIdWithPadding(uint16_t seq_num) {
  auto gop_seq_num_it = last_seq_num_gop_.upper_bound(seq_num);

  // If this padding packet "belongs" to a group of pictures that we don't track
  // anymore, do nothing.
  if (gop_seq_num_it == last_seq_num_gop_.begin())
    return;
  --gop_seq_num_it;

  // Calculate the next contiuous sequence number and search for it in
  // the padding packets we have stashed.
  uint16_t next_seq_num_with_padding = gop_seq_num_it->second.second + 1;
  auto padding_seq_num_it =
      stashed_padding_.lower_bound(next_seq_num_with_padding);

  // While there still are padding packets and those padding packets are
  // continuous, then advance the "last-picture-id-with-padding" and remove
  // the stashed padding packet.
  while (padding_seq_num_it != stashed_padding_.end() &&
         *padding_seq_num_it == next_seq_num_with_padding) {
    gop_seq_num_it->second.second = next_seq_num_with_padding;
    ++next_seq_num_with_padding;
    padding_seq_num_it = stashed_padding_.erase(padding_seq_num_it);
  }

  // In the case where the stream has been continuous without any new keyframes
  // for a while there is a risk that new frames will appear to be older than
  // the keyframe they belong to due to wrapping sequence number. In order
  // to prevent this we advance the picture id of the keyframe every so often.
  if (ForwardDiff(gop_seq_num_it->first, seq_num) > 10000) {
    RTC_DCHECK_EQ(1ul, last_seq_num_gop_.size());
    last_seq_num_gop_[seq_num] = gop_seq_num_it->second;
    last_seq_num_gop_.erase(gop_seq_num_it);
  }
}

void RtpFrameReferenceFinder::RetryStashedFrames() {
  size_t num_stashed_frames = stashed_frames_.size();

  // Clean up stashed frames if there are too many.
  while (stashed_frames_.size() > kMaxStashedFrames)
    stashed_frames_.pop_front();

  // Since frames are stashed if there is not enough data to determine their
  // frame references we should at most check |stashed_frames_.size()| in
  // order to not pop and push frames in and endless loop.
  // NOTE! This function may be called recursively, hence the
  //       "!stashed_frames_.empty()" condition.
  for (size_t i = 0; i < num_stashed_frames && !stashed_frames_.empty(); ++i) {
    std::unique_ptr<RtpFrameObject> frame = std::move(stashed_frames_.front());
    stashed_frames_.pop_front();
    ManageFrame(std::move(frame));
  }
}

void RtpFrameReferenceFinder::ManageFrameGeneric(
    std::unique_ptr<RtpFrameObject> frame,
    int picture_id) {
  // If |picture_id| is specified then we use that to set the frame references,
  // otherwise we use sequence number.
  if (picture_id != kNoPictureId) {
    if (last_unwrap_ == -1)
      last_unwrap_ = picture_id;

    frame->picture_id = UnwrapPictureId(picture_id % kPicIdLength);
    frame->num_references = frame->frame_type() == kVideoFrameKey ? 0 : 1;
    frame->references[0] = frame->picture_id - 1;
    frame_callback_->OnCompleteFrame(std::move(frame));
    return;
  }

  if (frame->frame_type() == kVideoFrameKey) {
    last_seq_num_gop_.insert(std::make_pair(
        frame->last_seq_num(),
        std::make_pair(frame->last_seq_num(), frame->last_seq_num())));
  }

  // We have received a frame but not yet a keyframe, stash this frame.
  if (last_seq_num_gop_.empty()) {
    stashed_frames_.push_back(std::move(frame));
    return;
  }

  // Clean up info for old keyframes but make sure to keep info
  // for the last keyframe.
  auto clean_to = last_seq_num_gop_.lower_bound(frame->last_seq_num() - 100);
  for (auto it = last_seq_num_gop_.begin();
       it != clean_to && last_seq_num_gop_.size() > 1;) {
    it = last_seq_num_gop_.erase(it);
  }

  // Find the last sequence number of the last frame for the keyframe
  // that this frame indirectly references.
  auto seq_num_it = last_seq_num_gop_.upper_bound(frame->last_seq_num());
  if (seq_num_it == last_seq_num_gop_.begin()) {
    LOG(LS_WARNING) << "Generic frame with packet range ["
                    << frame->first_seq_num() << ", " << frame->last_seq_num()
                    << "] has no GoP, dropping frame.";
    return;
  }
  seq_num_it--;

  // Make sure the packet sequence numbers are continuous, otherwise stash
  // this frame.
  uint16_t last_picture_id_gop = seq_num_it->second.first;
  uint16_t last_picture_id_with_padding_gop = seq_num_it->second.second;
  if (frame->frame_type() == kVideoFrameDelta) {
    uint16_t prev_seq_num = frame->first_seq_num() - 1;
    if (prev_seq_num != last_picture_id_with_padding_gop) {
      stashed_frames_.push_back(std::move(frame));
      return;
    }
  }

  RTC_DCHECK(AheadOrAt(frame->last_seq_num(), seq_num_it->first));

  // Since keyframes can cause reordering we can't simply assign the
  // picture id according to some incrementing counter.
  frame->picture_id = frame->last_seq_num();
  frame->num_references = frame->frame_type() == kVideoFrameDelta;
  frame->references[0] = last_picture_id_gop;
  if (AheadOf(frame->picture_id, last_picture_id_gop)) {
    seq_num_it->second.first = frame->picture_id;
    seq_num_it->second.second = frame->picture_id;
  }

  last_picture_id_ = frame->picture_id;
  UpdateLastPictureIdWithPadding(frame->picture_id);
  frame_callback_->OnCompleteFrame(std::move(frame));
  RetryStashedFrames();
}

void RtpFrameReferenceFinder::ManageFrameVp8(
    std::unique_ptr<RtpFrameObject> frame) {
  rtc::Optional<RTPVideoTypeHeader> rtp_codec_header = frame->GetCodecHeader();
  if (!rtp_codec_header)
    return;

  const RTPVideoHeaderVP8& codec_header = rtp_codec_header->VP8;

  if (codec_header.pictureId == kNoPictureId ||
      codec_header.temporalIdx == kNoTemporalIdx ||
      codec_header.tl0PicIdx == kNoTl0PicIdx) {
    ManageFrameGeneric(std::move(frame), codec_header.pictureId);
    return;
  }

  frame->picture_id = codec_header.pictureId % kPicIdLength;

  if (last_unwrap_ == -1)
    last_unwrap_ = codec_header.pictureId;

  if (last_picture_id_ == -1)
    last_picture_id_ = frame->picture_id;

  // Find if there has been a gap in fully received frames and save the picture
  // id of those frames in |not_yet_received_frames_|.
  if (AheadOf<uint16_t, kPicIdLength>(frame->picture_id, last_picture_id_)) {
    last_picture_id_ = Add<kPicIdLength>(last_picture_id_, 1);
    while (last_picture_id_ != frame->picture_id) {
      not_yet_received_frames_.insert(last_picture_id_);
      last_picture_id_ = Add<kPicIdLength>(last_picture_id_, 1);
    }
  }

  // Clean up info for base layers that are too old.
  uint8_t old_tl0_pic_idx = codec_header.tl0PicIdx - kMaxLayerInfo;
  auto clean_layer_info_to = layer_info_.lower_bound(old_tl0_pic_idx);
  layer_info_.erase(layer_info_.begin(), clean_layer_info_to);

  // Clean up info about not yet received frames that are too old.
  uint16_t old_picture_id =
      Subtract<kPicIdLength>(frame->picture_id, kMaxNotYetReceivedFrames);
  auto clean_frames_to = not_yet_received_frames_.lower_bound(old_picture_id);
  not_yet_received_frames_.erase(not_yet_received_frames_.begin(),
                                 clean_frames_to);

  if (frame->frame_type() == kVideoFrameKey) {
    frame->num_references = 0;
    layer_info_[codec_header.tl0PicIdx].fill(-1);
    CompletedFrameVp8(std::move(frame));
    return;
  }

  auto layer_info_it = layer_info_.find(codec_header.temporalIdx == 0
                                            ? codec_header.tl0PicIdx - 1
                                            : codec_header.tl0PicIdx);

  // If we don't have the base layer frame yet, stash this frame.
  if (layer_info_it == layer_info_.end()) {
    stashed_frames_.push_back(std::move(frame));
    return;
  }

  // A non keyframe base layer frame has been received, copy the layer info
  // from the previous base layer frame and set a reference to the previous
  // base layer frame.
  if (codec_header.temporalIdx == 0) {
    layer_info_it =
        layer_info_
            .insert(make_pair(codec_header.tl0PicIdx, layer_info_it->second))
            .first;
    frame->num_references = 1;
    frame->references[0] = layer_info_it->second[0];
    CompletedFrameVp8(std::move(frame));
    return;
  }

  // Layer sync frame, this frame only references its base layer frame.
  if (codec_header.layerSync) {
    frame->num_references = 1;
    frame->references[0] = layer_info_it->second[0];

    CompletedFrameVp8(std::move(frame));
    return;
  }

  // Find all references for this frame.
  frame->num_references = 0;
  for (uint8_t layer = 0; layer <= codec_header.temporalIdx; ++layer) {
    // If we have not yet received a previous frame on this temporal layer,
    // stash this frame.
    if (layer_info_it->second[layer] == -1) {
      stashed_frames_.push_back(std::move(frame));
      return;
    }

    // If the last frame on this layer is ahead of this frame it means that
    // a layer sync frame has been received after this frame for the same
    // base layer frame, drop this frame.
    if (AheadOf<uint16_t, kPicIdLength>(layer_info_it->second[layer],
                                        frame->picture_id)) {
      return;
    }

    // If we have not yet received a frame between this frame and the referenced
    // frame then we have to wait for that frame to be completed first.
    auto not_received_frame_it =
        not_yet_received_frames_.upper_bound(layer_info_it->second[layer]);
    if (not_received_frame_it != not_yet_received_frames_.end() &&
        AheadOf<uint16_t, kPicIdLength>(frame->picture_id,
                                        *not_received_frame_it)) {
      stashed_frames_.push_back(std::move(frame));
      return;
    }

    RTC_DCHECK((AheadOf<uint16_t, kPicIdLength>(frame->picture_id,
                                               layer_info_it->second[layer])));
    ++frame->num_references;
    frame->references[layer] = layer_info_it->second[layer];
  }

  CompletedFrameVp8(std::move(frame));
}

void RtpFrameReferenceFinder::CompletedFrameVp8(
    std::unique_ptr<RtpFrameObject> frame) {
  rtc::Optional<RTPVideoTypeHeader> rtp_codec_header = frame->GetCodecHeader();
  if (!rtp_codec_header)
    return;

  const RTPVideoHeaderVP8& codec_header = rtp_codec_header->VP8;

  uint8_t tl0_pic_idx = codec_header.tl0PicIdx;
  uint8_t temporal_index = codec_header.temporalIdx;
  auto layer_info_it = layer_info_.find(tl0_pic_idx);

  // Update this layer info and newer.
  while (layer_info_it != layer_info_.end()) {
    if (layer_info_it->second[temporal_index] != -1 &&
        AheadOf<uint16_t, kPicIdLength>(layer_info_it->second[temporal_index],
                                        frame->picture_id)) {
      // The frame was not newer, then no subsequent layer info have to be
      // update.
      break;
    }

    layer_info_it->second[codec_header.temporalIdx] = frame->picture_id;
    ++tl0_pic_idx;
    layer_info_it = layer_info_.find(tl0_pic_idx);
  }
  not_yet_received_frames_.erase(frame->picture_id);

  for (size_t i = 0; i < frame->num_references; ++i)
    frame->references[i] = UnwrapPictureId(frame->references[i]);
  frame->picture_id = UnwrapPictureId(frame->picture_id);

  frame_callback_->OnCompleteFrame(std::move(frame));
  RetryStashedFrames();
}

void RtpFrameReferenceFinder::ManageFrameVp9(
    std::unique_ptr<RtpFrameObject> frame) {
  rtc::Optional<RTPVideoTypeHeader> rtp_codec_header = frame->GetCodecHeader();
  if (!rtp_codec_header)
    return;

  const RTPVideoHeaderVP9& codec_header = rtp_codec_header->VP9;

  bool old_frame = Vp9PidTl0Fix(*frame, &rtp_codec_header->VP9.picture_id,
                                &rtp_codec_header->VP9.tl0_pic_idx);
  if (old_frame)
    return;

  if (codec_header.picture_id == kNoPictureId ||
      codec_header.temporal_idx == kNoTemporalIdx) {
    ManageFrameGeneric(std::move(frame), codec_header.picture_id);
    return;
  }

  frame->spatial_layer = codec_header.spatial_idx;
  frame->inter_layer_predicted = codec_header.inter_layer_predicted;
  frame->picture_id = codec_header.picture_id % kPicIdLength;

  if (last_unwrap_ == -1)
    last_unwrap_ = codec_header.picture_id;

  if (last_picture_id_ == -1)
    last_picture_id_ = frame->picture_id;

  if (codec_header.flexible_mode) {
    frame->num_references = codec_header.num_ref_pics;
    for (size_t i = 0; i < frame->num_references; ++i) {
      frame->references[i] =
          Subtract<1 << 16>(frame->picture_id, codec_header.pid_diff[i]);
    }

    CompletedFrameVp9(std::move(frame));
    return;
  }

  if (codec_header.ss_data_available) {
    // Scalability structures can only be sent with tl0 frames.
    if (codec_header.temporal_idx != 0) {
      LOG(LS_WARNING) << "Received scalability structure on a non base layer"
                         " frame. Scalability structure ignored.";
    } else {
      current_ss_idx_ = Add<kMaxGofSaved>(current_ss_idx_, 1);
      scalability_structures_[current_ss_idx_] = codec_header.gof;
      scalability_structures_[current_ss_idx_].pid_start = frame->picture_id;

      GofInfo info(&scalability_structures_[current_ss_idx_],
                   frame->picture_id);
      gof_info_.insert(std::make_pair(codec_header.tl0_pic_idx, info));
    }
  }

  // Clean up info for base layers that are too old.
  uint8_t old_tl0_pic_idx = codec_header.tl0_pic_idx - kMaxGofSaved;
  auto clean_gof_info_to = gof_info_.lower_bound(old_tl0_pic_idx);
  gof_info_.erase(gof_info_.begin(), clean_gof_info_to);

  if (frame->frame_type() == kVideoFrameKey) {
    // When using GOF all keyframes must include the scalability structure.
    if (!codec_header.ss_data_available)
      LOG(LS_WARNING) << "Received keyframe without scalability structure";

    frame->num_references = 0;
    GofInfo info = gof_info_.find(codec_header.tl0_pic_idx)->second;
    FrameReceivedVp9(frame->picture_id, &info);
    CompletedFrameVp9(std::move(frame));
    return;
  }

  auto gof_info_it = gof_info_.find(
      (codec_header.temporal_idx == 0 && !codec_header.ss_data_available)
          ? codec_header.tl0_pic_idx - 1
          : codec_header.tl0_pic_idx);

  // Gof info for this frame is not available yet, stash this frame.
  if (gof_info_it == gof_info_.end()) {
    stashed_frames_.push_back(std::move(frame));
    return;
  }

  GofInfo* info = &gof_info_it->second;
  FrameReceivedVp9(frame->picture_id, info);

  // Make sure we don't miss any frame that could potentially have the
  // up switch flag set.
  if (MissingRequiredFrameVp9(frame->picture_id, *info)) {
    stashed_frames_.push_back(std::move(frame));
    return;
  }

  if (codec_header.temporal_up_switch) {
    auto pid_tidx =
        std::make_pair(frame->picture_id, codec_header.temporal_idx);
    up_switch_.insert(pid_tidx);
  }

  // If this is a base layer frame that contains a scalability structure
  // then gof info has already been inserted earlier, so we only want to
  // insert if we haven't done so already.
  if (codec_header.temporal_idx == 0 && !codec_header.ss_data_available) {
    GofInfo new_info(info->gof, frame->picture_id);
    gof_info_.insert(std::make_pair(codec_header.tl0_pic_idx, new_info));
  }

  // Clean out old info about up switch frames.
  uint16_t old_picture_id = Subtract<kPicIdLength>(frame->picture_id, 50);
  auto up_switch_erase_to = up_switch_.lower_bound(old_picture_id);
  up_switch_.erase(up_switch_.begin(), up_switch_erase_to);

  size_t diff = ForwardDiff<uint16_t, kPicIdLength>(info->gof->pid_start,
                                                    frame->picture_id);
  size_t gof_idx = diff % info->gof->num_frames_in_gof;

  // Populate references according to the scalability structure.
  frame->num_references = info->gof->num_ref_pics[gof_idx];
  for (size_t i = 0; i < frame->num_references; ++i) {
    frame->references[i] = Subtract<kPicIdLength>(
        frame->picture_id, info->gof->pid_diff[gof_idx][i]);

    // If this is a reference to a frame earlier than the last up switch point,
    // then ignore this reference.
    if (UpSwitchInIntervalVp9(frame->picture_id, codec_header.temporal_idx,
                              frame->references[i])) {
      --frame->num_references;
    }
  }

  CompletedFrameVp9(std::move(frame));
}

bool RtpFrameReferenceFinder::MissingRequiredFrameVp9(uint16_t picture_id,
                                                      const GofInfo& info) {
  size_t diff =
      ForwardDiff<uint16_t, kPicIdLength>(info.gof->pid_start, picture_id);
  size_t gof_idx = diff % info.gof->num_frames_in_gof;
  size_t temporal_idx = info.gof->temporal_idx[gof_idx];

  // For every reference this frame has, check if there is a frame missing in
  // the interval (|ref_pid|, |picture_id|) in any of the lower temporal
  // layers. If so, we are missing a required frame.
  uint8_t num_references = info.gof->num_ref_pics[gof_idx];
  for (size_t i = 0; i < num_references; ++i) {
    uint16_t ref_pid =
        Subtract<kPicIdLength>(picture_id, info.gof->pid_diff[gof_idx][i]);
    for (size_t l = 0; l < temporal_idx; ++l) {
      auto missing_frame_it = missing_frames_for_layer_[l].lower_bound(ref_pid);
      if (missing_frame_it != missing_frames_for_layer_[l].end() &&
          AheadOf<uint16_t, kPicIdLength>(picture_id, *missing_frame_it)) {
        return true;
      }
    }
  }
  return false;
}

void RtpFrameReferenceFinder::FrameReceivedVp9(uint16_t picture_id,
                                               GofInfo* info) {
  int last_picture_id = info->last_picture_id;

  // If there is a gap, find which temporal layer the missing frames
  // belong to and add the frame as missing for that temporal layer.
  // Otherwise, remove this frame from the set of missing frames.
  if (AheadOf<uint16_t, kPicIdLength>(picture_id, last_picture_id)) {
    size_t diff = ForwardDiff<uint16_t, kPicIdLength>(info->gof->pid_start,
                                                      last_picture_id);
    size_t gof_idx = diff % info->gof->num_frames_in_gof;

    last_picture_id = Add<kPicIdLength>(last_picture_id, 1);
    while (last_picture_id != picture_id) {
      ++gof_idx;
      RTC_DCHECK_NE(0ul, gof_idx % info->gof->num_frames_in_gof);
      size_t temporal_idx = info->gof->temporal_idx[gof_idx];
      missing_frames_for_layer_[temporal_idx].insert(last_picture_id);
      last_picture_id = Add<kPicIdLength>(last_picture_id, 1);
    }
    info->last_picture_id = last_picture_id;
  } else {
    size_t diff =
        ForwardDiff<uint16_t, kPicIdLength>(info->gof->pid_start, picture_id);
    size_t gof_idx = diff % info->gof->num_frames_in_gof;
    size_t temporal_idx = info->gof->temporal_idx[gof_idx];
    missing_frames_for_layer_[temporal_idx].erase(picture_id);
  }
}

bool RtpFrameReferenceFinder::UpSwitchInIntervalVp9(uint16_t picture_id,
                                                    uint8_t temporal_idx,
                                                    uint16_t pid_ref) {
  for (auto up_switch_it = up_switch_.upper_bound(pid_ref);
       up_switch_it != up_switch_.end() &&
       AheadOf<uint16_t, kPicIdLength>(picture_id, up_switch_it->first);
       ++up_switch_it) {
    if (up_switch_it->second < temporal_idx)
      return true;
  }

  return false;
}

void RtpFrameReferenceFinder::CompletedFrameVp9(
    std::unique_ptr<RtpFrameObject> frame) {
  for (size_t i = 0; i < frame->num_references; ++i)
    frame->references[i] = UnwrapPictureId(frame->references[i]);
  frame->picture_id = UnwrapPictureId(frame->picture_id);

  frame_callback_->OnCompleteFrame(std::move(frame));
  RetryStashedFrames();
}

uint16_t RtpFrameReferenceFinder::UnwrapPictureId(uint16_t picture_id) {
  RTC_DCHECK_NE(-1, last_unwrap_);

  uint16_t unwrap_truncated = last_unwrap_ % kPicIdLength;
  uint16_t diff = MinDiff<uint16_t, kPicIdLength>(unwrap_truncated, picture_id);

  if (AheadOf<uint16_t, kPicIdLength>(picture_id, unwrap_truncated))
    last_unwrap_ = Add<1 << 16>(last_unwrap_, diff);
  else
    last_unwrap_ = Subtract<1 << 16>(last_unwrap_, diff);

  return last_unwrap_;
}

bool RtpFrameReferenceFinder::Vp9PidTl0Fix(const RtpFrameObject& frame,
                                           int16_t* picture_id,
                                           int16_t* tl0_pic_idx) {
  const int kTl0PicIdLength = 256;
  const uint8_t kMaxPidDiff = 128;

  // We are currently receiving VP9 without PID, nothing to fix.
  if (*picture_id == kNoPictureId)
    return false;

  // If |vp9_fix_jump_timestamp_| != -1 then a jump has occurred recently.
  if (vp9_fix_jump_timestamp_ != -1) {
    // If this frame has a timestamp older than |vp9_fix_jump_timestamp_| then
    // this frame is old (more previous than the frame where we detected the
    // jump) and should be dropped.
    if (AheadOf<uint32_t>(vp9_fix_jump_timestamp_, frame.timestamp))
      return true;

    // After 60 seconds, reset |vp9_fix_jump_timestamp_| in order to not
    // discard old frames when the timestamp wraps.
    int diff_ms =
        ForwardDiff<uint32_t>(vp9_fix_jump_timestamp_, frame.timestamp) / 90;
    if (diff_ms > 60 * 1000)
      vp9_fix_jump_timestamp_ = -1;
  }

  // Update |vp9_fix_last_timestamp_| with the most recent timestamp.
  if (vp9_fix_last_timestamp_ == -1)
    vp9_fix_last_timestamp_ = frame.timestamp;
  if (AheadOf<uint32_t>(frame.timestamp, vp9_fix_last_timestamp_))
    vp9_fix_last_timestamp_ = frame.timestamp;

  uint16_t fixed_pid = Add<kPicIdLength>(*picture_id, vp9_fix_pid_offset_);
  if (vp9_fix_last_picture_id_ == -1)
    vp9_fix_last_picture_id_ = *picture_id;

  int16_t fixed_tl0 = kNoTl0PicIdx;
  if (*tl0_pic_idx != kNoTl0PicIdx) {
    fixed_tl0 = Add<kTl0PicIdLength>(*tl0_pic_idx, vp9_fix_tl0_pic_idx_offset_);
    // Update |vp9_fix_last_tl0_pic_idx_| with the most recent tl0 pic index.
    if (vp9_fix_last_tl0_pic_idx_ == -1)
      vp9_fix_last_tl0_pic_idx_ = *tl0_pic_idx;
    if (AheadOf<uint8_t>(fixed_tl0, vp9_fix_last_tl0_pic_idx_))
      vp9_fix_last_tl0_pic_idx_ = fixed_tl0;
  }

  bool has_jumped = DetectVp9PicIdJump(fixed_pid, fixed_tl0, frame.timestamp);
  if (!has_jumped)
    has_jumped = DetectVp9Tl0PicIdxJump(fixed_tl0, frame.timestamp);

  if (has_jumped) {
    // First we calculate the offset to get to the previous picture id, and then
    // we add kMaxPid to avoid accidently referencing any previous
    // frames that was inserted into the FrameBuffer.
    vp9_fix_pid_offset_ = ForwardDiff<uint16_t, kPicIdLength>(
        *picture_id, vp9_fix_last_picture_id_);
    vp9_fix_pid_offset_ += kMaxPidDiff;

    fixed_pid = Add<kPicIdLength>(*picture_id, vp9_fix_pid_offset_);
    vp9_fix_last_picture_id_ = fixed_pid;
    vp9_fix_jump_timestamp_ = frame.timestamp;
    gof_info_.clear();

    vp9_fix_tl0_pic_idx_offset_ =
        ForwardDiff<uint8_t>(*tl0_pic_idx, vp9_fix_last_tl0_pic_idx_);
    vp9_fix_tl0_pic_idx_offset_ += kMaxGofSaved;
    fixed_tl0 = Add<kTl0PicIdLength>(*tl0_pic_idx, vp9_fix_tl0_pic_idx_offset_);
    vp9_fix_last_tl0_pic_idx_ = fixed_tl0;
  }

  // Update |vp9_fix_last_picture_id_| with the most recent picture id.
  if (AheadOf<uint16_t, kPicIdLength>(fixed_pid, vp9_fix_last_picture_id_))
    vp9_fix_last_picture_id_ = fixed_pid;

  *picture_id = fixed_pid;
  *tl0_pic_idx = fixed_tl0;

  return false;
}

bool RtpFrameReferenceFinder::DetectVp9PicIdJump(int fixed_pid,
                                                 int fixed_tl0,
                                                 uint32_t timestamp) const {
  // Test if there has been a jump backwards in the picture id.
  if (AheadOrAt<uint32_t>(timestamp, vp9_fix_last_timestamp_) &&
      AheadOf<uint16_t, kPicIdLength>(vp9_fix_last_picture_id_, fixed_pid)) {
    return true;
  }

  // Test if we have jumped forward too much. The reason we have to do this
  // is because the FrameBuffer holds history of old frames and inserting
  // frames with a much advanced picture id can result in the frame buffer
  // holding more than half of the interval of picture ids.
  if (AheadOrAt<uint32_t>(timestamp, vp9_fix_last_timestamp_) &&
      ForwardDiff<uint16_t, kPicIdLength>(vp9_fix_last_picture_id_, fixed_pid) >
          128) {
    return true;
  }

  // Special case where the picture id jump forward but not by much and the
  // tl0 jumps to the id of an already saved gof for that id. In order to
  // detect this we check if the picture id span over the length of the GOF.
  if (fixed_tl0 != kNoTl0PicIdx) {
    auto info_it = gof_info_.find(fixed_tl0);
    if (info_it != gof_info_.end()) {
      int last_pid_gof_idx_0 =
          Subtract<kPicIdLength>(info_it->second.last_picture_id,
                                 info_it->second.last_picture_id %
                                     info_it->second.gof->num_frames_in_gof);
      int pif_gof_end = Add<kPicIdLength>(
          last_pid_gof_idx_0, info_it->second.gof->num_frames_in_gof);
      if (AheadOf<uint16_t, kPicIdLength>(fixed_pid, pif_gof_end))
        return true;
    }
  }

  return false;
}

bool RtpFrameReferenceFinder::DetectVp9Tl0PicIdxJump(int fixed_tl0,
                                                     uint32_t timestamp) const {
  if (fixed_tl0 != kNoTl0PicIdx) {
    // Test if there has been a jump backwards in tl0 pic index.
    if (AheadOrAt<uint32_t>(timestamp, vp9_fix_last_timestamp_) &&
        AheadOf<uint8_t>(vp9_fix_last_tl0_pic_idx_, fixed_tl0)) {
      return true;
    }

    // Test if there has been a jump forward. If the jump forward results
    // in the tl0 pic index for this frame to be considered smaller than the
    // smallest item in |gof_info_| then we have jumped forward far enough to
    // wrap.
    if (!gof_info_.empty() &&
        AheadOf<uint8_t>(gof_info_.begin()->first, fixed_tl0)) {
      return true;
    }
  }
  return false;
}

}  // namespace video_coding
}  // namespace webrtc
