/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_MESSAGE_HANDLER_H_
#define RTC_BASE_MESSAGE_HANDLER_H_

#include <utility>

#include "rtc_base/constructor_magic.h"

namespace rtc {

struct Message;

// Messages get dispatched to a MessageHandler

class MessageHandler {
 public:
  virtual ~MessageHandler();
  virtual void OnMessage(Message* msg) = 0;

 protected:
  MessageHandler() {}

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(MessageHandler);
};

// Helper class to facilitate executing a functor on a thread.
template <class ReturnT, class FunctorT>
class FunctorMessageHandler : public MessageHandler {
 public:
  explicit FunctorMessageHandler(FunctorT&& functor)
      : functor_(std::forward<FunctorT>(functor)) {}
  virtual void OnMessage(Message* msg) { result_ = functor_(); }
  const ReturnT& result() const { return result_; }

  // Returns moved result. Should not call result() or MoveResult() again
  // after this.
  ReturnT MoveResult() { return std::move(result_); }

 private:
  FunctorT functor_;
  ReturnT result_;
};

// Specialization for ReturnT of void.
template <class FunctorT>
class FunctorMessageHandler<void, FunctorT> : public MessageHandler {
 public:
  explicit FunctorMessageHandler(FunctorT&& functor)
      : functor_(std::forward<FunctorT>(functor)) {}
  virtual void OnMessage(Message* msg) { functor_(); }
  void result() const {}
  void MoveResult() {}

 private:
  FunctorT functor_;
};

}  // namespace rtc

#endif  // RTC_BASE_MESSAGE_HANDLER_H_
