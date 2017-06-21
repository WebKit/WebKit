/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_ASYNCINVOKER_INL_H_
#define WEBRTC_BASE_ASYNCINVOKER_INL_H_

#include "webrtc/base/atomicops.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/messagehandler.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/thread_annotations.h"

namespace rtc {

class AsyncInvoker;

// Helper class for AsyncInvoker. Runs a task and triggers a callback
// on the calling thread if necessary.
class AsyncClosure {
 public:
  explicit AsyncClosure(AsyncInvoker* invoker) : invoker_(invoker) {}
  virtual ~AsyncClosure();
  // Runs the asynchronous task, and triggers a callback to the calling
  // thread if needed. Should be called from the target thread.
  virtual void Execute() = 0;

 protected:
  AsyncInvoker* invoker_;
};

// Simple closure that doesn't trigger a callback for the calling thread.
template <class FunctorT>
class FireAndForgetAsyncClosure : public AsyncClosure {
 public:
  explicit FireAndForgetAsyncClosure(AsyncInvoker* invoker,
                                     const FunctorT& functor)
      : AsyncClosure(invoker), functor_(functor) {}
  virtual void Execute() {
    functor_();
  }
 private:
  FunctorT functor_;
};

}  // namespace rtc

#endif  // WEBRTC_BASE_ASYNCINVOKER_INL_H_
