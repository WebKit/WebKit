/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Based on the Quic implementation in Chromium.

#ifndef MODULES_CONGESTION_CONTROLLER_BBR_PACKET_NUMBER_INDEXED_QUEUE_H_
#define MODULES_CONGESTION_CONTROLLER_BBR_PACKET_NUMBER_INDEXED_QUEUE_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <type_traits>
#include <utility>

#include "rtc_base/checks.h"

namespace webrtc {
namespace bbr {

// PacketNumberIndexedQueue is a queue of mostly continuous numbered entries
// which supports the following operations:
// - adding elements to the end of the queue, or at some point past the end
// - removing elements in any order
// - retrieving elements
// If all elements are inserted in order, all of the operations above are
// amortized O(1) time.
//
// Internally, the data structure is a deque where each element is marked as
// present or not.  The deque starts at the lowest present index.  Whenever an
// element is removed, it's marked as not present, and the front of the deque is
// cleared of elements that are not present.
//
// The tail of the queue is not cleared due to the assumption of entries being
// inserted in order, though removing all elements of the queue will return it
// to its initial state.
//
// Note that this data structure is inherently hazardous, since an addition of
// just two entries will cause it to consume all of the memory available.
// Because of that, it is not a general-purpose container and should not be used
// as one.
template <typename T>
class PacketNumberIndexedQueue {
 public:
  PacketNumberIndexedQueue()
      : number_of_present_entries_(0), first_packet_(0) {}

  // Retrieve the entry associated with the packet number.  Returns the pointer
  // to the entry in case of success, or nullptr if the entry does not exist.
  T* GetEntry(int64_t packet_number);
  const T* GetEntry(int64_t packet_number) const;

  // Inserts data associated |packet_number| into (or past) the end of the
  // queue, filling up the missing intermediate entries as necessary.  Returns
  // true if the element has been inserted successfully, false if it was already
  // in the queue or inserted out of order.
  template <typename... Args>
  bool Emplace(int64_t packet_number, Args&&... args);

  // Removes data associated with |packet_number| and frees the slots in the
  // queue as necessary.
  bool Remove(int64_t packet_number);

  bool IsEmpty() const { return number_of_present_entries_ == 0; }

  // Returns the number of entries in the queue.
  size_t number_of_present_entries() const {
    return number_of_present_entries_;
  }

  // Returns the number of entries allocated in the underlying deque.  This is
  // proportional to the memory usage of the queue.
  size_t entry_slots_used() const { return entries_.size(); }

  // Packet number of the first entry in the queue.  Zero if the queue is empty.
  int64_t first_packet() const { return first_packet_; }

  // Packet number of the last entry ever inserted in the queue.  Note that the
  // entry in question may have already been removed.  Zero if the queue is
  // empty.
  int64_t last_packet() const {
    if (IsEmpty()) {
      return 0;
    }
    return first_packet_ + entries_.size() - 1;
  }

 private:
  // Wrapper around T used to mark whether the entry is actually in the map.
  struct EntryWrapper {
    T data;
    bool present;

    EntryWrapper() : data(), present(false) {}

    template <typename... Args>
    explicit EntryWrapper(Args&&... args)
        : data(std::forward<Args>(args)...), present(true) {}
  };

  // Cleans up unused slots in the front after removing an element.
  void Cleanup();

  const EntryWrapper* GetEntryWrapper(int64_t offset) const;
  EntryWrapper* GetEntryWrapper(int64_t offset) {
    const auto* const_this = this;
    return const_cast<EntryWrapper*>(const_this->GetEntryWrapper(offset));
  }

  std::deque<EntryWrapper> entries_;
  size_t number_of_present_entries_;
  int64_t first_packet_;
};

template <typename T>
T* PacketNumberIndexedQueue<T>::GetEntry(int64_t packet_number) {
  EntryWrapper* entry = GetEntryWrapper(packet_number);
  if (entry == nullptr) {
    return nullptr;
  }
  return &entry->data;
}

template <typename T>
const T* PacketNumberIndexedQueue<T>::GetEntry(int64_t packet_number) const {
  const EntryWrapper* entry = GetEntryWrapper(packet_number);
  if (entry == nullptr) {
    return nullptr;
  }
  return &entry->data;
}

template <typename T>
template <typename... Args>
bool PacketNumberIndexedQueue<T>::Emplace(int64_t packet_number,
                                          Args&&... args) {
  if (IsEmpty()) {
    RTC_DCHECK(entries_.empty());
    RTC_DCHECK_EQ(0u, first_packet_);

    entries_.emplace_back(std::forward<Args>(args)...);
    number_of_present_entries_ = 1;
    first_packet_ = packet_number;
    return true;
  }

  // Do not allow insertion out-of-order.
  if (packet_number <= last_packet()) {
    return false;
  }

  // Handle potentially missing elements.
  int64_t offset = packet_number - first_packet_;
  if (offset > static_cast<int64_t>(entries_.size())) {
    entries_.resize(offset);
  }

  number_of_present_entries_++;
  entries_.emplace_back(std::forward<Args>(args)...);
  RTC_DCHECK_EQ(packet_number, last_packet());
  return true;
}

template <typename T>
bool PacketNumberIndexedQueue<T>::Remove(int64_t packet_number) {
  EntryWrapper* entry = GetEntryWrapper(packet_number);
  if (entry == nullptr) {
    return false;
  }
  entry->present = false;
  number_of_present_entries_--;

  if (packet_number == first_packet()) {
    Cleanup();
  }
  return true;
}

template <typename T>
void PacketNumberIndexedQueue<T>::Cleanup() {
  while (!entries_.empty() && !entries_.front().present) {
    entries_.pop_front();
    first_packet_++;
  }
  if (entries_.empty()) {
    first_packet_ = 0;
  }
}

template <typename T>
auto PacketNumberIndexedQueue<T>::GetEntryWrapper(int64_t offset) const
    -> const EntryWrapper* {
  if (offset < first_packet_) {
    return nullptr;
  }

  offset -= first_packet_;
  if (offset >= static_cast<int64_t>(entries_.size())) {
    return nullptr;
  }

  const EntryWrapper* entry = &entries_[offset];
  if (!entry->present) {
    return nullptr;
  }

  return entry;
}

}  // namespace bbr
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_BBR_PACKET_NUMBER_INDEXED_QUEUE_H_
