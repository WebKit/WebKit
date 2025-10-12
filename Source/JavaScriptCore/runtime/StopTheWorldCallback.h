/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <utility>
#include <wtf/IterationStatus.h>

namespace JSC {

class VM;

using StopTheWorldStatus = std::pair<IterationStatus, VM*>;

#define STW_RESUME_ALL_TOKEN reinterpret_cast<VM*>(1)

// StopTheWorldCallback return values:
#define STW_CONTINUE() StopTheWorldStatus(IterationStatus::Continue, nullptr)
#define STW_CONTEXT_SWITCH(targetVM) StopTheWorldStatus(IterationStatus::Continue, targetVM)
#define STW_RESUME_ONE(targetVM) StopTheWorldStatus(IterationStatus::Done, targetVM)
#define STW_RESUME_ALL() StopTheWorldStatus(IterationStatus::Done, STW_RESUME_ALL_TOKEN)
#define STW_RESUME() StopTheWorldStatus(IterationStatus::Done, nullptr)

enum class StopTheWorldEvent : uint8_t {
    VMCreated,
    VMActivated,
    VMStopped,
};

// The VMManager Stop the World (STW) mechanism will call handlers of this shape once the world
// is stopped. The handler is expected to return one of the above StopTheWorldStatus results.
//
// STW_CONTINUE means that the handler expects to be called again on the same thread, unless
// an external agent requestResumeAll. In practice, this result is not really useful except
// for tests.
//
// STW_CONTEXT_SWITCH means that the handler wants to switch to another thread as specified
// by the targetVM. The VMManager will stop the current thread, and call the handler back
// from the targetVM thread while all threads remain stopped (Stopped mode).
//
// STW_RESUME_ONE means that the handler wants a specific thread to start running while all
// other VM threads remain stopped (RunOne mode). This may or may not result in a deadlock,
// as the targetVM thread to run may be blocked on resources held by other VM threads that
// remain stopped. It is the responsibility of the client to detect if this occurs (perhaps
// with a timeout), and call VMManager::requestResumeAll() to unblock the deadlock of necessary.
//
// STW_RESUME_ALL means that the handler wants all VM threads to resume execution after
// this (RunAll mode).
//
// STW_RESUME means that the handler wants to resume execution with previous mode (either
// RunAll or RunOne mode).

using StopTheWorldCallback = StopTheWorldStatus (*)(VM&, StopTheWorldEvent);

} // namespace JSC
