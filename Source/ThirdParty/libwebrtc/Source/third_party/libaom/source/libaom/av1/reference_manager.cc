/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <algorithm>
#include <set>
#include <utility>
#include <tuple>
#include <vector>

#include "av1/reference_manager.h"
#include "av1/ratectrl_qmode.h"

namespace aom {

void RefFrameManager::Reset() {
  free_ref_idx_list_.clear();
  for (int i = 0; i < static_cast<int>(ref_frame_table_.size()); ++i) {
    free_ref_idx_list_.push_back(i);
    ref_frame_table_[i] = GopFrameInvalid();
  }
  forward_stack_.clear();
  backward_queue_.clear();
  last_queue_.clear();
}

int RefFrameManager::AllocateRefIdx() {
  if (free_ref_idx_list_.empty()) {
    size_t backward_size = backward_queue_.size();
    size_t last_size = last_queue_.size();
    if (last_size >= backward_size) {
      int ref_idx = last_queue_.front();
      last_queue_.pop_front();
      free_ref_idx_list_.push_back(ref_idx);
    } else {
      int ref_idx = backward_queue_.front();
      backward_queue_.pop_front();
      free_ref_idx_list_.push_back(ref_idx);
    }
  }

  int ref_idx = free_ref_idx_list_.front();
  free_ref_idx_list_.pop_front();
  return ref_idx;
}

int RefFrameManager::GetRefFrameCountByType(
    RefUpdateType ref_update_type) const {
  size_t cnt = 0;
  switch (ref_update_type) {
    case RefUpdateType::kForward: cnt = forward_stack_.size(); break;
    case RefUpdateType::kBackward: cnt = backward_queue_.size(); break;
    case RefUpdateType::kLast: cnt = last_queue_.size(); break;
    case RefUpdateType::kNone: cnt = 0; break;
  }
  return static_cast<int>(cnt);
}

int RefFrameManager::GetRefFrameCount() const {
  return GetRefFrameCountByType(RefUpdateType::kForward) +
         GetRefFrameCountByType(RefUpdateType::kBackward) +
         GetRefFrameCountByType(RefUpdateType::kLast);
}

// TODO(angiebird): Add unit test.
// Find the ref_idx corresponding to a ref_update_type.
// Return -1 if no ref frame is found.
// The priority_idx indicate closeness between the current frame and
// the ref frame in display order.
// For example, ref_update_type == kForward and priority_idx == 0 means
// find the closest ref frame in forward_stack_.
int RefFrameManager::GetRefFrameIdxByPriority(RefUpdateType ref_update_type,
                                              int priority_idx) const {
  if (ref_update_type == RefUpdateType::kForward) {
    int size = static_cast<int>(forward_stack_.size());
    if (priority_idx < size) {
      return forward_stack_[size - priority_idx - 1];
    }
  } else if (ref_update_type == RefUpdateType::kBackward) {
    int size = static_cast<int>(backward_queue_.size());
    if (priority_idx < size) {
      return backward_queue_[size - priority_idx - 1];
    }
  } else if (ref_update_type == RefUpdateType::kLast) {
    int size = static_cast<int>(last_queue_.size());
    if (priority_idx < size) {
      return last_queue_[size - priority_idx - 1];
    }
  }
  return -1;
}

// The priority_idx indicate closeness between the current frame and
// the ref frame in display order.
// For example, ref_update_type == kForward and priority_idx == 0 means
// find the closest ref frame in forward_stack_.
GopFrame RefFrameManager::GetRefFrameByPriority(RefUpdateType ref_update_type,
                                                int priority_idx) const {
  int ref_idx = GetRefFrameIdxByPriority(ref_update_type, priority_idx);
  if (ref_idx == -1) {
    return GopFrameInvalid();
  }
  assert(ref_frame_table_[ref_idx].update_ref_idx == ref_idx);
  return ref_frame_table_[ref_idx];
}

GopFrame RefFrameManager::GetRefFrameByIndex(int ref_idx) const {
  return ref_frame_table_[ref_idx];
}

ReferenceName get_ref_name(RefUpdateType ref_update_type, int priority_idx,
                           const std::set<ReferenceName> &used_name_set) {
  // TODO(angiebird): Find the better way to assign name lists.
  // Maybe sort the names based on how frequent each name is being used in the
  // past?
  const std::vector<ReferenceName> forward_name_list{
    ReferenceName::kAltrefFrame,  ReferenceName::kBwdrefFrame,
    ReferenceName::kAltref2Frame, ReferenceName::kGoldenFrame,
    ReferenceName::kLastFrame,    ReferenceName::kLast2Frame,
    ReferenceName::kLast3Frame
  };
  const std::vector<ReferenceName> backward_name_list{
    ReferenceName::kGoldenFrame, ReferenceName::kLastFrame,
    ReferenceName::kLast2Frame,  ReferenceName::kLast3Frame,
    ReferenceName::kBwdrefFrame, ReferenceName::kAltref2Frame,
    ReferenceName::kAltrefFrame
  };
  const std::vector<ReferenceName> last_name_list{
    ReferenceName::kLastFrame,   ReferenceName::kLast2Frame,
    ReferenceName::kLast3Frame,  ReferenceName::kGoldenFrame,
    ReferenceName::kBwdrefFrame, ReferenceName::kAltref2Frame,
    ReferenceName::kAltrefFrame
  };

  const std::vector<ReferenceName> *name_list = nullptr;
  switch (ref_update_type) {
    case RefUpdateType::kForward: name_list = &forward_name_list; break;
    case RefUpdateType::kBackward: name_list = &backward_name_list; break;
    case RefUpdateType::kLast: name_list = &last_name_list; break;
    case RefUpdateType::kNone: break;
  }

  if (name_list) {
    const int name_list_size = static_cast<int>(name_list->size());
    for (int idx = priority_idx; idx < name_list_size; ++idx) {
      ReferenceName ref_name = name_list->at(idx);
      bool not_used = used_name_set.find(ref_name) == used_name_set.end();
      if (not_used) return ref_name;
    }
  }
  return ReferenceName::kNoneFrame;
}

// Generate a list of available reference frames in priority order for the
// current to-be-coded frame. The list size should be less or equal to the size
// of ref_frame_table_. The reference frames with smaller indices are more
// likely to be a good reference frame. Therefore, they should be prioritized
// when the reference frame count is limited. For example, if we plan to use 3
// reference frames, we should choose ref_frame_list[0], ref_frame_list[1] and
// ref_frame_list[2].
std::vector<ReferenceFrame> RefFrameManager::GetRefFrameListByPriority() const {
  constexpr int round_robin_size = 3;
  const std::vector<RefUpdateType> round_robin_list{ RefUpdateType::kForward,
                                                     RefUpdateType::kBackward,
                                                     RefUpdateType::kLast };
  std::vector<int> priority_idx_list(round_robin_size, 0);
  int available_ref_frames = GetRefFrameCount();
  std::vector<ReferenceFrame> ref_frame_list;
  int ref_frame_count = 0;
  int round_robin_idx = 0;
  std::set<ReferenceName> used_name_set;
  while (ref_frame_count < available_ref_frames) {
    const RefUpdateType ref_update_type = round_robin_list[round_robin_idx];
    int priority_idx = priority_idx_list[round_robin_idx];
    int ref_idx = GetRefFrameIdxByPriority(ref_update_type, priority_idx);
    if (ref_idx != -1) {
      const ReferenceName name =
          get_ref_name(ref_update_type, priority_idx, used_name_set);
      assert(name != ReferenceName::kNoneFrame);
      used_name_set.insert(name);
      ReferenceFrame ref_frame = { ref_idx, name };
      ref_frame_list.push_back(ref_frame);
      ++ref_frame_count;
      ++priority_idx_list[round_robin_idx];
    }
    round_robin_idx = (round_robin_idx + 1) % round_robin_size;
  }
  return ref_frame_list;
}

void RefFrameManager::UpdateOrder(int global_order_idx) {
  cur_global_order_idx_ = global_order_idx;
  if (forward_stack_.empty()) {
    return;
  }
  int ref_idx = forward_stack_.back();
  const GopFrame &gf_frame = ref_frame_table_[ref_idx];

  // If the current processing frame is an overlay / show existing frame.
  if (gf_frame.global_order_idx == global_order_idx) {
    forward_stack_.pop_back();
    if (gf_frame.is_golden_frame) {
      // high quality frame
      backward_queue_.push_back(ref_idx);
    } else {
      last_queue_.push_back(ref_idx);
    }
  }
}

int RefFrameManager::ColocatedRefIdx(int global_order_idx) {
  if (forward_stack_.empty()) return -1;
  int ref_idx = forward_stack_.back();
  int arf_global_order_idx = ref_frame_table_[ref_idx].global_order_idx;
  if (arf_global_order_idx == global_order_idx) {
    return ref_idx;
  }
  return -1;
}

static RefUpdateType infer_ref_update_type(const GopFrame &gop_frame,
                                           int cur_global_order_idx) {
  if (gop_frame.global_order_idx > cur_global_order_idx) {
    return RefUpdateType::kForward;
  }
  if (gop_frame.is_golden_frame) {
    return RefUpdateType::kBackward;
  }
  if (gop_frame.encode_ref_mode == EncodeRefMode::kShowExisting ||
      gop_frame.encode_ref_mode == EncodeRefMode::kOverlay) {
    return RefUpdateType::kNone;
  }
  return RefUpdateType::kLast;
}

using PrimaryRefKey = std::tuple<int,   // abs layer_depth delta
                                 bool,  // is_key_frame differs
                                 bool,  // is_golden_frame differs
                                 bool,  // is_arf_frame differs
                                 bool,  // is_show_frame differs
                                 bool,  // encode_ref_mode differs
                                 int>;  // abs order_idx delta

// Generate PrimaryRefKey based on abs layer_depth delta,
// frame flags and abs order_idx delta. These are the fields that will
// be used to pick the primary reference frame for probability model
static PrimaryRefKey get_primary_ref_key(const GopFrame &cur_frame,
                                         const GopFrame &ref_frame) {
  return std::make_tuple(abs(cur_frame.layer_depth - ref_frame.layer_depth),
                         cur_frame.is_key_frame != ref_frame.is_key_frame,
                         cur_frame.is_golden_frame != ref_frame.is_golden_frame,
                         cur_frame.is_arf_frame != ref_frame.is_arf_frame,
                         cur_frame.is_show_frame != ref_frame.is_show_frame,
                         cur_frame.encode_ref_mode != ref_frame.encode_ref_mode,
                         abs(cur_frame.order_idx - ref_frame.order_idx));
}

// Pick primary_ref_idx for probability model.
ReferenceFrame RefFrameManager::GetPrimaryRefFrame(
    const GopFrame &gop_frame) const {
  assert(gop_frame.is_valid);
  std::vector<std::pair<PrimaryRefKey, int>> candidate_list;
  for (int ref_idx = 0; ref_idx < static_cast<int>(ref_frame_table_.size());
       ++ref_idx) {
    const GopFrame &ref_frame = ref_frame_table_[ref_idx];
    if (ref_frame.is_valid) {
      assert(ref_idx == ref_frame.update_ref_idx);
      PrimaryRefKey key = get_primary_ref_key(gop_frame, ref_frame);
      std::pair<PrimaryRefKey, int> candidate = { key, ref_idx };
      candidate_list.push_back(candidate);
    }
  }

  std::sort(candidate_list.begin(), candidate_list.end());

  ReferenceFrame ref_frame = { -1, ReferenceName::kNoneFrame };
  std::vector<ReferenceFrame> ref_frame_list = GetRefFrameListByPriority();
  assert(candidate_list.size() == ref_frame_list.size());
  if (!candidate_list.empty()) {
    int ref_idx = candidate_list[0].second;
    for (const auto &frame : ref_frame_list) {
      if (frame.index == ref_idx) {
        ref_frame = frame;
      }
    }
  }
  return ref_frame;
}

void RefFrameManager::UpdateRefFrameTable(GopFrame *gop_frame) {
  gop_frame->ref_frame_list = GetRefFrameListByPriority();
  gop_frame->primary_ref_frame = GetPrimaryRefFrame(*gop_frame);
  gop_frame->colocated_ref_idx = ColocatedRefIdx(gop_frame->global_order_idx);

  if (gop_frame->is_show_frame) {
    UpdateOrder(gop_frame->global_order_idx);
  }
  // Call infer_ref_update_type() after UpdateOrder() so that
  // cur_global_order_idx_ is up-to-date
  RefUpdateType ref_update_type =
      infer_ref_update_type(*gop_frame, cur_global_order_idx_);
  if (ref_update_type == RefUpdateType::kNone) {
    gop_frame->update_ref_idx = -1;
  } else {
    const int ref_idx = AllocateRefIdx();
    gop_frame->update_ref_idx = ref_idx;
    switch (ref_update_type) {
      case RefUpdateType::kForward: forward_stack_.push_back(ref_idx); break;
      case RefUpdateType::kBackward: backward_queue_.push_back(ref_idx); break;
      case RefUpdateType::kLast: last_queue_.push_back(ref_idx); break;
      case RefUpdateType::kNone: break;
    }
    ref_frame_table_[ref_idx] = *gop_frame;
  }
}

}  // namespace aom
