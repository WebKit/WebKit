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

#include "config.h"
#include "SwiftCxxInteropTestbed.h"

#include <optional>
#include <wtf/Assertions.h>

namespace SwiftCxxInteropTestbed {

// Handed to a stored handler that resetStoredIntCompletionHandler() has to call itself.
static constexpr int teardownValue = -2;

// Left behind in a probe that has been moved from, so a test sees an obviously wrong value rather than a
// plausible one if the bridge hands Swift the source of a move instead of the destination.
static constexpr int movedFromValue = -1;

static int& liveMoveOnlyProbes()
{
    static int count = 0;
    return count;
}

MoveOnlyProbe::MoveOnlyProbe(int value)
    : m_value(value)
{
    ++liveMoveOnlyProbes();
}

MoveOnlyProbe::MoveOnlyProbe(MoveOnlyProbe&& other)
    : m_value(other.m_value)
{
    other.m_value = movedFromValue;
    ++liveMoveOnlyProbes();
}

MoveOnlyProbe::~MoveOnlyProbe()
{
    --liveMoveOnlyProbes();
}

int liveMoveOnlyProbeCount()
{
    return liveMoveOnlyProbes();
}

static int& liveCopyCountingProbes()
{
    static int count = 0;
    return count;
}

static int& copyCountingProbeCopies()
{
    static int count = 0;
    return count;
}

CopyCountingProbe::CopyCountingProbe(int value)
    : m_value(value)
{
    ++liveCopyCountingProbes();
}

CopyCountingProbe::CopyCountingProbe(const CopyCountingProbe& other)
    : m_value(other.m_value)
{
    ++copyCountingProbeCopies();
    ++liveCopyCountingProbes();
}

CopyCountingProbe::CopyCountingProbe(CopyCountingProbe&& other)
    : m_value(other.m_value)
{
    other.m_value = movedFromValue;
    ++liveCopyCountingProbes();
}

CopyCountingProbe::~CopyCountingProbe()
{
    --liveCopyCountingProbes();
}

int liveCopyCountingProbeCount()
{
    return liveCopyCountingProbes();
}

int copyCountingProbeCopyCount()
{
    return copyCountingProbeCopies();
}

void resetCopyCountingProbeCounts()
{
    copyCountingProbeCopies() = 0;
}

int callIntBoolFunction(bool argument, IntBoolFunction&& function)
{
    return function(argument);
}

void callIntCompletionHandler(int argument, IntCompletionHandler&& completionHandler)
{
    completionHandler(argument);
}

void callVoidCompletionHandler(VoidCompletionHandler&& completionHandler)
{
    completionHandler();
}

static std::optional<IntCompletionHandler>& storedIntCompletionHandler()
{
    static std::optional<IntCompletionHandler> handler;
    return handler;
}

void storeIntCompletionHandler(IntCompletionHandler&& completionHandler)
{
    // Overwriting the slot would destroy an uncalled handler: that trips
    // ~CompletionHandler's assertion and strands whatever the handler owned, so a test
    // awaiting a continuation it held would hang instead of failing.
    RELEASE_ASSERT(!storedIntCompletionHandler());
    storedIntCompletionHandler().emplace(WTF::move(completionHandler));
}

void invokeStoredIntCompletionHandler(int argument)
{
    auto handler = WTF::move(storedIntCompletionHandler());
    storedIntCompletionHandler().reset();

    if (handler)
        (*handler)(argument);
}

void resetStoredIntCompletionHandler()
{
    auto handler = WTF::move(storedIntCompletionHandler());
    storedIntCompletionHandler().reset();

    // Call rather than just drop it, both because ~CompletionHandler requires it and so
    // that a test which returned early fails on the sentinel instead of hanging. Teardown
    // must never assert itself, so check the handler has not already run.
    if (handler && *handler)
        (*handler)(teardownValue);
}

void callMoveOnlyProbeCompletionHandler(int argument, MoveOnlyProbeCompletionHandler&& completionHandler)
{
    completionHandler(MoveOnlyProbe { argument });
}

void callCopyCountingProbeCompletionHandler(int argument, CopyCountingProbeCompletionHandler&& completionHandler)
{
    completionHandler(CopyCountingProbe { argument });
}

static std::optional<MoveOnlyProbeCompletionHandler>& storedMoveOnlyProbeCompletionHandler()
{
    static std::optional<MoveOnlyProbeCompletionHandler> handler;
    return handler;
}

void storeMoveOnlyProbeCompletionHandler(MoveOnlyProbeCompletionHandler&& completionHandler)
{
    // See storeIntCompletionHandler().
    RELEASE_ASSERT(!storedMoveOnlyProbeCompletionHandler());
    storedMoveOnlyProbeCompletionHandler().emplace(WTF::move(completionHandler));
}

void invokeStoredMoveOnlyProbeCompletionHandler(int argument)
{
    auto handler = WTF::move(storedMoveOnlyProbeCompletionHandler());
    storedMoveOnlyProbeCompletionHandler().reset();

    if (handler)
        (*handler)(MoveOnlyProbe { argument });
}

void resetStoredMoveOnlyProbeCompletionHandler()
{
    auto handler = WTF::move(storedMoveOnlyProbeCompletionHandler());
    storedMoveOnlyProbeCompletionHandler().reset();

    // See resetStoredIntCompletionHandler().
    if (handler && *handler)
        (*handler)(MoveOnlyProbe { teardownValue });
}

static int& sharedProbeRefs()
{
    static int count = 0;
    return count;
}

static int& sharedProbeDerefs()
{
    static int count = 0;
    return count;
}

void SharedProbe::ref()
{
    ++sharedProbeRefs();
    ++m_refCount;
}

void SharedProbe::deref()
{
    ++sharedProbeDerefs();
    --m_refCount;

    // Deliberately never freed: see the class comment. A test that drove the count to zero has to be
    // able to report that rather than crash on the next access.
}

static SharedProbe& sharedProbe()
{
    static SharedProbe probe;
    return probe;
}

int callSharedProbeHolderCompletionHandler(SharedProbeHolderCompletionHandler&& completionHandler)
{
    completionHandler(SharedProbeHolder { &sharedProbe() });
    return sharedProbe().refCount();
}

int sharedProbeRefCalls()
{
    return sharedProbeRefs();
}

int sharedProbeDerefCalls()
{
    return sharedProbeDerefs();
}

void resetSharedProbe()
{
    sharedProbeRefs() = 0;
    sharedProbeDerefs() = 0;
    sharedProbe().resetRefCount();
}

SelfReferentialProbe::SelfReferentialProbe(int value)
    : m_value(value)
    , m_self(&m_value)
{
}

SelfReferentialProbe::SelfReferentialProbe(SelfReferentialProbe&& other)
    : m_value(other.m_value)
    , m_self(&m_value)
{
    other.m_value = movedFromValue;
}

SelfReferentialProbe::~SelfReferentialProbe()
{
    // Non-trivial on purpose. See the class comment: this is what makes Swift relocate the value with
    // the move constructor above instead of copying its bytes.
    m_self = nullptr;
}

void callSelfReferentialProbeCompletionHandler(int argument, SelfReferentialProbeCompletionHandler&& completionHandler)
{
    completionHandler(SelfReferentialProbe { argument });
}

};
