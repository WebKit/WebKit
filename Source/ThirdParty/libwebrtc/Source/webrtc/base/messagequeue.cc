/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <algorithm>

#include "webrtc/base/atomicops.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/messagequeue.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/trace_event.h"

namespace rtc {
namespace {

const int kMaxMsgLatency = 150;  // 150 ms
const int kSlowDispatchLoggingThreshold = 50;  // 50 ms

class SCOPED_LOCKABLE DebugNonReentrantCritScope {
 public:
  DebugNonReentrantCritScope(const CriticalSection* cs, bool* locked)
      EXCLUSIVE_LOCK_FUNCTION(cs)
      : cs_(cs), locked_(locked) {
    cs_->Enter();
    RTC_DCHECK(!*locked_);
    *locked_ = true;
  }

  ~DebugNonReentrantCritScope() UNLOCK_FUNCTION() {
    *locked_ = false;
    cs_->Leave();
  }

 private:
  const CriticalSection* const cs_;
  bool* locked_;

  RTC_DISALLOW_COPY_AND_ASSIGN(DebugNonReentrantCritScope);
};
}  // namespace

//------------------------------------------------------------------
// MessageQueueManager

MessageQueueManager* MessageQueueManager::instance_ = nullptr;

MessageQueueManager* MessageQueueManager::Instance() {
  // Note: This is not thread safe, but it is first called before threads are
  // spawned.
  if (!instance_)
    instance_ = new MessageQueueManager;
  return instance_;
}

bool MessageQueueManager::IsInitialized() {
  return instance_ != nullptr;
}

MessageQueueManager::MessageQueueManager() : locked_(false) {}

MessageQueueManager::~MessageQueueManager() {
}

void MessageQueueManager::Add(MessageQueue *message_queue) {
  return Instance()->AddInternal(message_queue);
}
void MessageQueueManager::AddInternal(MessageQueue *message_queue) {
  DebugNonReentrantCritScope cs(&crit_, &locked_);
  message_queues_.push_back(message_queue);
}

void MessageQueueManager::Remove(MessageQueue *message_queue) {
  // If there isn't a message queue manager instance, then there isn't a queue
  // to remove.
  if (!instance_) return;
  return Instance()->RemoveInternal(message_queue);
}
void MessageQueueManager::RemoveInternal(MessageQueue *message_queue) {
  // If this is the last MessageQueue, destroy the manager as well so that
  // we don't leak this object at program shutdown. As mentioned above, this is
  // not thread-safe, but this should only happen at program termination (when
  // the ThreadManager is destroyed, and threads are no longer active).
  bool destroy = false;
  {
    DebugNonReentrantCritScope cs(&crit_, &locked_);
    std::vector<MessageQueue *>::iterator iter;
    iter = std::find(message_queues_.begin(), message_queues_.end(),
                     message_queue);
    if (iter != message_queues_.end()) {
      message_queues_.erase(iter);
    }
    destroy = message_queues_.empty();
  }
  if (destroy) {
    instance_ = nullptr;
    delete this;
  }
}

void MessageQueueManager::Clear(MessageHandler *handler) {
  // If there isn't a message queue manager instance, then there aren't any
  // queues to remove this handler from.
  if (!instance_) return;
  return Instance()->ClearInternal(handler);
}
void MessageQueueManager::ClearInternal(MessageHandler *handler) {
  DebugNonReentrantCritScope cs(&crit_, &locked_);
  std::vector<MessageQueue *>::iterator iter;
  for (iter = message_queues_.begin(); iter != message_queues_.end(); iter++)
    (*iter)->Clear(handler);
}

void MessageQueueManager::ProcessAllMessageQueues() {
  if (!instance_) {
    return;
  }
  return Instance()->ProcessAllMessageQueuesInternal();
}

void MessageQueueManager::ProcessAllMessageQueuesInternal() {
  // This works by posting a delayed message at the current time and waiting
  // for it to be dispatched on all queues, which will ensure that all messages
  // that came before it were also dispatched.
  volatile int queues_not_done = 0;

  // This class is used so that whether the posted message is processed, or the
  // message queue is simply cleared, queues_not_done gets decremented.
  class ScopedIncrement : public MessageData {
   public:
    ScopedIncrement(volatile int* value) : value_(value) {
      AtomicOps::Increment(value_);
    }
    ~ScopedIncrement() override { AtomicOps::Decrement(value_); }

   private:
    volatile int* value_;
  };

  {
    DebugNonReentrantCritScope cs(&crit_, &locked_);
    for (MessageQueue* queue : message_queues_) {
      if (!queue->IsProcessingMessages()) {
        // If the queue is not processing messages, it can
        // be ignored. If we tried to post a message to it, it would be dropped
        // or ignored.
        continue;
      }
      queue->PostDelayed(RTC_FROM_HERE, 0, nullptr, MQID_DISPOSE,
                         new ScopedIncrement(&queues_not_done));
    }
  }
  // Note: One of the message queues may have been on this thread, which is why
  // we can't synchronously wait for queues_not_done to go to 0; we need to
  // process messages as well.
  while (AtomicOps::AcquireLoad(&queues_not_done) > 0) {
    rtc::Thread::Current()->ProcessMessages(0);
  }
}

//------------------------------------------------------------------
// MessageQueue
MessageQueue::MessageQueue(SocketServer* ss, bool init_queue)
    : fPeekKeep_(false),
      dmsgq_next_num_(0),
      fInitialized_(false),
      fDestroyed_(false),
      stop_(0),
      ss_(ss) {
  RTC_DCHECK(ss);
  // Currently, MessageQueue holds a socket server, and is the base class for
  // Thread.  It seems like it makes more sense for Thread to hold the socket
  // server, and provide it to the MessageQueue, since the Thread controls
  // the I/O model, and MQ is agnostic to those details.  Anyway, this causes
  // messagequeue_unittest to depend on network libraries... yuck.
  ss_->SetMessageQueue(this);
  if (init_queue) {
    DoInit();
  }
}

MessageQueue::MessageQueue(std::unique_ptr<SocketServer> ss, bool init_queue)
    : MessageQueue(ss.get(), init_queue) {
  own_ss_ = std::move(ss);
}

MessageQueue::~MessageQueue() {
  DoDestroy();
}

void MessageQueue::DoInit() {
  if (fInitialized_) {
    return;
  }

  fInitialized_ = true;
  MessageQueueManager::Add(this);
}

void MessageQueue::DoDestroy() {
  if (fDestroyed_) {
    return;
  }

  fDestroyed_ = true;
  // The signal is done from here to ensure
  // that it always gets called when the queue
  // is going away.
  SignalQueueDestroyed();
  MessageQueueManager::Remove(this);
  Clear(nullptr);

  if (ss_) {
    ss_->SetMessageQueue(nullptr);
  }
}

SocketServer* MessageQueue::socketserver() {
  return ss_;
}

void MessageQueue::WakeUpSocketServer() {
  ss_->WakeUp();
}

void MessageQueue::Quit() {
  AtomicOps::ReleaseStore(&stop_, 1);
  WakeUpSocketServer();
}

bool MessageQueue::IsQuitting() {
  return AtomicOps::AcquireLoad(&stop_) != 0;
}

bool MessageQueue::IsProcessingMessages() {
  return !IsQuitting();
}

void MessageQueue::Restart() {
  AtomicOps::ReleaseStore(&stop_, 0);
}

bool MessageQueue::Peek(Message *pmsg, int cmsWait) {
  if (fPeekKeep_) {
    *pmsg = msgPeek_;
    return true;
  }
  if (!Get(pmsg, cmsWait))
    return false;
  msgPeek_ = *pmsg;
  fPeekKeep_ = true;
  return true;
}

bool MessageQueue::Get(Message *pmsg, int cmsWait, bool process_io) {
  // Return and clear peek if present
  // Always return the peek if it exists so there is Peek/Get symmetry

  if (fPeekKeep_) {
    *pmsg = msgPeek_;
    fPeekKeep_ = false;
    return true;
  }

  // Get w/wait + timer scan / dispatch + socket / event multiplexer dispatch

  int64_t cmsTotal = cmsWait;
  int64_t cmsElapsed = 0;
  int64_t msStart = TimeMillis();
  int64_t msCurrent = msStart;
  while (true) {
    // Check for sent messages
    ReceiveSends();

    // Check for posted events
    int64_t cmsDelayNext = kForever;
    bool first_pass = true;
    while (true) {
      // All queue operations need to be locked, but nothing else in this loop
      // (specifically handling disposed message) can happen inside the crit.
      // Otherwise, disposed MessageHandlers will cause deadlocks.
      {
        CritScope cs(&crit_);
        // On the first pass, check for delayed messages that have been
        // triggered and calculate the next trigger time.
        if (first_pass) {
          first_pass = false;
          while (!dmsgq_.empty()) {
            if (msCurrent < dmsgq_.top().msTrigger_) {
              cmsDelayNext = TimeDiff(dmsgq_.top().msTrigger_, msCurrent);
              break;
            }
            msgq_.push_back(dmsgq_.top().msg_);
            dmsgq_.pop();
          }
        }
        // Pull a message off the message queue, if available.
        if (msgq_.empty()) {
          break;
        } else {
          *pmsg = msgq_.front();
          msgq_.pop_front();
        }
      }  // crit_ is released here.

      // Log a warning for time-sensitive messages that we're late to deliver.
      if (pmsg->ts_sensitive) {
        int64_t delay = TimeDiff(msCurrent, pmsg->ts_sensitive);
        if (delay > 0) {
          LOG_F(LS_WARNING) << "id: " << pmsg->message_id << "  delay: "
                            << (delay + kMaxMsgLatency) << "ms";
        }
      }
      // If this was a dispose message, delete it and skip it.
      if (MQID_DISPOSE == pmsg->message_id) {
        RTC_DCHECK(nullptr == pmsg->phandler);
        delete pmsg->pdata;
        *pmsg = Message();
        continue;
      }
      return true;
    }

    if (IsQuitting())
      break;

    // Which is shorter, the delay wait or the asked wait?

    int64_t cmsNext;
    if (cmsWait == kForever) {
      cmsNext = cmsDelayNext;
    } else {
      cmsNext = std::max<int64_t>(0, cmsTotal - cmsElapsed);
      if ((cmsDelayNext != kForever) && (cmsDelayNext < cmsNext))
        cmsNext = cmsDelayNext;
    }

    {
      // Wait and multiplex in the meantime
      if (!ss_->Wait(static_cast<int>(cmsNext), process_io))
        return false;
    }

    // If the specified timeout expired, return

    msCurrent = TimeMillis();
    cmsElapsed = TimeDiff(msCurrent, msStart);
    if (cmsWait != kForever) {
      if (cmsElapsed >= cmsWait)
        return false;
    }
  }
  return false;
}

void MessageQueue::ReceiveSends() {
}

void MessageQueue::Post(const Location& posted_from,
                        MessageHandler* phandler,
                        uint32_t id,
                        MessageData* pdata,
                        bool time_sensitive) {
  if (IsQuitting())
    return;

  // Keep thread safe
  // Add the message to the end of the queue
  // Signal for the multiplexer to return

  {
    CritScope cs(&crit_);
    Message msg;
    msg.posted_from = posted_from;
    msg.phandler = phandler;
    msg.message_id = id;
    msg.pdata = pdata;
    if (time_sensitive) {
      msg.ts_sensitive = TimeMillis() + kMaxMsgLatency;
    }
    msgq_.push_back(msg);
  }
  WakeUpSocketServer();
}

void MessageQueue::PostDelayed(const Location& posted_from,
                               int cmsDelay,
                               MessageHandler* phandler,
                               uint32_t id,
                               MessageData* pdata) {
  return DoDelayPost(posted_from, cmsDelay, TimeAfter(cmsDelay), phandler, id,
                     pdata);
}

void MessageQueue::PostAt(const Location& posted_from,
                          uint32_t tstamp,
                          MessageHandler* phandler,
                          uint32_t id,
                          MessageData* pdata) {
  // This should work even if it is used (unexpectedly).
  int64_t delay = static_cast<uint32_t>(TimeMillis()) - tstamp;
  return DoDelayPost(posted_from, delay, tstamp, phandler, id, pdata);
}

void MessageQueue::PostAt(const Location& posted_from,
                          int64_t tstamp,
                          MessageHandler* phandler,
                          uint32_t id,
                          MessageData* pdata) {
  return DoDelayPost(posted_from, TimeUntil(tstamp), tstamp, phandler, id,
                     pdata);
}

void MessageQueue::DoDelayPost(const Location& posted_from,
                               int64_t cmsDelay,
                               int64_t tstamp,
                               MessageHandler* phandler,
                               uint32_t id,
                               MessageData* pdata) {
  if (IsQuitting()) {
    return;
  }

  // Keep thread safe
  // Add to the priority queue. Gets sorted soonest first.
  // Signal for the multiplexer to return.

  {
    CritScope cs(&crit_);
    Message msg;
    msg.posted_from = posted_from;
    msg.phandler = phandler;
    msg.message_id = id;
    msg.pdata = pdata;
    DelayedMessage dmsg(cmsDelay, tstamp, dmsgq_next_num_, msg);
    dmsgq_.push(dmsg);
    // If this message queue processes 1 message every millisecond for 50 days,
    // we will wrap this number.  Even then, only messages with identical times
    // will be misordered, and then only briefly.  This is probably ok.
    ++dmsgq_next_num_;
    RTC_DCHECK_NE(0, dmsgq_next_num_);
  }
  WakeUpSocketServer();
}

int MessageQueue::GetDelay() {
  CritScope cs(&crit_);

  if (!msgq_.empty())
    return 0;

  if (!dmsgq_.empty()) {
    int delay = TimeUntil(dmsgq_.top().msTrigger_);
    if (delay < 0)
      delay = 0;
    return delay;
  }

  return kForever;
}

void MessageQueue::Clear(MessageHandler* phandler,
                         uint32_t id,
                         MessageList* removed) {
  CritScope cs(&crit_);

  // Remove messages with phandler

  if (fPeekKeep_ && msgPeek_.Match(phandler, id)) {
    if (removed) {
      removed->push_back(msgPeek_);
    } else {
      delete msgPeek_.pdata;
    }
    fPeekKeep_ = false;
  }

  // Remove from ordered message queue

  for (MessageList::iterator it = msgq_.begin(); it != msgq_.end();) {
    if (it->Match(phandler, id)) {
      if (removed) {
        removed->push_back(*it);
      } else {
        delete it->pdata;
      }
      it = msgq_.erase(it);
    } else {
      ++it;
    }
  }

  // Remove from priority queue. Not directly iterable, so use this approach

  PriorityQueue::container_type::iterator new_end = dmsgq_.container().begin();
  for (PriorityQueue::container_type::iterator it = new_end;
       it != dmsgq_.container().end(); ++it) {
    if (it->msg_.Match(phandler, id)) {
      if (removed) {
        removed->push_back(it->msg_);
      } else {
        delete it->msg_.pdata;
      }
    } else {
      *new_end++ = *it;
    }
  }
  dmsgq_.container().erase(new_end, dmsgq_.container().end());
  dmsgq_.reheap();
}

void MessageQueue::Dispatch(Message *pmsg) {
  TRACE_EVENT2("webrtc", "MessageQueue::Dispatch", "src_file_and_line",
               pmsg->posted_from.file_and_line(), "src_func",
               pmsg->posted_from.function_name());
  int64_t start_time = TimeMillis();
  pmsg->phandler->OnMessage(pmsg);
  int64_t end_time = TimeMillis();
  int64_t diff = TimeDiff(end_time, start_time);
  if (diff >= kSlowDispatchLoggingThreshold) {
    LOG(LS_INFO) << "Message took " << diff << "ms to dispatch. Posted from: "
                 << pmsg->posted_from.ToString();
  }
}

}  // namespace rtc
