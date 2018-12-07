/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/thread.h"

#if defined(WEBRTC_WIN)
#include <comdef.h>
#elif defined(WEBRTC_POSIX)
#include <time.h>
#else
#error "Either WEBRTC_WIN or WEBRTC_POSIX needs to be defined."
#endif

#if defined(WEBRTC_WIN)
// Disable warning that we don't care about:
// warning C4722: destructor never returns, potential memory leak
#pragma warning(disable : 4722)
#endif

#include <stdio.h>
#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/logging.h"
#include "rtc_base/nullsocketserver.h"
#include "rtc_base/timeutils.h"
#include "rtc_base/trace_event.h"

#if defined(WEBRTC_MAC)
#include "rtc_base/system/cocoa_threading.h"

/*
 * These are forward-declarations for methods that are part of the
 * ObjC runtime. They are declared in the private header objc-internal.h.
 * These calls are what clang inserts when using @autoreleasepool in ObjC,
 * but here they are used directly in order to keep this file C++.
 * https://clang.llvm.org/docs/AutomaticReferenceCounting.html#runtime-support
 */
extern "C" {
void* objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void* pool);
}

namespace {
class ScopedAutoReleasePool {
 public:
  ScopedAutoReleasePool() : pool_(objc_autoreleasePoolPush()) {}
  ~ScopedAutoReleasePool() { objc_autoreleasePoolPop(pool_); }

 private:
  void* const pool_;
};
}  // namespace
#endif

namespace rtc {

ThreadManager* ThreadManager::Instance() {
  static ThreadManager* const thread_manager = new ThreadManager();
  return thread_manager;
}

ThreadManager::~ThreadManager() {
  // By above RTC_DEFINE_STATIC_LOCAL.
  RTC_NOTREACHED() << "ThreadManager should never be destructed.";
}

// static
Thread* Thread::Current() {
  ThreadManager* manager = ThreadManager::Instance();
  Thread* thread = manager->CurrentThread();

#ifndef NO_MAIN_THREAD_WRAPPING
  // Only autowrap the thread which instantiated the ThreadManager.
  if (!thread && manager->IsMainThread()) {
    thread = new Thread(SocketServer::CreateDefault());
    thread->WrapCurrentWithThreadManager(manager, true);
  }
#endif

  return thread;
}

#if defined(WEBRTC_POSIX)
ThreadManager::ThreadManager() : main_thread_ref_(CurrentThreadRef()) {
#if defined(WEBRTC_MAC)
  InitCocoaMultiThreading();
#endif
  pthread_key_create(&key_, nullptr);
}

Thread* ThreadManager::CurrentThread() {
  return static_cast<Thread*>(pthread_getspecific(key_));
}

void ThreadManager::SetCurrentThread(Thread* thread) {
#if RTC_DLOG_IS_ON
  if (CurrentThread() && thread) {
    RTC_DLOG(LS_ERROR) << "SetCurrentThread: Overwriting an existing value?";
  }
#endif  // RTC_DLOG_IS_ON
  pthread_setspecific(key_, thread);
}
#endif

#if defined(WEBRTC_WIN)
ThreadManager::ThreadManager()
    : key_(TlsAlloc()), main_thread_ref_(CurrentThreadRef()) {}

Thread* ThreadManager::CurrentThread() {
  return static_cast<Thread*>(TlsGetValue(key_));
}

void ThreadManager::SetCurrentThread(Thread* thread) {
  RTC_DCHECK(!CurrentThread() || !thread);
  TlsSetValue(key_, thread);
}
#endif

Thread* ThreadManager::WrapCurrentThread() {
  Thread* result = CurrentThread();
  if (nullptr == result) {
    result = new Thread(SocketServer::CreateDefault());
    result->WrapCurrentWithThreadManager(this, true);
  }
  return result;
}

void ThreadManager::UnwrapCurrentThread() {
  Thread* t = CurrentThread();
  if (t && !(t->IsOwned())) {
    t->UnwrapCurrent();
    delete t;
  }
}

bool ThreadManager::IsMainThread() {
  return IsThreadRefEqual(CurrentThreadRef(), main_thread_ref_);
}

Thread::ScopedDisallowBlockingCalls::ScopedDisallowBlockingCalls()
    : thread_(Thread::Current()),
      previous_state_(thread_->SetAllowBlockingCalls(false)) {}

Thread::ScopedDisallowBlockingCalls::~ScopedDisallowBlockingCalls() {
  RTC_DCHECK(thread_->IsCurrent());
  thread_->SetAllowBlockingCalls(previous_state_);
}

// DEPRECATED.
Thread::Thread() : Thread(SocketServer::CreateDefault()) {}

Thread::Thread(SocketServer* ss) : Thread(ss, /*do_init=*/true) {}

Thread::Thread(std::unique_ptr<SocketServer> ss)
    : Thread(std::move(ss), /*do_init=*/true) {}

Thread::Thread(SocketServer* ss, bool do_init)
    : MessageQueue(ss, /*do_init=*/false) {
  SetName("Thread", this);  // default name
  if (do_init) {
    DoInit();
  }
}

Thread::Thread(std::unique_ptr<SocketServer> ss, bool do_init)
    : MessageQueue(std::move(ss), false) {
  SetName("Thread", this);  // default name
  if (do_init) {
    DoInit();
  }
}

Thread::~Thread() {
  Stop();
  DoDestroy();
}

bool Thread::IsCurrent() const {
  return ThreadManager::Instance()->CurrentThread() == this;
}

std::unique_ptr<Thread> Thread::CreateWithSocketServer() {
  return std::unique_ptr<Thread>(new Thread(SocketServer::CreateDefault()));
}

std::unique_ptr<Thread> Thread::Create() {
  return std::unique_ptr<Thread>(
      new Thread(std::unique_ptr<SocketServer>(new NullSocketServer())));
}

bool Thread::SleepMs(int milliseconds) {
  AssertBlockingIsAllowedOnCurrentThread();

#if defined(WEBRTC_WIN)
  ::Sleep(milliseconds);
  return true;
#else
  // POSIX has both a usleep() and a nanosleep(), but the former is deprecated,
  // so we use nanosleep() even though it has greater precision than necessary.
  struct timespec ts;
  ts.tv_sec = milliseconds / 1000;
  ts.tv_nsec = (milliseconds % 1000) * 1000000;
  int ret = nanosleep(&ts, nullptr);
  if (ret != 0) {
    RTC_LOG_ERR(LS_WARNING) << "nanosleep() returning early";
    return false;
  }
  return true;
#endif
}

bool Thread::SetName(const std::string& name, const void* obj) {
  RTC_DCHECK(!IsRunning());

  name_ = name;
  if (obj) {
    // The %p specifier typically produce at most 16 hex digits, possibly with a
    // 0x prefix. But format is implementation defined, so add some margin.
    char buf[30];
    snprintf(buf, sizeof(buf), " 0x%p", obj);
    name_ += buf;
  }
  return true;
}

bool Thread::Start(Runnable* runnable) {
  RTC_DCHECK(!IsRunning());

  if (IsRunning())
    return false;

  Restart();  // reset IsQuitting() if the thread is being restarted

  // Make sure that ThreadManager is created on the main thread before
  // we start a new thread.
  ThreadManager::Instance();

  owned_ = true;

  ThreadInit* init = new ThreadInit;
  init->thread = this;
  init->runnable = runnable;
#if defined(WEBRTC_WIN)
  thread_ = CreateThread(nullptr, 0, PreRun, init, 0, &thread_id_);
  if (!thread_) {
    return false;
  }
#elif defined(WEBRTC_POSIX)
  pthread_attr_t attr;
  pthread_attr_init(&attr);

  int error_code = pthread_create(&thread_, &attr, PreRun, init);
  if (0 != error_code) {
    RTC_LOG(LS_ERROR) << "Unable to create pthread, error " << error_code;
    thread_ = 0;
    return false;
  }
  RTC_DCHECK(thread_);
#endif
  return true;
}

bool Thread::WrapCurrent() {
  return WrapCurrentWithThreadManager(ThreadManager::Instance(), true);
}

void Thread::UnwrapCurrent() {
  // Clears the platform-specific thread-specific storage.
  ThreadManager::Instance()->SetCurrentThread(nullptr);
#if defined(WEBRTC_WIN)
  if (thread_ != nullptr) {
    if (!CloseHandle(thread_)) {
      RTC_LOG_GLE(LS_ERROR)
          << "When unwrapping thread, failed to close handle.";
    }
    thread_ = nullptr;
    thread_id_ = 0;
  }
#elif defined(WEBRTC_POSIX)
  thread_ = 0;
#endif
}

void Thread::SafeWrapCurrent() {
  WrapCurrentWithThreadManager(ThreadManager::Instance(), false);
}

void Thread::Join() {
  if (!IsRunning())
    return;

  RTC_DCHECK(!IsCurrent());
  if (Current() && !Current()->blocking_calls_allowed_) {
    RTC_LOG(LS_WARNING) << "Waiting for the thread to join, "
                        << "but blocking calls have been disallowed";
  }

#if defined(WEBRTC_WIN)
  RTC_DCHECK(thread_ != nullptr);
  WaitForSingleObject(thread_, INFINITE);
  CloseHandle(thread_);
  thread_ = nullptr;
  thread_id_ = 0;
#elif defined(WEBRTC_POSIX)
  pthread_join(thread_, nullptr);
  thread_ = 0;
#endif
}

bool Thread::SetAllowBlockingCalls(bool allow) {
  RTC_DCHECK(IsCurrent());
  bool previous = blocking_calls_allowed_;
  blocking_calls_allowed_ = allow;
  return previous;
}

// static
void Thread::AssertBlockingIsAllowedOnCurrentThread() {
#if !defined(NDEBUG)
  Thread* current = Thread::Current();
  RTC_DCHECK(!current || current->blocking_calls_allowed_);
#endif
}

// static
#if defined(WEBRTC_WIN)
DWORD WINAPI Thread::PreRun(LPVOID pv) {
#else
void* Thread::PreRun(void* pv) {
#endif
  ThreadInit* init = static_cast<ThreadInit*>(pv);
  ThreadManager::Instance()->SetCurrentThread(init->thread);
  rtc::SetCurrentThreadName(init->thread->name_.c_str());
#if defined(WEBRTC_MAC)
  ScopedAutoReleasePool pool;
#endif
  if (init->runnable) {
    init->runnable->Run(init->thread);
  } else {
    init->thread->Run();
  }
  ThreadManager::Instance()->SetCurrentThread(nullptr);
  delete init;
#ifdef WEBRTC_WIN
  return 0;
#else
  return nullptr;
#endif
}

void Thread::Run() {
  ProcessMessages(kForever);
}

bool Thread::IsOwned() {
  RTC_DCHECK(IsRunning());
  return owned_;
}

void Thread::Stop() {
  MessageQueue::Quit();
  Join();
}

void Thread::Send(const Location& posted_from,
                  MessageHandler* phandler,
                  uint32_t id,
                  MessageData* pdata) {
  if (IsQuitting())
    return;

  // Sent messages are sent to the MessageHandler directly, in the context
  // of "thread", like Win32 SendMessage. If in the right context,
  // call the handler directly.
  Message msg;
  msg.posted_from = posted_from;
  msg.phandler = phandler;
  msg.message_id = id;
  msg.pdata = pdata;
  if (IsCurrent()) {
    phandler->OnMessage(&msg);
    return;
  }

  AssertBlockingIsAllowedOnCurrentThread();

  AutoThread thread;
  Thread* current_thread = Thread::Current();
  RTC_DCHECK(current_thread != nullptr);  // AutoThread ensures this

  bool ready = false;
  {
    CritScope cs(&crit_);
    _SendMessage smsg;
    smsg.thread = current_thread;
    smsg.msg = msg;
    smsg.ready = &ready;
    sendlist_.push_back(smsg);
  }

  // Wait for a reply
  WakeUpSocketServer();

  bool waited = false;
  crit_.Enter();
  while (!ready) {
    crit_.Leave();
    // We need to limit "ReceiveSends" to |this| thread to avoid an arbitrary
    // thread invoking calls on the current thread.
    current_thread->ReceiveSendsFromThread(this);
    current_thread->socketserver()->Wait(kForever, false);
    waited = true;
    crit_.Enter();
  }
  crit_.Leave();

  // Our Wait loop above may have consumed some WakeUp events for this
  // MessageQueue, that weren't relevant to this Send.  Losing these WakeUps can
  // cause problems for some SocketServers.
  //
  // Concrete example:
  // Win32SocketServer on thread A calls Send on thread B.  While processing the
  // message, thread B Posts a message to A.  We consume the wakeup for that
  // Post while waiting for the Send to complete, which means that when we exit
  // this loop, we need to issue another WakeUp, or else the Posted message
  // won't be processed in a timely manner.

  if (waited) {
    current_thread->socketserver()->WakeUp();
  }
}

void Thread::ReceiveSends() {
  ReceiveSendsFromThread(nullptr);
}

void Thread::ReceiveSendsFromThread(const Thread* source) {
  // Receive a sent message. Cleanup scenarios:
  // - thread sending exits: We don't allow this, since thread can exit
  //   only via Join, so Send must complete.
  // - thread receiving exits: Wakeup/set ready in Thread::Clear()
  // - object target cleared: Wakeup/set ready in Thread::Clear()
  _SendMessage smsg;

  crit_.Enter();
  while (PopSendMessageFromThread(source, &smsg)) {
    crit_.Leave();

    Dispatch(&smsg.msg);

    crit_.Enter();
    *smsg.ready = true;
    smsg.thread->socketserver()->WakeUp();
  }
  crit_.Leave();
}

bool Thread::PopSendMessageFromThread(const Thread* source, _SendMessage* msg) {
  for (std::list<_SendMessage>::iterator it = sendlist_.begin();
       it != sendlist_.end(); ++it) {
    if (it->thread == source || source == nullptr) {
      *msg = *it;
      sendlist_.erase(it);
      return true;
    }
  }
  return false;
}

void Thread::InvokeInternal(const Location& posted_from,
                            MessageHandler* handler) {
  TRACE_EVENT2("webrtc", "Thread::Invoke", "src_file_and_line",
               posted_from.file_and_line(), "src_func",
               posted_from.function_name());
  Send(posted_from, handler);
}

bool Thread::IsProcessingMessagesForTesting() {
  return (owned_ || IsCurrent()) &&
         MessageQueue::IsProcessingMessagesForTesting();
}

void Thread::Clear(MessageHandler* phandler,
                   uint32_t id,
                   MessageList* removed) {
  CritScope cs(&crit_);

  // Remove messages on sendlist_ with phandler
  // Object target cleared: remove from send list, wakeup/set ready
  // if sender not null.

  std::list<_SendMessage>::iterator iter = sendlist_.begin();
  while (iter != sendlist_.end()) {
    _SendMessage smsg = *iter;
    if (smsg.msg.Match(phandler, id)) {
      if (removed) {
        removed->push_back(smsg.msg);
      } else {
        delete smsg.msg.pdata;
      }
      iter = sendlist_.erase(iter);
      *smsg.ready = true;
      smsg.thread->socketserver()->WakeUp();
      continue;
    }
    ++iter;
  }

  ClearInternal(phandler, id, removed);
}

bool Thread::ProcessMessages(int cmsLoop) {
  // Using ProcessMessages with a custom clock for testing and a time greater
  // than 0 doesn't work, since it's not guaranteed to advance the custom
  // clock's time, and may get stuck in an infinite loop.
  RTC_DCHECK(GetClockForTesting() == nullptr || cmsLoop == 0 ||
             cmsLoop == kForever);
  int64_t msEnd = (kForever == cmsLoop) ? 0 : TimeAfter(cmsLoop);
  int cmsNext = cmsLoop;

  while (true) {
#if defined(WEBRTC_MAC)
    ScopedAutoReleasePool pool;
#endif
    Message msg;
    if (!Get(&msg, cmsNext))
      return !IsQuitting();
    Dispatch(&msg);

    if (cmsLoop != kForever) {
      cmsNext = static_cast<int>(TimeUntil(msEnd));
      if (cmsNext < 0)
        return true;
    }
  }
}

bool Thread::WrapCurrentWithThreadManager(ThreadManager* thread_manager,
                                          bool need_synchronize_access) {
  RTC_DCHECK(!IsRunning());

#if defined(WEBRTC_WIN)
  if (need_synchronize_access) {
    // We explicitly ask for no rights other than synchronization.
    // This gives us the best chance of succeeding.
    thread_ = OpenThread(SYNCHRONIZE, FALSE, GetCurrentThreadId());
    if (!thread_) {
      RTC_LOG_GLE(LS_ERROR) << "Unable to get handle to thread.";
      return false;
    }
    thread_id_ = GetCurrentThreadId();
  }
#elif defined(WEBRTC_POSIX)
  thread_ = pthread_self();
#endif
  owned_ = false;
  thread_manager->SetCurrentThread(this);
  return true;
}

bool Thread::IsRunning() {
#if defined(WEBRTC_WIN)
  return thread_ != nullptr;
#elif defined(WEBRTC_POSIX)
  return thread_ != 0;
#endif
}

AutoThread::AutoThread()
    : Thread(SocketServer::CreateDefault(), /*do_init=*/false) {
  DoInit();
  if (!ThreadManager::Instance()->CurrentThread()) {
    ThreadManager::Instance()->SetCurrentThread(this);
  }
}

AutoThread::~AutoThread() {
  Stop();
  DoDestroy();
  if (ThreadManager::Instance()->CurrentThread() == this) {
    ThreadManager::Instance()->SetCurrentThread(nullptr);
  }
}

AutoSocketServerThread::AutoSocketServerThread(SocketServer* ss)
    : Thread(ss, /*do_init=*/false) {
  DoInit();
  old_thread_ = ThreadManager::Instance()->CurrentThread();
  // Temporarily set the current thread to nullptr so that we can keep checks
  // around that catch unintentional pointer overwrites.
  rtc::ThreadManager::Instance()->SetCurrentThread(nullptr);
  rtc::ThreadManager::Instance()->SetCurrentThread(this);
  if (old_thread_) {
    MessageQueueManager::Remove(old_thread_);
  }
}

AutoSocketServerThread::~AutoSocketServerThread() {
  RTC_DCHECK(ThreadManager::Instance()->CurrentThread() == this);
  // Some tests post destroy messages to this thread. To avoid memory
  // leaks, we have to process those messages. In particular
  // P2PTransportChannelPingTest, relying on the message posted in
  // cricket::Connection::Destroy.
  ProcessMessages(0);
  // Stop and destroy the thread before clearing it as the current thread.
  // Sometimes there are messages left in the MessageQueue that will be
  // destroyed by DoDestroy, and sometimes the destructors of the message and/or
  // its contents rely on this thread still being set as the current thread.
  Stop();
  DoDestroy();
  rtc::ThreadManager::Instance()->SetCurrentThread(nullptr);
  rtc::ThreadManager::Instance()->SetCurrentThread(old_thread_);
  if (old_thread_) {
    MessageQueueManager::Add(old_thread_);
  }
}

}  // namespace rtc
