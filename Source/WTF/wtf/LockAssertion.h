/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/CapabilityLock.h>
#include <wtf/Noncopyable.h>

namespace WTF {

// A type to use for asserting that a locking condition for a member function or a variable
// is held.
// Used as:
// struct MyClass {
//      const LockAssertion& myVariableLock() const;
//      int myVariable WTF_GUARDED_BY(myVariableLock()) { 0 };
// };
// MyClass d;
// assertIsShared(d.myVariableLock());
// print(d.myVariable + 6);
// assertIsHeld(d.myVariableLock());
// d.myVariable = 7;
//
// In the above, the myVariableLock() runs the if conditions wrt which access is allowed in the scenario
// and returns the respective LockAssertion. See LockAssertionTests for examples.
class WTF_CAPABILITY_LOCK LockAssertion {
    WTF_MAKE_NONCOPYABLE(LockAssertion);
public:
    WTF_EXPORT_PRIVATE static const LockAssertion exclusive;
    WTF_EXPORT_PRIVATE static const LockAssertion shared;
    WTF_EXPORT_PRIVATE static const LockAssertion unlocked;
    WTF_EXPORT_PRIVATE static const LockAssertion unused; // To be used for cases where condition is not defined for !ASSERT_ENABLED.
    bool operator==(const LockAssertion& other) const { return this == &other; }
private:
    LockAssertion() = default;
};

template<>
inline bool lockIsHeld<LockAssertion>(const LockAssertion& lock)
{
    return lock == LockAssertion::exclusive;
}

template<>
inline bool lockIsShared<LockAssertion>(const LockAssertion& lock)
{
    return lock == LockAssertion::shared || lock == LockAssertion::exclusive;
}

}

using WTF::LockAssertion;
