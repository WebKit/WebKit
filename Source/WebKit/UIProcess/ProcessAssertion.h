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

#ifndef ProcessAssertion_h
#define ProcessAssertion_h

#include <wtf/Function.h>
#include <wtf/ProcessID.h>
#include <wtf/text/WTFString.h>

#if !OS(WINDOWS)
#include <unistd.h>
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR)
#include <wtf/RetainPtr.h>
#include <wtf/WeakPtr.h>
OBJC_CLASS BKSProcessAssertion;
#endif

namespace WebKit {
    
enum class AssertionState {
    Suspended,
    Background,
    Download,
    Foreground
};

class ProcessAssertionClient {
public:
    virtual ~ProcessAssertionClient() { };
    virtual void assertionWillExpireImminently() = 0;
};

class ProcessAssertion
#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    : public CanMakeWeakPtr<ProcessAssertion>
#endif
{
public:
    ProcessAssertion(ProcessID, AssertionState, Function<void()>&& invalidationCallback = { });
    ProcessAssertion(ProcessID, const String& reason, AssertionState, Function<void()>&& invalidationCallback = { });
    virtual ~ProcessAssertion();

    virtual void setClient(ProcessAssertionClient& client) { m_client = &client; }
    ProcessAssertionClient* client() { return m_client; }

    AssertionState state() const { return m_assertionState; }
    virtual void setState(AssertionState);

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR)
protected:
    enum class Validity { No, Yes, Unset };
    Validity validity() const { return m_validity; }
#endif

private:
#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    void markAsInvalidated();

    RetainPtr<BKSProcessAssertion> m_assertion;
    Validity m_validity { Validity::Unset };
    Function<void()> m_invalidationCallback;
#endif
    AssertionState m_assertionState;
    ProcessAssertionClient* m_client { nullptr };
};
    
class ProcessAndUIAssertion final : public ProcessAssertion {
public:
    ProcessAndUIAssertion(ProcessID, AssertionState);
    ~ProcessAndUIAssertion();

    void setClient(ProcessAssertionClient&) final;

    void setState(AssertionState) final;

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR)
private:
    void updateRunInBackgroundCount();

    bool m_isHoldingBackgroundAssertion { false };
#endif
};
    
}

#endif // ProcessAssertion_h
