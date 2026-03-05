/*
 *  Copyright 2025 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SIGSLOT_TRAMPOLINE_H_
#define RTC_BASE_SIGSLOT_TRAMPOLINE_H_

#include <utility>

#include "absl/functional/any_invocable.h"
#include "rtc_base/callback_list.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

namespace webrtc {
// A template to simplify the replacement of sigslot::Signal with a
// CallbackList.

// THIS IS A TEMPORARY OBJECT:
// Once all callers have converted to Subscribe* and Notify*, the signal
// and the trampoline can be replaced with a CallbackList, or, for the case
// where only one listener can ever exist, a simple callback.

// Usage, for class MyClass and signal SignalMyNamedEvent:
// class MyClass {
//   MyClass()
//     : my_named_event_trampoline_(this) {}
//   // existing:
//   sigslot::signal0<> SignalMyNamedEvent;
//   // new, this is what we want callers to use instead
//   void NotifyMyNamedEvent() { SignalMyNamedEvent(); }
//   void SubscribeMyNamedEvent(absl::AnyInvocable<void()> callback) {
//     my_named_event_trampoline_.Subscribe(std::move(callback));
//   }
//   private:
//    SignalTrampoline<MyClass, &MyClass::SignalMyNamedEvent>
//        my_named_event_trampoline_;
//  }
//
// At caller, replace:
//     my_class_object.SignalMyNamedEvent.connect(target, function)
// with:
//     my_class_object.SubscribeMyNamedEvent([target]{ target.function(); }
// Note that the SubscribeMyNamedEvent will NOT guarantee that the target
// continues to exist; if there is any doubt about that, use a SafeInvocable:
//     my_class_object.SubscibeMyNamedEvent(
//         SafeInvocable(target.safety_flag_.flag(),
//                       [target] { target.function(); }
// WHEN REMOVING THE SIGNAL
// Make a CL that will:
// - Delete the SignalMyNamedEvent signal
// - Change my_named_event_trampoline_ to my_named_event_callbacks,
//   of type CallbackList<function arguments>
// - Change SubscribeMyNamedEvent to call my_named_event_callbacks.AddReceiver
// - Change NotifyMyNamedEvent to call my_named_event_callbacks_.Send
// - If UnsubscribeMyNamedEvent exists, change that also.
// - Delete the initialization of my_named_event_trampoline_ from the
//   relevant constructor.
// Sending this through the bots will flush out remaining references to
// SignalMyNamedEvent.
namespace internal {
template <typename MemberPtrT>
struct member_pointer_traits;

// Used to find the type of ClassT::Member
template <typename ClassT, typename MemberT>
struct member_pointer_traits<MemberT ClassT::*> {
  using member_type = MemberT;
};

template <typename SignalT>
class SignalTrampolineBase;

template <typename... Args>
class SignalTrampolineBase<sigslot::signal<Args...>>
    : public sigslot::has_slots<> {
 public:
  void Subscribe(absl::AnyInvocable<void(Args...)> callback) {
    callbacks_.AddReceiver(std::move(callback));
  }
  void Subscribe(const void* tag, absl::AnyInvocable<void(Args...)> callback) {
    callbacks_.AddReceiver(tag, std::move(callback));
  }
  void Unsubscribe(const void* tag) { callbacks_.RemoveReceivers(tag); }
  void Notify(Args... args) { callbacks_.Send(args...); }

 private:
  CallbackList<Args...> callbacks_;
};

template <typename T, auto member_signal>
using SignalTrampolineMemberBase =
    SignalTrampolineBase<typename internal::member_pointer_traits<
        decltype(member_signal)>::member_type>;

}  // namespace internal

template <class T, auto member_signal>
class SignalTrampoline
    : public internal::SignalTrampolineMemberBase<T, member_signal> {
 private:
  using Base = internal::SignalTrampolineMemberBase<T, member_signal>;

 public:
  explicit SignalTrampoline(T* that) {
    (that->*member_signal).connect(static_cast<Base*>(this), &Base::Notify);
  }
};

}  // namespace webrtc

#endif  // RTC_BASE_SIGSLOT_TRAMPOLINE_H_
