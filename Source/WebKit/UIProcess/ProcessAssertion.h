/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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

#include <wtf/CompletionHandler.h>
#include <wtf/Function.h>
#include <wtf/ProcessID.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if !OS(WINDOWS)
#include <unistd.h>
#endif

#if PLATFORM(COCOA) && USE(RUNNINGBOARD)
#include <wtf/RetainPtr.h>

OBJC_CLASS RBSAssertion;
OBJC_CLASS WKRBSAssertionDelegate;
#endif // PLATFORM(COCOA) && USE(RUNNINGBOARD)

namespace WebKit {

enum class ProcessAssertionType {
    Suspended,
    Background,
    UnboundedNetworking,
    Foreground,
    MediaPlayback,
    FinishTaskInterruptable,
};

ASCIILiteral processAssertionTypeDescription(ProcessAssertionType);

class ProcessAssertion : public ThreadSafeRefCounted<ProcessAssertion>, public CanMakeWeakPtr<ProcessAssertion, WeakPtrFactoryInitialization::Eager> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Mode : bool { Sync, Async };
    static Ref<ProcessAssertion> create(ProcessID pid, const String& reason, ProcessAssertionType type, Mode mode = Mode::Async, CompletionHandler<void()>&& acquisisionHandler = nullptr)
    {
        auto assertion = adoptRef(*new ProcessAssertion(pid, reason, type));
        if (mode == Mode::Async)
            assertion->acquireAsync(WTFMove(acquisisionHandler));
        else {
            assertion->acquireSync();
            if (acquisisionHandler)
                acquisisionHandler();
        }
        return assertion;
    }
    static double remainingRunTimeInSeconds(ProcessID);
    virtual ~ProcessAssertion();

    void setPrepareForInvalidationHandler(Function<void()>&& handler) { m_prepareForInvalidationHandler = WTFMove(handler); }
    void setInvalidationHandler(Function<void()>&& handler) { m_invalidationHandler = WTFMove(handler); }

    ProcessAssertionType type() const { return m_assertionType; }
    ProcessID pid() const { return m_pid; }

    bool isValid() const;

protected:
    ProcessAssertion(ProcessID, const String& reason, ProcessAssertionType);

    void acquireAsync(CompletionHandler<void()>&&);
    void acquireSync();

#if PLATFORM(COCOA) && USE(RUNNINGBOARD)
    void processAssertionWillBeInvalidated();
    virtual void processAssertionWasInvalidated();
#endif

private:
    const ProcessAssertionType m_assertionType;
    const ProcessID m_pid;
    const String m_reason;
#if PLATFORM(COCOA) && USE(RUNNINGBOARD)
    RetainPtr<RBSAssertion> m_rbsAssertion;
    RetainPtr<WKRBSAssertionDelegate> m_delegate;
    bool m_wasInvalidated { false };
#endif
    Function<void()> m_prepareForInvalidationHandler;
    Function<void()> m_invalidationHandler;
};

class ProcessAndUIAssertion final : public ProcessAssertion {
public:
    static Ref<ProcessAndUIAssertion> create(ProcessID pid, const String& reason, ProcessAssertionType type, Mode mode = Mode::Async, CompletionHandler<void()>&& acquisisionHandler = nullptr)
    {
        auto assertion = adoptRef(*new ProcessAndUIAssertion(pid, reason, type));
        if (mode == Mode::Async)
            assertion->acquireAsync(WTFMove(acquisisionHandler));
        else {
            assertion->acquireSync();
            if (acquisisionHandler)
                acquisisionHandler();
        }
        return assertion;
    }
    ~ProcessAndUIAssertion();

    void uiAssertionWillExpireImminently();

    void setUIAssertionExpirationHandler(Function<void()>&& handler) { m_uiAssertionExpirationHandler = WTFMove(handler); }
#if PLATFORM(IOS_FAMILY)
    static void setProcessStateMonitorEnabled(bool);
#endif

private:
    ProcessAndUIAssertion(ProcessID, const String& reason, ProcessAssertionType);

#if PLATFORM(IOS_FAMILY)
    void processAssertionWasInvalidated() final;
#endif
    void updateRunInBackgroundCount();

    Function<void()> m_uiAssertionExpirationHandler;
    bool m_isHoldingBackgroundTask { false };
};
    
} // namespace WebKit
