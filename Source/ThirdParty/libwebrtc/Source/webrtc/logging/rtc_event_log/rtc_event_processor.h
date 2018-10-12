/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_RTC_EVENT_PROCESSOR_H_
#define LOGGING_RTC_EVENT_LOG_RTC_EVENT_PROCESSOR_H_

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "rtc_base/function_view.h"

namespace webrtc {

// This file contains helper class used to process the elements of two or more
// sorted lists in timestamp order. The effect is the same as doing a merge step
// in the merge-sort algorithm but without copying the elements or modifying the
// lists.

// Interface to allow "merging" lists of different types. ProcessNext()
// processes the next unprocesses element in the list. IsEmpty() checks if all
// elements have been processed. GetNextTime returns the timestamp of the next
// unprocessed element.
class ProcessableEventListInterface {
 public:
  virtual ~ProcessableEventListInterface() = default;
  virtual void ProcessNext() = 0;
  virtual bool IsEmpty() const = 0;
  virtual int64_t GetNextTime() const = 0;
};

// ProcessableEventList encapsulates a list of events and a function that will
// be applied to each element of the list.
template <typename T>
class ProcessableEventList : public ProcessableEventListInterface {
 public:
  // N.B. |f| is not owned by ProcessableEventList. The caller must ensure that
  // the function object or lambda outlives ProcessableEventList and
  // RtcEventProcessor. The same thing applies to the iterators (begin, end);
  // the vector must outlive ProcessableEventList and must not be modified until
  // processing has finished.
  ProcessableEventList(typename std::vector<T>::const_iterator begin,
                       typename std::vector<T>::const_iterator end,
                       rtc::FunctionView<void(const T&)> f)
      : begin_(begin), end_(end), f_(f) {}

  void ProcessNext() override {
    RTC_DCHECK(!IsEmpty());
    f_(*begin_);
    ++begin_;
  }

  bool IsEmpty() const override { return begin_ == end_; }

  int64_t GetNextTime() const override {
    RTC_DCHECK(!IsEmpty());
    return begin_->log_time_us();
  }

 private:
  typename std::vector<T>::const_iterator begin_;
  typename std::vector<T>::const_iterator end_;
  rtc::FunctionView<void(const T&)> f_;
};

// Helper class used to "merge" two or more lists of ordered RtcEventLog events
// so that they can be treated as a single ordered list. Since the individual
// lists may have different types, we need to access the lists via pointers to
// the common base class.
//
// Usage example:
// ParsedRtcEventLogNew log;
// auto incoming_handler = [] (LoggedRtcpPacketIncoming elem) { ... };
// auto incoming_rtcp =
//     absl::make_unique<ProcessableEventList<LoggedRtcpPacketIncoming>>(
//         log.incoming_rtcp_packets().begin(),
//         log.incoming_rtcp_packets().end(),
//         incoming_handler);
// auto outgoing_handler = [] (LoggedRtcpPacketOutgoing elem) { ... };
// auto outgoing_rtcp =
//     absl::make_unique<ProcessableEventList<LoggedRtcpPacketOutgoing>>(
//         log.outgoing_rtcp_packets().begin(),
//         log.outgoing_rtcp_packets().end(),
//         outgoing_handler);
//
// RtcEventProcessor processor;
// processor.AddEvents(std::move(incoming_rtcp));
// processor.AddEvents(std::move(outgoing_rtcp));
// processor.ProcessEventsInOrder();
class RtcEventProcessor {
 public:
  // The elements of each list is processed in the index order. To process all
  // elements in all lists in timestamp order, each lists need to be sorted in
  // timestamp order prior to insertion. Otherwise,
  void AddEvents(std::unique_ptr<ProcessableEventListInterface> events) {
    if (!events->IsEmpty()) {
      event_lists_.push_back(std::move(events));
      std::push_heap(event_lists_.begin(), event_lists_.end(), Cmp);
    }
  }

  void ProcessEventsInOrder() {
    // |event_lists_| is a min-heap of lists ordered by the timestamp of the
    // first element in the list. We therefore process the first element of the
    // first list, then reinsert the remainder of that list into the heap
    // if the list still contains unprocessed elements.
    while (!event_lists_.empty()) {
      event_lists_.front()->ProcessNext();
      std::pop_heap(event_lists_.begin(), event_lists_.end(), Cmp);
      if (event_lists_.back()->IsEmpty()) {
        event_lists_.pop_back();
      } else {
        std::push_heap(event_lists_.begin(), event_lists_.end(), Cmp);
      }
    }
  }

 private:
  using ListPtrType = std::unique_ptr<ProcessableEventListInterface>;
  std::vector<ListPtrType> event_lists_;
  // Comparison function to make |event_lists_| into a min heap.
  static bool Cmp(const ListPtrType& a, const ListPtrType& b) {
    return a->GetNextTime() > b->GetNextTime();
  }
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_RTC_EVENT_PROCESSOR_H_
