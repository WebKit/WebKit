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
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/text/WTFString.h>

#if !OS(WINDOWS)
#include <unistd.h>
#endif

#if USE(EXTENSIONKIT)
#include "AssertionCapability.h"
#endif

#if USE(EXTENSIONKIT)
#include "ExtensionProcess.h"
#endif

#if USE(RUNNINGBOARD)
#include <wtf/RetainPtr.h>

OBJC_CLASS RBSAssertion;
OBJC_CLASS WKRBSAssertionDelegate;
#endif // USE(RUNNINGBOARD)

#if USE(EXTENSIONKIT)
OBJC_CLASS BEWebContentProcess;
OBJC_CLASS BENetworkingProcess;
OBJC_CLASS BERenderingProcess;
OBJC_PROTOCOL(BEProcessCapabilityGrant);
#endif

namespace WebKit {

enum class ProcessAssertionType : uint8_t {
    NearSuspended,
    Background,
    UnboundedNetworking,
    Foreground,
    MediaPlayback,
    FinishTaskCanSleep,
    FinishTaskInterruptable,
    BoostedJetsam,
};

ASCIILiteral processAssertionTypeDescription(ProcessAssertionType);

class AuxiliaryProcessProxy;

class ProcessAssertion : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ProcessAssertion> {
    WTF_MAKE_TZONE_ALLOCATED(ProcessAssertion);
public:
    enum class Mode : bool { Sync, Async };
#if USE(EXTENSIONKIT)
    static Ref<ProcessAssertion> create(ExtensionProcess&, const String& reason, ProcessAssertionType, Mode = Mode::Async, const String& environmentIdentifier = emptyString(), CompletionHandler<void()>&& acquisisionHandler = nullptr);
#endif
    static Ref<ProcessAssertion> create(ProcessID, const String& reason, ProcessAssertionType, Mode = Mode::Async, const String& environmentIdentifier = emptyString(), CompletionHandler<void()>&& acquisisionHandler = nullptr);
    static Ref<ProcessAssertion> create(AuxiliaryProcessProxy&, const String& reason, ProcessAssertionType, Mode = Mode::Async, CompletionHandler<void()>&& acquisisionHandler = nullptr);

    static double remainingRunTimeInSeconds(ProcessID);
    virtual ~ProcessAssertion();

    void setPrepareForInvalidationHandler(Function<void()>&& handler) { m_prepareForInvalidationHandler = WTFMove(handler); }
    void setInvalidationHandler(Function<void()>&& handler) { m_invalidationHandler = WTFMove(handler); }

    ProcessAssertionType type() const { return m_assertionType; }
    ProcessID pid() const { return m_pid; }

    bool isValid() const;

protected:
    ProcessAssertion(ProcessID, const String& reason, ProcessAssertionType, const String& environmentIdentifier);
    ProcessAssertion(AuxiliaryProcessProxy&, const String& reason, ProcessAssertionType);

    void init(const String& environmentIdentifier);

    void aquireAssertion(Mode, CompletionHandler<void()>&&);

    void acquireAsync(CompletionHandler<void()>&&);
    void acquireSync();

#if USE(RUNNINGBOARD)
    void processAssertionWillBeInvalidated();
    virtual void processAssertionWasInvalidated();
#endif

private:
    const ProcessAssertionType m_assertionType;
    const ProcessID m_pid;
    const String m_reason;
#if USE(RUNNINGBOARD)
    RetainPtr<RBSAssertion> m_rbsAssertion;
    RetainPtr<WKRBSAssertionDelegate> m_delegate;
    bool m_wasInvalidated { false };
#endif
    Function<void()> m_prepareForInvalidationHandler;
    Function<void()> m_invalidationHandler;
#if USE(EXTENSIONKIT)
    static Lock s_capabilityLock;
    std::optional<AssertionCapability> m_capability;
    ExtensionCapabilityGrant m_grant WTF_GUARDED_BY_LOCK(s_capabilityLock);
    std::optional<ExtensionProcess> m_process;
#endif
};

class ProcessAndUIAssertion final : public ProcessAssertion {
public:
    static Ref<ProcessAndUIAssertion> create(AuxiliaryProcessProxy& process, const String& reason, ProcessAssertionType type, Mode mode = Mode::Async, CompletionHandler<void()>&& acquisisionHandler = nullptr)
    {
        auto assertion = adoptRef(*new ProcessAndUIAssertion(process, reason, type));
        assertion->aquireAssertion(mode, WTFMove(acquisisionHandler));
        return assertion;
    }
    ~ProcessAndUIAssertion();

    void uiAssertionWillExpireImminently();

    void setUIAssertionExpirationHandler(Function<void()>&& handler) { m_uiAssertionExpirationHandler = WTFMove(handler); }
#if PLATFORM(IOS_FAMILY)
    static void setProcessStateMonitorEnabled(bool);
#endif

private:
    ProcessAndUIAssertion(AuxiliaryProcessProxy&, const String& reason, ProcessAssertionType);

#if PLATFORM(IOS_FAMILY)
    void processAssertionWasInvalidated() final;
#endif
    void updateRunInBackgroundCount();

    Function<void()> m_uiAssertionExpirationHandler;
    bool m_isHoldingBackgroundTask { false };
};
    
} // namespace WebKit
