/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains Macros for creating proxies for webrtc MediaStream and
// PeerConnection classes.
// TODO(deadbeef): Move this to pc/; this is part of the implementation.

//
// Example usage:
//
// class TestInterface : public rtc::RefCountInterface {
//  public:
//   std::string FooA() = 0;
//   std::string FooB(bool arg1) const = 0;
//   std::string FooC(bool arg1) = 0;
//  };
//
// Note that return types can not be a const reference.
//
// class Test : public TestInterface {
// ... implementation of the interface.
// };
//
// BEGIN_PROXY_MAP(Test)
//   PROXY_SIGNALING_THREAD_DESTRUCTOR()
//   PROXY_METHOD0(std::string, FooA)
//   PROXY_CONSTMETHOD1(std::string, FooB, arg1)
//   PROXY_WORKER_METHOD1(std::string, FooC, arg1)
// END_PROXY_MAP()
//
// Where the destructor and first two methods are invoked on the signaling
// thread, and the third is invoked on the worker thread.
//
// The proxy can be created using
//
//   TestProxy::Create(Thread* signaling_thread, Thread* worker_thread,
//                     TestInterface*).
//
// The variant defined with BEGIN_SIGNALING_PROXY_MAP is unaware of
// the worker thread, and invokes all methods on the signaling thread.
//
// The variant defined with BEGIN_OWNED_PROXY_MAP does not use
// refcounting, and instead just takes ownership of the object being proxied.

#ifndef API_PROXY_H_
#define API_PROXY_H_

#include <memory>
#include <string>
#include <utility>

#include "api/scoped_refptr.h"
#include "rtc_base/event.h"
#include "rtc_base/message_handler.h"
#include "rtc_base/message_queue.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/thread.h"

namespace rtc {
class Location;
}

namespace webrtc {

template <typename R>
class ReturnType {
 public:
  template <typename C, typename M>
  void Invoke(C* c, M m) {
    r_ = (c->*m)();
  }
  template <typename C, typename M, typename T1>
  void Invoke(C* c, M m, T1 a1) {
    r_ = (c->*m)(std::move(a1));
  }
  template <typename C, typename M, typename T1, typename T2>
  void Invoke(C* c, M m, T1 a1, T2 a2) {
    r_ = (c->*m)(std::move(a1), std::move(a2));
  }
  template <typename C, typename M, typename T1, typename T2, typename T3>
  void Invoke(C* c, M m, T1 a1, T2 a2, T3 a3) {
    r_ = (c->*m)(std::move(a1), std::move(a2), std::move(a3));
  }
  template <typename C,
            typename M,
            typename T1,
            typename T2,
            typename T3,
            typename T4>
  void Invoke(C* c, M m, T1 a1, T2 a2, T3 a3, T4 a4) {
    r_ = (c->*m)(std::move(a1), std::move(a2), std::move(a3), std::move(a4));
  }
  template <typename C,
            typename M,
            typename T1,
            typename T2,
            typename T3,
            typename T4,
            typename T5>
  void Invoke(C* c, M m, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5) {
    r_ = (c->*m)(std::move(a1), std::move(a2), std::move(a3), std::move(a4),
                 std::move(a5));
  }

  R moved_result() { return std::move(r_); }

 private:
  R r_;
};

template <>
class ReturnType<void> {
 public:
  template <typename C, typename M>
  void Invoke(C* c, M m) {
    (c->*m)();
  }
  template <typename C, typename M, typename T1>
  void Invoke(C* c, M m, T1 a1) {
    (c->*m)(std::move(a1));
  }
  template <typename C, typename M, typename T1, typename T2>
  void Invoke(C* c, M m, T1 a1, T2 a2) {
    (c->*m)(std::move(a1), std::move(a2));
  }
  template <typename C, typename M, typename T1, typename T2, typename T3>
  void Invoke(C* c, M m, T1 a1, T2 a2, T3 a3) {
    (c->*m)(std::move(a1), std::move(a2), std::move(a3));
  }

  void moved_result() {}
};

namespace internal {

class SynchronousMethodCall : public rtc::MessageData,
                              public rtc::MessageHandler {
 public:
  explicit SynchronousMethodCall(rtc::MessageHandler* proxy);
  ~SynchronousMethodCall() override;

  void Invoke(const rtc::Location& posted_from, rtc::Thread* t);

 private:
  void OnMessage(rtc::Message*) override;

  rtc::Event e_;
  rtc::MessageHandler* proxy_;
};

}  // namespace internal

template <typename C, typename R>
class MethodCall0 : public rtc::Message, public rtc::MessageHandler {
 public:
  typedef R (C::*Method)();
  MethodCall0(C* c, Method m) : c_(c), m_(m) {}

  R Marshal(const rtc::Location& posted_from, rtc::Thread* t) {
    internal::SynchronousMethodCall(this).Invoke(posted_from, t);
    return r_.moved_result();
  }

 private:
  void OnMessage(rtc::Message*) { r_.Invoke(c_, m_); }

  C* c_;
  Method m_;
  ReturnType<R> r_;
};

template <typename C, typename R>
class ConstMethodCall0 : public rtc::Message, public rtc::MessageHandler {
 public:
  typedef R (C::*Method)() const;
  ConstMethodCall0(C* c, Method m) : c_(c), m_(m) {}

  R Marshal(const rtc::Location& posted_from, rtc::Thread* t) {
    internal::SynchronousMethodCall(this).Invoke(posted_from, t);
    return r_.moved_result();
  }

 private:
  void OnMessage(rtc::Message*) { r_.Invoke(c_, m_); }

  C* c_;
  Method m_;
  ReturnType<R> r_;
};

template <typename C, typename R, typename T1>
class MethodCall1 : public rtc::Message, public rtc::MessageHandler {
 public:
  typedef R (C::*Method)(T1 a1);
  MethodCall1(C* c, Method m, T1 a1) : c_(c), m_(m), a1_(std::move(a1)) {}

  R Marshal(const rtc::Location& posted_from, rtc::Thread* t) {
    internal::SynchronousMethodCall(this).Invoke(posted_from, t);
    return r_.moved_result();
  }

 private:
  void OnMessage(rtc::Message*) { r_.Invoke(c_, m_, std::move(a1_)); }

  C* c_;
  Method m_;
  ReturnType<R> r_;
  T1 a1_;
};

template <typename C, typename R, typename T1>
class ConstMethodCall1 : public rtc::Message, public rtc::MessageHandler {
 public:
  typedef R (C::*Method)(T1 a1) const;
  ConstMethodCall1(C* c, Method m, T1 a1) : c_(c), m_(m), a1_(std::move(a1)) {}

  R Marshal(const rtc::Location& posted_from, rtc::Thread* t) {
    internal::SynchronousMethodCall(this).Invoke(posted_from, t);
    return r_.moved_result();
  }

 private:
  void OnMessage(rtc::Message*) { r_.Invoke(c_, m_, std::move(a1_)); }

  C* c_;
  Method m_;
  ReturnType<R> r_;
  T1 a1_;
};

template <typename C, typename R, typename T1, typename T2>
class MethodCall2 : public rtc::Message, public rtc::MessageHandler {
 public:
  typedef R (C::*Method)(T1 a1, T2 a2);
  MethodCall2(C* c, Method m, T1 a1, T2 a2)
      : c_(c), m_(m), a1_(std::move(a1)), a2_(std::move(a2)) {}

  R Marshal(const rtc::Location& posted_from, rtc::Thread* t) {
    internal::SynchronousMethodCall(this).Invoke(posted_from, t);
    return r_.moved_result();
  }

 private:
  void OnMessage(rtc::Message*) {
    r_.Invoke(c_, m_, std::move(a1_), std::move(a2_));
  }

  C* c_;
  Method m_;
  ReturnType<R> r_;
  T1 a1_;
  T2 a2_;
};

template <typename C, typename R, typename T1, typename T2, typename T3>
class MethodCall3 : public rtc::Message, public rtc::MessageHandler {
 public:
  typedef R (C::*Method)(T1 a1, T2 a2, T3 a3);
  MethodCall3(C* c, Method m, T1 a1, T2 a2, T3 a3)
      : c_(c),
        m_(m),
        a1_(std::move(a1)),
        a2_(std::move(a2)),
        a3_(std::move(a3)) {}

  R Marshal(const rtc::Location& posted_from, rtc::Thread* t) {
    internal::SynchronousMethodCall(this).Invoke(posted_from, t);
    return r_.moved_result();
  }

 private:
  void OnMessage(rtc::Message*) {
    r_.Invoke(c_, m_, std::move(a1_), std::move(a2_), std::move(a3_));
  }

  C* c_;
  Method m_;
  ReturnType<R> r_;
  T1 a1_;
  T2 a2_;
  T3 a3_;
};

template <typename C,
          typename R,
          typename T1,
          typename T2,
          typename T3,
          typename T4>
class MethodCall4 : public rtc::Message, public rtc::MessageHandler {
 public:
  typedef R (C::*Method)(T1 a1, T2 a2, T3 a3, T4 a4);
  MethodCall4(C* c, Method m, T1 a1, T2 a2, T3 a3, T4 a4)
      : c_(c),
        m_(m),
        a1_(std::move(a1)),
        a2_(std::move(a2)),
        a3_(std::move(a3)),
        a4_(std::move(a4)) {}

  R Marshal(const rtc::Location& posted_from, rtc::Thread* t) {
    internal::SynchronousMethodCall(this).Invoke(posted_from, t);
    return r_.moved_result();
  }

 private:
  void OnMessage(rtc::Message*) {
    r_.Invoke(c_, m_, std::move(a1_), std::move(a2_), std::move(a3_),
              std::move(a4_));
  }

  C* c_;
  Method m_;
  ReturnType<R> r_;
  T1 a1_;
  T2 a2_;
  T3 a3_;
  T4 a4_;
};

template <typename C,
          typename R,
          typename T1,
          typename T2,
          typename T3,
          typename T4,
          typename T5>
class MethodCall5 : public rtc::Message, public rtc::MessageHandler {
 public:
  typedef R (C::*Method)(T1 a1, T2 a2, T3 a3, T4 a4, T5 a5);
  MethodCall5(C* c, Method m, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5)
      : c_(c),
        m_(m),
        a1_(std::move(a1)),
        a2_(std::move(a2)),
        a3_(std::move(a3)),
        a4_(std::move(a4)),
        a5_(std::move(a5)) {}

  R Marshal(const rtc::Location& posted_from, rtc::Thread* t) {
    internal::SynchronousMethodCall(this).Invoke(posted_from, t);
    return r_.moved_result();
  }

 private:
  void OnMessage(rtc::Message*) {
    r_.Invoke(c_, m_, std::move(a1_), std::move(a2_), std::move(a3_),
              std::move(a4_), std::move(a5_));
  }

  C* c_;
  Method m_;
  ReturnType<R> r_;
  T1 a1_;
  T2 a2_;
  T3 a3_;
  T4 a4_;
  T5 a5_;
};

// Helper macros to reduce code duplication.
#define PROXY_MAP_BOILERPLATE(c)                          \
  template <class INTERNAL_CLASS>                         \
  class c##ProxyWithInternal;                             \
  typedef c##ProxyWithInternal<c##Interface> c##Proxy;    \
  template <class INTERNAL_CLASS>                         \
  class c##ProxyWithInternal : public c##Interface {      \
   protected:                                             \
    typedef c##Interface C;                               \
                                                          \
   public:                                                \
    const INTERNAL_CLASS* internal() const { return c_; } \
    INTERNAL_CLASS* internal() { return c_; }

// clang-format off
// clang-format would put the semicolon alone,
// leading to a presubmit error (cpplint.py)
#define END_PROXY_MAP() \
  };
// clang-format on

#define SIGNALING_PROXY_MAP_BOILERPLATE(c)                               \
 protected:                                                              \
  c##ProxyWithInternal(rtc::Thread* signaling_thread, INTERNAL_CLASS* c) \
      : signaling_thread_(signaling_thread), c_(c) {}                    \
                                                                         \
 private:                                                                \
  mutable rtc::Thread* signaling_thread_;

#define WORKER_PROXY_MAP_BOILERPLATE(c)                               \
 protected:                                                           \
  c##ProxyWithInternal(rtc::Thread* signaling_thread,                 \
                       rtc::Thread* worker_thread, INTERNAL_CLASS* c) \
      : signaling_thread_(signaling_thread),                          \
        worker_thread_(worker_thread),                                \
        c_(c) {}                                                      \
                                                                      \
 private:                                                             \
  mutable rtc::Thread* signaling_thread_;                             \
  mutable rtc::Thread* worker_thread_;

// Note that the destructor is protected so that the proxy can only be
// destroyed via RefCountInterface.
#define REFCOUNTED_PROXY_MAP_BOILERPLATE(c)            \
 protected:                                            \
  ~c##ProxyWithInternal() {                            \
    MethodCall0<c##ProxyWithInternal, void> call(      \
        this, &c##ProxyWithInternal::DestroyInternal); \
    call.Marshal(RTC_FROM_HERE, destructor_thread());  \
  }                                                    \
                                                       \
 private:                                              \
  void DestroyInternal() { c_ = nullptr; }             \
  rtc::scoped_refptr<INTERNAL_CLASS> c_;

// Note: This doesn't use a unique_ptr, because it intends to handle a corner
// case where an object's deletion triggers a callback that calls back into
// this proxy object. If relying on a unique_ptr to delete the object, its
// inner pointer would be set to null before this reentrant callback would have
// a chance to run, resulting in a segfault.
#define OWNED_PROXY_MAP_BOILERPLATE(c)                 \
 public:                                               \
  ~c##ProxyWithInternal() {                            \
    MethodCall0<c##ProxyWithInternal, void> call(      \
        this, &c##ProxyWithInternal::DestroyInternal); \
    call.Marshal(RTC_FROM_HERE, destructor_thread());  \
  }                                                    \
                                                       \
 private:                                              \
  void DestroyInternal() { delete c_; }                \
  INTERNAL_CLASS* c_;

#define BEGIN_SIGNALING_PROXY_MAP(c)                                         \
  PROXY_MAP_BOILERPLATE(c)                                                   \
  SIGNALING_PROXY_MAP_BOILERPLATE(c)                                         \
  REFCOUNTED_PROXY_MAP_BOILERPLATE(c)                                        \
 public:                                                                     \
  static rtc::scoped_refptr<c##ProxyWithInternal> Create(                    \
      rtc::Thread* signaling_thread, INTERNAL_CLASS* c) {                    \
    return new rtc::RefCountedObject<c##ProxyWithInternal>(signaling_thread, \
                                                           c);               \
  }

#define BEGIN_PROXY_MAP(c)                                                    \
  PROXY_MAP_BOILERPLATE(c)                                                    \
  WORKER_PROXY_MAP_BOILERPLATE(c)                                             \
  REFCOUNTED_PROXY_MAP_BOILERPLATE(c)                                         \
 public:                                                                      \
  static rtc::scoped_refptr<c##ProxyWithInternal> Create(                     \
      rtc::Thread* signaling_thread, rtc::Thread* worker_thread,              \
      INTERNAL_CLASS* c) {                                                    \
    return new rtc::RefCountedObject<c##ProxyWithInternal>(signaling_thread,  \
                                                           worker_thread, c); \
  }

#define BEGIN_OWNED_PROXY_MAP(c)                                   \
  PROXY_MAP_BOILERPLATE(c)                                         \
  WORKER_PROXY_MAP_BOILERPLATE(c)                                  \
  OWNED_PROXY_MAP_BOILERPLATE(c)                                   \
 public:                                                           \
  static std::unique_ptr<c##Interface> Create(                     \
      rtc::Thread* signaling_thread, rtc::Thread* worker_thread,   \
      std::unique_ptr<INTERNAL_CLASS> c) {                         \
    return std::unique_ptr<c##Interface>(new c##ProxyWithInternal( \
        signaling_thread, worker_thread, c.release()));            \
  }

#define PROXY_SIGNALING_THREAD_DESTRUCTOR()                            \
 private:                                                              \
  rtc::Thread* destructor_thread() const { return signaling_thread_; } \
                                                                       \
 public:  // NOLINTNEXTLINE

#define PROXY_WORKER_THREAD_DESTRUCTOR()                            \
 private:                                                           \
  rtc::Thread* destructor_thread() const { return worker_thread_; } \
                                                                    \
 public:  // NOLINTNEXTLINE

#define PROXY_METHOD0(r, method)                           \
  r method() override {                                    \
    MethodCall0<C, r> call(c_, &C::method);                \
    return call.Marshal(RTC_FROM_HERE, signaling_thread_); \
  }

#define PROXY_CONSTMETHOD0(r, method)                      \
  r method() const override {                              \
    ConstMethodCall0<C, r> call(c_, &C::method);           \
    return call.Marshal(RTC_FROM_HERE, signaling_thread_); \
  }

#define PROXY_METHOD1(r, method, t1)                           \
  r method(t1 a1) override {                                   \
    MethodCall1<C, r, t1> call(c_, &C::method, std::move(a1)); \
    return call.Marshal(RTC_FROM_HERE, signaling_thread_);     \
  }

#define PROXY_CONSTMETHOD1(r, method, t1)                           \
  r method(t1 a1) const override {                                  \
    ConstMethodCall1<C, r, t1> call(c_, &C::method, std::move(a1)); \
    return call.Marshal(RTC_FROM_HERE, signaling_thread_);          \
  }

#define PROXY_METHOD2(r, method, t1, t2)                          \
  r method(t1 a1, t2 a2) override {                               \
    MethodCall2<C, r, t1, t2> call(c_, &C::method, std::move(a1), \
                                   std::move(a2));                \
    return call.Marshal(RTC_FROM_HERE, signaling_thread_);        \
  }

#define PROXY_METHOD3(r, method, t1, t2, t3)                          \
  r method(t1 a1, t2 a2, t3 a3) override {                            \
    MethodCall3<C, r, t1, t2, t3> call(c_, &C::method, std::move(a1), \
                                       std::move(a2), std::move(a3)); \
    return call.Marshal(RTC_FROM_HERE, signaling_thread_);            \
  }

#define PROXY_METHOD4(r, method, t1, t2, t3, t4)                          \
  r method(t1 a1, t2 a2, t3 a3, t4 a4) override {                         \
    MethodCall4<C, r, t1, t2, t3, t4> call(c_, &C::method, std::move(a1), \
                                           std::move(a2), std::move(a3),  \
                                           std::move(a4));                \
    return call.Marshal(RTC_FROM_HERE, signaling_thread_);                \
  }

#define PROXY_METHOD5(r, method, t1, t2, t3, t4, t5)                          \
  r method(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5) override {                      \
    MethodCall5<C, r, t1, t2, t3, t4, t5> call(c_, &C::method, std::move(a1), \
                                               std::move(a2), std::move(a3),  \
                                               std::move(a4), std::move(a5)); \
    return call.Marshal(RTC_FROM_HERE, signaling_thread_);                    \
  }

// Define methods which should be invoked on the worker thread.
#define PROXY_WORKER_METHOD0(r, method)                 \
  r method() override {                                 \
    MethodCall0<C, r> call(c_, &C::method);             \
    return call.Marshal(RTC_FROM_HERE, worker_thread_); \
  }

#define PROXY_WORKER_CONSTMETHOD0(r, method)            \
  r method() const override {                           \
    ConstMethodCall0<C, r> call(c_, &C::method);        \
    return call.Marshal(RTC_FROM_HERE, worker_thread_); \
  }

#define PROXY_WORKER_METHOD1(r, method, t1)                    \
  r method(t1 a1) override {                                   \
    MethodCall1<C, r, t1> call(c_, &C::method, std::move(a1)); \
    return call.Marshal(RTC_FROM_HERE, worker_thread_);        \
  }

#define PROXY_WORKER_CONSTMETHOD1(r, method, t1)                    \
  r method(t1 a1) const override {                                  \
    ConstMethodCall1<C, r, t1> call(c_, &C::method, std::move(a1)); \
    return call.Marshal(RTC_FROM_HERE, worker_thread_);             \
  }

#define PROXY_WORKER_METHOD2(r, method, t1, t2)                   \
  r method(t1 a1, t2 a2) override {                               \
    MethodCall2<C, r, t1, t2> call(c_, &C::method, std::move(a1), \
                                   std::move(a2));                \
    return call.Marshal(RTC_FROM_HERE, worker_thread_);           \
  }

#define PROXY_WORKER_CONSTMETHOD2(r, method, t1, t2)                   \
  r method(t1 a1, t2 a2) const override {                              \
    ConstMethodCall2<C, r, t1, t2> call(c_, &C::method, std::move(a1), \
                                        std::move(a2));                \
    return call.Marshal(RTC_FROM_HERE, worker_thread_);                \
  }

#define PROXY_WORKER_METHOD3(r, method, t1, t2, t3)                   \
  r method(t1 a1, t2 a2, t3 a3) override {                            \
    MethodCall3<C, r, t1, t2, t3> call(c_, &C::method, std::move(a1), \
                                       std::move(a2), std::move(a3)); \
    return call.Marshal(RTC_FROM_HERE, worker_thread_);               \
  }

#define PROXY_WORKER_CONSTMETHOD3(r, method, t1, t2)                       \
  r method(t1 a1, t2 a2, t3 a3) const override {                           \
    ConstMethodCall3<C, r, t1, t2, t3> call(c_, &C::method, std::move(a1), \
                                            std::move(a2), std::move(a3)); \
    return call.Marshal(RTC_FROM_HERE, worker_thread_);                    \
  }

}  // namespace webrtc

#endif  //  API_PROXY_H_
