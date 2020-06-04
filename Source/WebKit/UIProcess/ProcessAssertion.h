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

#include <wtf/Function.h>
#include <wtf/ProcessID.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if !OS(WINDOWS)
#include <unistd.h>
#endif

#if PLATFORM(IOS_FAMILY)
#include <wtf/RetainPtr.h>

OBJC_CLASS RBSAssertion;
OBJC_CLASS WKRBSAssertionDelegate;

#if !HAVE(RUNNINGBOARD_VISIBILITY_ASSERTIONS)
OBJC_CLASS BKSProcessAssertion;
#endif
#endif // PLATFORM(IOS_FAMILY)

namespace WebKit {

enum class ProcessAssertionType {
    Suspended,
    Background,
    UnboundedNetworking,
    Foreground,
    MediaPlayback,
};

class ProcessAssertion : public CanMakeWeakPtr<ProcessAssertion> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    class Client {
    public:
        virtual ~Client() { }

        virtual void uiAssertionWillExpireImminently() = 0;
        virtual void assertionWasInvalidated() = 0;
    };

    ProcessAssertion(ProcessID, const String& reason, ProcessAssertionType);
    virtual ~ProcessAssertion();

    void setClient(Client& client) { m_client = &client; }
    Client* client() { return m_client; }

    ProcessAssertionType type() const { return m_assertionType; }
    ProcessID pid() const { return m_pid; }

    bool isValid() const;

#if PLATFORM(IOS_FAMILY)
protected:
    virtual void processAssertionWasInvalidated();
#endif

private:
    const ProcessAssertionType m_assertionType;
    const ProcessID m_pid;
#if PLATFORM(IOS_FAMILY)
    RetainPtr<RBSAssertion> m_rbsAssertion;
    RetainPtr<WKRBSAssertionDelegate> m_delegate;
#if !HAVE(RUNNINGBOARD_VISIBILITY_ASSERTIONS)
    RetainPtr<BKSProcessAssertion> m_bksAssertion; // Legacy.
#endif
#endif
    Client* m_client { nullptr };
};

#if PLATFORM(IOS_FAMILY)

class ProcessAndUIAssertion final : public ProcessAssertion {
public:
    ProcessAndUIAssertion(ProcessID, const String& reason, ProcessAssertionType);
    ~ProcessAndUIAssertion();

    void uiAssertionWillExpireImminently();

private:
    void processAssertionWasInvalidated() final;
    void updateRunInBackgroundCount();

    bool m_isHoldingBackgroundTask { false };
};

#else

using ProcessAndUIAssertion = ProcessAssertion;

#endif // PLATFORM(IOS_FAMILY)
    
} // namespace WebKit
