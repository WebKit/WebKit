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

#include <wtf/ThreadSafetyAnalysis.h>

namespace WTF {

// Functions implementing "lock" capability, declared by WTF_CAPABILITY("lock") or WTF_CAPABILITY_LOCK.

// Implementation of "lock" capability for any such type. Types specialize these templates.
template<typename T> bool lockIsHeld(const T&);
template<typename T> bool lockIsShared(const T&);


// Establishes the fact that `t` is held exclusively. The static analysis will use this fact to verify access
// to functions and member variables depending on this fact.
// On ASSERT_ENABLED builds, proves this claim during run-time by ASSERTing.
template<typename T>
inline void assertIsHeld(const T& lock) WTF_ASSERTS_ACQUIRED_CAPABILITY(lock)
{
    ASSERT_UNUSED(lock, lockIsHeld(lock));
}

// Establishes the fact that `t` is held for reading. The static analysis will use this fact to verify access
// to functions and member variables depending on this fact.
// On ASSERT_ENABLED builds, proves this claim during run-time by ASSERTing.
template<typename T>
inline void assertIsShared(const T& lock) WTF_ASSERTS_ACQUIRED_SHARED_CAPABILITY(lock)
{
    ASSERT_UNUSED(lock, lockIsShared(lock));
}

}

using WTF::assertIsHeld;
using WTF::assertIsShared;
