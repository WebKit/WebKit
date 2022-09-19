/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/operations_chain.h"

#include <atomic>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "rtc_base/event.h"
#include "rtc_base/gunit.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace rtc {

using ::testing::ElementsAre;

namespace {

constexpr int kDefaultTimeout = 3000;

}  // namespace

class OperationTracker {
 public:
  OperationTracker() : background_thread_(Thread::Create()) {
    background_thread_->Start();
  }
  // The caller is responsible for ensuring that no operations are pending.
  ~OperationTracker() {}

  // Creates a binding for the synchronous operation (see
  // StartSynchronousOperation() below).
  std::function<void(std::function<void()>)> BindSynchronousOperation(
      Event* operation_complete_event) {
    return [this, operation_complete_event](std::function<void()> callback) {
      StartSynchronousOperation(operation_complete_event, std::move(callback));
    };
  }

  // Creates a binding for the asynchronous operation (see
  // StartAsynchronousOperation() below).
  std::function<void(std::function<void()>)> BindAsynchronousOperation(
      Event* unblock_operation_event,
      Event* operation_complete_event) {
    return [this, unblock_operation_event,
            operation_complete_event](std::function<void()> callback) {
      StartAsynchronousOperation(unblock_operation_event,
                                 operation_complete_event, std::move(callback));
    };
  }

  // When an operation is completed, its associated Event* is added to this
  // list, in chronological order. This allows you to verify the order that
  // operations are executed.
  const std::vector<Event*>& completed_operation_events() const {
    return completed_operation_events_;
  }

 private:
  // This operation is completed synchronously; the callback is invoked before
  // the function returns.
  void StartSynchronousOperation(Event* operation_complete_event,
                                 std::function<void()> callback) {
    completed_operation_events_.push_back(operation_complete_event);
    operation_complete_event->Set();
    callback();
  }

  // This operation is completed asynchronously; it pings `background_thread_`,
  // blocking that thread until `unblock_operation_event` is signaled and then
  // completes upon posting back to the thread that the operation started on.
  // Note that this requires the starting thread to be executing tasks (handle
  // messages), i.e. must not be blocked.
  void StartAsynchronousOperation(Event* unblock_operation_event,
                                  Event* operation_complete_event,
                                  std::function<void()> callback) {
    Thread* current_thread = Thread::Current();
    background_thread_->PostTask([this, current_thread, unblock_operation_event,
                                  operation_complete_event, callback]() {
      unblock_operation_event->Wait(Event::kForever);
      current_thread->PostTask([this, operation_complete_event, callback]() {
        completed_operation_events_.push_back(operation_complete_event);
        operation_complete_event->Set();
        callback();
      });
    });
  }

  std::unique_ptr<Thread> background_thread_;
  std::vector<Event*> completed_operation_events_;
};

// The OperationTrackerProxy ensures all operations are chained on a separate
// thread. This allows tests to block while chained operations are posting
// between threads.
class OperationTrackerProxy {
 public:
  OperationTrackerProxy()
      : operations_chain_thread_(Thread::Create()),
        operation_tracker_(nullptr),
        operations_chain_(nullptr) {
    operations_chain_thread_->Start();
  }

  std::unique_ptr<Event> Initialize() {
    std::unique_ptr<Event> event = std::make_unique<Event>();
    operations_chain_thread_->PostTask([this, event_ptr = event.get()]() {
      operation_tracker_ = std::make_unique<OperationTracker>();
      operations_chain_ = OperationsChain::Create();
      event_ptr->Set();
    });
    return event;
  }

  void SetOnChainEmptyCallback(std::function<void()> on_chain_empty_callback) {
    Event event;
    operations_chain_thread_->PostTask(
        [this, &event,
         on_chain_empty_callback = std::move(on_chain_empty_callback)]() {
          operations_chain_->SetOnChainEmptyCallback(
              std::move(on_chain_empty_callback));
          event.Set();
        });
    event.Wait(Event::kForever);
  }

  bool IsEmpty() {
    Event event;
    bool is_empty = false;
    operations_chain_thread_->PostTask([this, &event, &is_empty]() {
      is_empty = operations_chain_->IsEmpty();
      event.Set();
    });
    event.Wait(Event::kForever);
    return is_empty;
  }

  std::unique_ptr<Event> ReleaseOperationChain() {
    std::unique_ptr<Event> event = std::make_unique<Event>();
    operations_chain_thread_->PostTask([this, event_ptr = event.get()]() {
      operations_chain_ = nullptr;
      event_ptr->Set();
    });
    return event;
  }

  // Chains a synchronous operation on the operation chain's thread.
  std::unique_ptr<Event> PostSynchronousOperation() {
    std::unique_ptr<Event> operation_complete_event = std::make_unique<Event>();
    operations_chain_thread_->PostTask(
        [this,
         operation_complete_event_ptr = operation_complete_event.get()]() {
          operations_chain_->ChainOperation(
              operation_tracker_->BindSynchronousOperation(
                  operation_complete_event_ptr));
        });
    return operation_complete_event;
  }

  // Chains an asynchronous operation on the operation chain's thread. This
  // involves the operation chain thread and an additional background thread.
  std::unique_ptr<Event> PostAsynchronousOperation(
      Event* unblock_operation_event) {
    std::unique_ptr<Event> operation_complete_event = std::make_unique<Event>();
    operations_chain_thread_->PostTask(
        [this, unblock_operation_event,
         operation_complete_event_ptr = operation_complete_event.get()]() {
          operations_chain_->ChainOperation(
              operation_tracker_->BindAsynchronousOperation(
                  unblock_operation_event, operation_complete_event_ptr));
        });
    return operation_complete_event;
  }

  // The order of completed events. Touches the `operation_tracker_` on the
  // calling thread, this is only thread safe if all chained operations have
  // completed.
  const std::vector<Event*>& completed_operation_events() const {
    return operation_tracker_->completed_operation_events();
  }

 private:
  std::unique_ptr<Thread> operations_chain_thread_;
  std::unique_ptr<OperationTracker> operation_tracker_;
  scoped_refptr<OperationsChain> operations_chain_;
};

// On destruction, sets a boolean flag to true.
class SignalOnDestruction final {
 public:
  SignalOnDestruction(bool* destructor_called)
      : destructor_called_(destructor_called) {
    RTC_DCHECK(destructor_called_);
  }
  ~SignalOnDestruction() {
    // Moved objects will have `destructor_called_` set to null. Destroying a
    // moved SignalOnDestruction should not signal.
    if (destructor_called_) {
      *destructor_called_ = true;
    }
  }

  SignalOnDestruction(const SignalOnDestruction&) = delete;
  SignalOnDestruction& operator=(const SignalOnDestruction&) = delete;

  // Move operators.
  SignalOnDestruction(SignalOnDestruction&& other)
      : SignalOnDestruction(other.destructor_called_) {
    other.destructor_called_ = nullptr;
  }
  SignalOnDestruction& operator=(SignalOnDestruction&& other) {
    destructor_called_ = other.destructor_called_;
    other.destructor_called_ = nullptr;
    return *this;
  }

 private:
  bool* destructor_called_;
};

TEST(OperationsChainTest, SynchronousOperation) {
  OperationTrackerProxy operation_tracker_proxy;
  operation_tracker_proxy.Initialize()->Wait(Event::kForever);

  operation_tracker_proxy.PostSynchronousOperation()->Wait(Event::kForever);
}

TEST(OperationsChainTest, AsynchronousOperation) {
  OperationTrackerProxy operation_tracker_proxy;
  operation_tracker_proxy.Initialize()->Wait(Event::kForever);

  Event unblock_async_operation_event;
  auto async_operation_completed_event =
      operation_tracker_proxy.PostAsynchronousOperation(
          &unblock_async_operation_event);
  // This should not be signaled until we unblock the operation.
  EXPECT_FALSE(async_operation_completed_event->Wait(0));
  // Unblock the operation and wait for it to complete.
  unblock_async_operation_event.Set();
  async_operation_completed_event->Wait(Event::kForever);
}

TEST(OperationsChainTest,
     SynchronousOperationsAreExecutedImmediatelyWhenChainIsEmpty) {
  // Testing synchonicity must be done without the OperationTrackerProxy to
  // ensure messages are not processed in parallel. This test has no background
  // threads.
  scoped_refptr<OperationsChain> operations_chain = OperationsChain::Create();
  OperationTracker operation_tracker;
  Event event0;
  operations_chain->ChainOperation(
      operation_tracker.BindSynchronousOperation(&event0));
  // This should already be signaled. (If it wasn't, waiting wouldn't help,
  // because we'd be blocking the only thread that exists.)
  EXPECT_TRUE(event0.Wait(0));
  // Chaining another operation should also execute immediately because the
  // chain should already be empty.
  Event event1;
  operations_chain->ChainOperation(
      operation_tracker.BindSynchronousOperation(&event1));
  EXPECT_TRUE(event1.Wait(0));
}

TEST(OperationsChainTest, AsynchronousOperationBlocksSynchronousOperation) {
  OperationTrackerProxy operation_tracker_proxy;
  operation_tracker_proxy.Initialize()->Wait(Event::kForever);

  Event unblock_async_operation_event;
  auto async_operation_completed_event =
      operation_tracker_proxy.PostAsynchronousOperation(
          &unblock_async_operation_event);

  auto sync_operation_completed_event =
      operation_tracker_proxy.PostSynchronousOperation();

  unblock_async_operation_event.Set();

  sync_operation_completed_event->Wait(Event::kForever);
  // The asynchronous avent should have blocked the synchronous event, meaning
  // this should already be signaled.
  EXPECT_TRUE(async_operation_completed_event->Wait(0));
}

TEST(OperationsChainTest, OperationsAreExecutedInOrder) {
  OperationTrackerProxy operation_tracker_proxy;
  operation_tracker_proxy.Initialize()->Wait(Event::kForever);

  // Chain a mix of asynchronous and synchronous operations.
  Event operation0_unblock_event;
  auto operation0_completed_event =
      operation_tracker_proxy.PostAsynchronousOperation(
          &operation0_unblock_event);

  Event operation1_unblock_event;
  auto operation1_completed_event =
      operation_tracker_proxy.PostAsynchronousOperation(
          &operation1_unblock_event);

  auto operation2_completed_event =
      operation_tracker_proxy.PostSynchronousOperation();

  auto operation3_completed_event =
      operation_tracker_proxy.PostSynchronousOperation();

  Event operation4_unblock_event;
  auto operation4_completed_event =
      operation_tracker_proxy.PostAsynchronousOperation(
          &operation4_unblock_event);

  auto operation5_completed_event =
      operation_tracker_proxy.PostSynchronousOperation();

  Event operation6_unblock_event;
  auto operation6_completed_event =
      operation_tracker_proxy.PostAsynchronousOperation(
          &operation6_unblock_event);

  // Unblock events in reverse order. Operations 5, 3 and 2 are synchronous and
  // don't need to be unblocked.
  operation6_unblock_event.Set();
  operation4_unblock_event.Set();
  operation1_unblock_event.Set();
  operation0_unblock_event.Set();
  // Await all operations. The await-order shouldn't matter since they all get
  // executed eventually.
  operation0_completed_event->Wait(Event::kForever);
  operation1_completed_event->Wait(Event::kForever);
  operation2_completed_event->Wait(Event::kForever);
  operation3_completed_event->Wait(Event::kForever);
  operation4_completed_event->Wait(Event::kForever);
  operation5_completed_event->Wait(Event::kForever);
  operation6_completed_event->Wait(Event::kForever);

  EXPECT_THAT(
      operation_tracker_proxy.completed_operation_events(),
      ElementsAre(
          operation0_completed_event.get(), operation1_completed_event.get(),
          operation2_completed_event.get(), operation3_completed_event.get(),
          operation4_completed_event.get(), operation5_completed_event.get(),
          operation6_completed_event.get()));
}

TEST(OperationsChainTest, IsEmpty) {
  OperationTrackerProxy operation_tracker_proxy;
  operation_tracker_proxy.Initialize()->Wait(Event::kForever);

  // The chain is initially empty.
  EXPECT_TRUE(operation_tracker_proxy.IsEmpty());
  // Chain a single event.
  Event unblock_async_operation_event0;
  auto async_operation_completed_event0 =
      operation_tracker_proxy.PostAsynchronousOperation(
          &unblock_async_operation_event0);
  // The chain is not empty while an event is pending.
  EXPECT_FALSE(operation_tracker_proxy.IsEmpty());
  // Completing the operation empties the chain.
  unblock_async_operation_event0.Set();
  async_operation_completed_event0->Wait(Event::kForever);
  EXPECT_TRUE(operation_tracker_proxy.IsEmpty());

  // Chain multiple events.
  Event unblock_async_operation_event1;
  auto async_operation_completed_event1 =
      operation_tracker_proxy.PostAsynchronousOperation(
          &unblock_async_operation_event1);
  Event unblock_async_operation_event2;
  auto async_operation_completed_event2 =
      operation_tracker_proxy.PostAsynchronousOperation(
          &unblock_async_operation_event2);
  // Again, the chain is not empty while an event is pending.
  EXPECT_FALSE(operation_tracker_proxy.IsEmpty());
  // Upon completing the first event, the chain is still not empty.
  unblock_async_operation_event1.Set();
  async_operation_completed_event1->Wait(Event::kForever);
  EXPECT_FALSE(operation_tracker_proxy.IsEmpty());
  // Completing the last evenet empties the chain.
  unblock_async_operation_event2.Set();
  async_operation_completed_event2->Wait(Event::kForever);
  EXPECT_TRUE(operation_tracker_proxy.IsEmpty());
}

TEST(OperationsChainTest, OnChainEmptyCallback) {
  rtc::AutoThread main_thread;
  OperationTrackerProxy operation_tracker_proxy;
  operation_tracker_proxy.Initialize()->Wait(Event::kForever);

  std::atomic<size_t> on_empty_callback_counter(0u);
  operation_tracker_proxy.SetOnChainEmptyCallback(
      [&on_empty_callback_counter] { ++on_empty_callback_counter; });

  // Chain a single event.
  Event unblock_async_operation_event0;
  auto async_operation_completed_event0 =
      operation_tracker_proxy.PostAsynchronousOperation(
          &unblock_async_operation_event0);
  // The callback is not invoked until the operation has completed.
  EXPECT_EQ(0u, on_empty_callback_counter);
  // Completing the operation empties the chain, invoking the callback.
  unblock_async_operation_event0.Set();
  async_operation_completed_event0->Wait(Event::kForever);
  EXPECT_TRUE_WAIT(1u == on_empty_callback_counter, kDefaultTimeout);

  // Chain multiple events.
  Event unblock_async_operation_event1;
  auto async_operation_completed_event1 =
      operation_tracker_proxy.PostAsynchronousOperation(
          &unblock_async_operation_event1);
  Event unblock_async_operation_event2;
  auto async_operation_completed_event2 =
      operation_tracker_proxy.PostAsynchronousOperation(
          &unblock_async_operation_event2);
  // Again, the callback is not invoked until the operation has completed.
  EXPECT_TRUE_WAIT(1u == on_empty_callback_counter, kDefaultTimeout);
  // Upon completing the first event, the chain is still not empty, so the
  // callback must not be invoked yet.
  unblock_async_operation_event1.Set();
  async_operation_completed_event1->Wait(Event::kForever);
  EXPECT_TRUE_WAIT(1u == on_empty_callback_counter, kDefaultTimeout);
  // Completing the last evenet empties the chain, invoking the callback.
  unblock_async_operation_event2.Set();
  async_operation_completed_event2->Wait(Event::kForever);
  EXPECT_TRUE_WAIT(2u == on_empty_callback_counter, kDefaultTimeout);
}

TEST(OperationsChainTest,
     SafeToReleaseReferenceToOperationChainWhileOperationIsPending) {
  OperationTrackerProxy operation_tracker_proxy;
  operation_tracker_proxy.Initialize()->Wait(Event::kForever);

  Event unblock_async_operation_event;
  auto async_operation_completed_event =
      operation_tracker_proxy.PostAsynchronousOperation(
          &unblock_async_operation_event);

  // Pending operations keep the OperationChain alive, making it safe for the
  // test to release any references before unblocking the async operation.
  operation_tracker_proxy.ReleaseOperationChain()->Wait(Event::kForever);

  unblock_async_operation_event.Set();
  async_operation_completed_event->Wait(Event::kForever);
}

TEST(OperationsChainTest, FunctorIsNotDestroyedWhileExecuting) {
  scoped_refptr<OperationsChain> operations_chain = OperationsChain::Create();

  bool destructor_called = false;
  SignalOnDestruction signal_on_destruction(&destructor_called);

  operations_chain->ChainOperation(
      [signal_on_destruction = std::move(signal_on_destruction),
       &destructor_called](std::function<void()> callback) {
        EXPECT_FALSE(destructor_called);
        // Invoking the callback marks the operation as complete, popping the
        // Operation object from the OperationsChain internal queue.
        callback();
        // Even though the internal Operation object has been destroyed,
        // variables captured by this lambda expression must still be valid (the
        // associated functor must not be deleted while executing).
        EXPECT_FALSE(destructor_called);
      });
  // The lambda having executed synchronously and completed, its captured
  // variables should now have been deleted.
  EXPECT_TRUE(destructor_called);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

TEST(OperationsChainDeathTest, OperationNotInvokingCallbackShouldCrash) {
  scoped_refptr<OperationsChain> operations_chain = OperationsChain::Create();
  EXPECT_DEATH(
      operations_chain->ChainOperation([](std::function<void()> callback) {}),
      "");
}

TEST(OperationsChainDeathTest,
     OperationInvokingCallbackMultipleTimesShouldCrash) {
  scoped_refptr<OperationsChain> operations_chain = OperationsChain::Create();
  EXPECT_DEATH(
      operations_chain->ChainOperation([](std::function<void()> callback) {
        // Signal that the operation has completed multiple times.
        callback();
        callback();
      }),
      "");
}

#endif  // RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

}  // namespace rtc
