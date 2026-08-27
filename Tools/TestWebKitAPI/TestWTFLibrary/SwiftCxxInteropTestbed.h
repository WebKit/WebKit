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

#pragma once

#ifdef __cplusplus

#import <wtf/CompletionHandler.h>
#import <wtf/Function.h>
#import <wtf/StdLibExtras.h>

namespace SwiftCxxInteropTestbed {

// MARK: Types

// Move-only, so that Swift imports it as `~Copyable`.
class MoveOnlyProbe {
public:
    explicit MoveOnlyProbe(int);
    MoveOnlyProbe(MoveOnlyProbe&&);
    MoveOnlyProbe(const MoveOnlyProbe&) = delete;
    MoveOnlyProbe& operator=(const MoveOnlyProbe&) = delete;
    ~MoveOnlyProbe();

    int value() const { return m_value; }

private:
    int m_value;
};

// Copyable but not trivially copyable, like WTF::String or WTF::Vector: the bridge has to run the copy
// constructor for one of these rather than copying its bytes.
class CopyCountingProbe {
public:
    explicit CopyCountingProbe(int);
    CopyCountingProbe(const CopyCountingProbe&);
    CopyCountingProbe(CopyCountingProbe&&);
    ~CopyCountingProbe();

    int value() const { return m_value; }

private:
    int m_value;
};

// Refcounted, and imported by Swift as a managed reference. C++ and Swift disagree about who owns a
// raw pointer to one of these, so the bridge has to let Swift copy any argument holding one rather
// than take it. Nothing is ever freed, so a miscount fails an assertion instead of crashing the
// test binary.
class SharedProbe {
public:
    void ref();
    void deref();

    int refCount() const { return m_refCount; }
    void resetRefCount() { m_refCount = 1; }

private:
    int m_refCount { 1 };
} SWIFT_SHARED_REFERENCE(refSharedProbe, derefSharedProbe);

// Trivially copyable in C++, so its copy constructor does not retain, but Swift imports `probe` as a
// managed reference that Swift will release.
struct SharedProbeHolder {
    SharedProbe* probe;
};

// Holds a pointer into itself, so relocating it byte-wise would leave that pointer dangling. Its
// non-trivial destructor is what makes Swift relocate it with the move constructor instead, which is
// the property WTF::CompletionHandler's static_assert relies on.
class SelfReferentialProbe {
public:
    explicit SelfReferentialProbe(int);
    SelfReferentialProbe(SelfReferentialProbe&&);
    SelfReferentialProbe(const SelfReferentialProbe&) = delete;
    SelfReferentialProbe& operator=(const SelfReferentialProbe&) = delete;
    ~SelfReferentialProbe();

    bool interiorPointerIsValid() const { return m_self == &m_value; }
    int valueThroughInteriorPointer() const { return *m_self; }

private:
    int m_value;
    int* m_self;
};

// MARK: Using declarations

using IntBoolFunction = WTF::Function<int(bool)>;

using IntCompletionHandler = WTF::CompletionHandler<void(int)>;
using VoidCompletionHandler = WTF::CompletionHandler<void()>;

using MoveOnlyProbeCompletionHandler = WTF::CompletionHandler<void(MoveOnlyProbe&&)>;
using CopyCountingProbeCompletionHandler = WTF::CompletionHandler<void(CopyCountingProbe)>;
using SharedProbeHolderCompletionHandler = WTF::CompletionHandler<void(SharedProbeHolder)>;
using SelfReferentialProbeCompletionHandler = WTF::CompletionHandler<void(SelfReferentialProbe&&)>;

// MARK: Function declarations

int callIntBoolFunction(bool, IntBoolFunction&&);

void callIntCompletionHandler(int, IntCompletionHandler&&);

void callVoidCompletionHandler(VoidCompletionHandler&&);

void storeIntCompletionHandler(IntCompletionHandler&&);
void invokeStoredIntCompletionHandler(int);

// Call from test teardown: storing a handler and returning without invoking it would
// otherwise leave it stranded for the next test to trip over.
void resetStoredIntCompletionHandler();

void callMoveOnlyProbeCompletionHandler(int, MoveOnlyProbeCompletionHandler&&);

void storeMoveOnlyProbeCompletionHandler(MoveOnlyProbeCompletionHandler&&);
void invokeStoredMoveOnlyProbeCompletionHandler(int);
void resetStoredMoveOnlyProbeCompletionHandler();

// A copyable argument reaches Swift as a copy, so exactly one copy constructor call should be observable.
void callCopyCountingProbeCompletionHandler(int, CopyCountingProbeCompletionHandler&&);

// Zero once every probe the bridge constructed has been destroyed, so a test can prove the argument's
// destructor ran exactly once on the way across.
int liveMoveOnlyProbeCount();
int liveCopyCountingProbeCount();

// How many times CopyCountingProbe's copy constructor has run since resetCopyCountingProbeCounts().
int copyCountingProbeCopyCount();
void resetCopyCountingProbeCounts();

// Hands the handler a borrowed reference to the shared probe and returns its reference count once the
// handler has run. A bridge that leaves the caller's ownership alone returns 1.
int callSharedProbeHolderCompletionHandler(SharedProbeHolderCompletionHandler&&);

// How many times the bridge caused a retain or a release. These must match.
int sharedProbeRefCalls();
int sharedProbeDerefCalls();
void resetSharedProbe();

void callSelfReferentialProbeCompletionHandler(int, SelfReferentialProbeCompletionHandler&&);

}

inline void refSharedProbe(SwiftCxxInteropTestbed::SharedProbe* WTF_NONNULL probe)
{
    probe->ref();
}

inline void derefSharedProbe(SwiftCxxInteropTestbed::SharedProbe* WTF_NONNULL probe)
{
    probe->deref();
}

#endif // __cplusplus
