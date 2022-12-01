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

#ifndef AOM_AV1_REFERENCE_MANAGER_H_
#define AOM_AV1_REFERENCE_MANAGER_H_

#include <deque>
#include <iostream>
#include <vector>

#include "av1/ratectrl_qmode_interface.h"

namespace aom {

enum class RefUpdateType { kForward, kBackward, kLast, kNone };

class RefFrameManager {
 public:
  explicit RefFrameManager(int ref_frame_table_size)
      : ref_frame_table_(ref_frame_table_size) {
    // forward_max_size_ define max number of arf frames that can exists at
    // the same time. In the other words, it's the max size of forward_stack_.
    // TODO(angiebird): Figure out if this number is optimal.
    forward_max_size_ = ref_frame_table_size - 2;
    cur_global_order_idx_ = 0;
    Reset();
  }
  ~RefFrameManager() = default;

  RefFrameManager(const RefFrameManager &) = delete;
  RefFrameManager &operator=(const RefFrameManager &) = delete;

  friend std::ostream &operator<<(std::ostream &os,
                                  const RefFrameManager &rfm) {
    os << "=" << std::endl;
    os << "forward: ";
    for (const auto &ref_idx : rfm.forward_stack_) {
      os << rfm.ref_frame_table_[ref_idx].order_idx << " ";
    }
    os << std::endl;
    os << "backward: ";
    for (const auto &ref_idx : rfm.backward_queue_) {
      os << rfm.ref_frame_table_[ref_idx].order_idx << " ";
    }
    os << std::endl;
    os << "last: ";
    for (const auto &ref_idx : rfm.last_queue_) {
      os << rfm.ref_frame_table_[ref_idx].order_idx << " ";
    }
    os << std::endl;
    return os;
  }

  void Reset();
  int AllocateRefIdx();
  int GetRefFrameCountByType(RefUpdateType ref_update_type) const;
  int GetRefFrameCount() const;
  std::vector<ReferenceFrame> GetRefFrameListByPriority() const;
  int GetRefFrameIdxByPriority(RefUpdateType ref_update_type,
                               int priority_idx) const;
  GopFrame GetRefFrameByPriority(RefUpdateType ref_update_type,
                                 int priority_idx) const;
  GopFrame GetRefFrameByIndex(int ref_idx) const;
  void UpdateOrder(int global_order_idx);
  int ColocatedRefIdx(int global_order_idx);
  int ForwardMaxSize() const { return forward_max_size_; }
  int CurGlobalOrderIdx() const { return cur_global_order_idx_; }
  void UpdateRefFrameTable(GopFrame *gop_frame);
  ReferenceFrame GetPrimaryRefFrame(const GopFrame &gop_frame) const;

 private:
  int forward_max_size_;
  int cur_global_order_idx_;
  RefFrameTable ref_frame_table_;
  std::deque<int> free_ref_idx_list_;
  std::vector<int> forward_stack_;
  std::deque<int> backward_queue_;
  std::deque<int> last_queue_;
};

}  // namespace aom

#endif  // AOM_AV1_REFERENCE_MANAGER_H_
