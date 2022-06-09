/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include <wtf/text/WTFString.h>

namespace WebCore {

class MockContentFilterSettings {
    friend class NeverDestroyed<MockContentFilterSettings>;
public:
    enum class DecisionPoint {
        AfterWillSendRequest,
        AfterRedirect,
        AfterResponse,
        AfterAddData,
        AfterFinishedAddingData,
        Never
    };

    enum class Decision {
        Allow,
        Block
    };

    WEBCORE_TESTSUPPORT_EXPORT static MockContentFilterSettings& singleton();
    static void reset();
    static const char* unblockURLHost() { return "mock-unblock"; }

    // Trick the generated bindings into thinking we're RefCounted.
    void ref() { }
    void deref() { }

    bool enabled() const { return m_enabled; }
    WEBCORE_TESTSUPPORT_EXPORT void setEnabled(bool);

    const String& blockedString() const { return m_blockedString; }
    void setBlockedString(const String& blockedString) { m_blockedString = blockedString; }

    DecisionPoint decisionPoint() const { return m_decisionPoint; }
    void setDecisionPoint(DecisionPoint decisionPoint) { m_decisionPoint = decisionPoint; }

    Decision decision() const { return m_decision; }
    void setDecision(Decision decision) { m_decision = decision; }

    Decision unblockRequestDecision() const { return m_unblockRequestDecision; }
    void setUnblockRequestDecision(Decision unblockRequestDecision) { m_unblockRequestDecision = unblockRequestDecision; }

    const String& unblockRequestURL() const;

    const String& modifiedRequestURL() const { return m_modifiedRequestURL; }
    void setModifiedRequestURL(const String& modifiedRequestURL) { m_modifiedRequestURL = modifiedRequestURL; }

private:
    MockContentFilterSettings() = default;
    MockContentFilterSettings(const MockContentFilterSettings&) = delete;
    MockContentFilterSettings& operator=(const MockContentFilterSettings&) = default;

    bool m_enabled { false };
    DecisionPoint m_decisionPoint { DecisionPoint::AfterResponse };
    Decision m_decision { Decision::Allow };
    Decision m_unblockRequestDecision { Decision::Block };
    String m_blockedString;
    String m_modifiedRequestURL;
};

} // namespace WebCore
