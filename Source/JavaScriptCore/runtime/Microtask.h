/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/ObjectIdentifier.h>

namespace JSC {

enum class MicrotaskIdentifierType { };
using MicrotaskIdentifier = ObjectIdentifier<MicrotaskIdentifierType>;

enum class InternalMicrotask : uint8_t {
    PromiseResolveThenableJobFast = 0,
    PromiseResolveThenableJobWithInternalMicrotaskFast,

    PromiseResolveThenableJob,
    PromiseResolveThenableJobWithInternalMicrotask,

    PromiseResolveWithoutHandlerJob,

    PromiseRaceResolveJob,
    PromiseAllResolveJob,
    PromiseAllSettledResolveJob,
    PromiseAnyResolveJob,
    PromiseFinallyReactionJob,
    PromiseFinallyAwaitJob,

    InternalPromiseAllResolveJob,

    PromiseReactionJob,

    AsyncFunctionResume,
    AsyncFromSyncIteratorContinue,
    AsyncFromSyncIteratorDone,
    AsyncGeneratorYieldAwaited,
    AsyncGeneratorBodyCallNormal,
    AsyncGeneratorBodyCallReturn,
    AsyncGeneratorResumeNext,

    InvokeFunctionJob,
    Opaque, // Dispatch must handle everything.
};

constexpr unsigned maxMicrotaskArguments = 3;

enum class QueuedTaskResult : uint8_t {
    Executed,
    Discard,
    Suspended,
};

} // namespace JSC
