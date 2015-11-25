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

#ifndef MockContentFilterSettings_h
#define MockContentFilterSettings_h

#include <CoreFoundation/CoreFoundation.h>

typedef CF_ENUM(int, WebMockContentFilterDecision) {
    WebMockContentFilterDecisionAllow,
    WebMockContentFilterDecisionBlock
};

typedef CF_ENUM(int, WebMockContentFilterDecisionPoint) {
    WebMockContentFilterDecisionPointAfterWillSendRequest,
    WebMockContentFilterDecisionPointAfterRedirect,
    WebMockContentFilterDecisionPointAfterResponse,
    WebMockContentFilterDecisionPointAfterAddData,
    WebMockContentFilterDecisionPointAfterFinishedAddingData,
    WebMockContentFilterDecisionPointNever
};

#ifdef __cplusplus

#include <wtf/NeverDestroyed.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class MockContentFilterSettings {
    friend class NeverDestroyed<MockContentFilterSettings>;

public:
    WTF_EXPORT_PRIVATE static MockContentFilterSettings& singleton();
    static void reset();
    static const char* unblockURLHost() { return "mock-unblock"; }

    // Trick the generated bindings into thinking we're RefCounted.
    void ref() { }
    void deref() { }

    bool enabled() const { return m_enabled; }
    WTF_EXPORT_PRIVATE void setEnabled(bool);

    const String& blockedString() const { return m_blockedString; }
    void setBlockedString(const String& blockedString) { m_blockedString = blockedString; }

    WebMockContentFilterDecisionPoint decisionPoint() const { return m_decisionPoint; }
    void setDecisionPoint(WebMockContentFilterDecisionPoint decisionPoint) { m_decisionPoint = decisionPoint; }

    WebMockContentFilterDecision decision() const { return m_decision; }
    void setDecision(WebMockContentFilterDecision decision) { m_decision = decision; }

    WebMockContentFilterDecision unblockRequestDecision() const { return m_unblockRequestDecision; }
    void setUnblockRequestDecision(WebMockContentFilterDecision unblockRequestDecision) { m_unblockRequestDecision = unblockRequestDecision; }

    const String& unblockRequestURL() const;

    const String& modifiedRequestURL() const { return m_modifiedRequestURL; }
    void setModifiedRequestURL(const String& modifiedRequestURL) { m_modifiedRequestURL = modifiedRequestURL; }

private:
    MockContentFilterSettings() = default;
    MockContentFilterSettings(const MockContentFilterSettings&) = delete;
    MockContentFilterSettings& operator=(const MockContentFilterSettings&) = default;

    bool m_enabled { false };
    WebMockContentFilterDecisionPoint m_decisionPoint { WebMockContentFilterDecisionPointAfterResponse };
    WebMockContentFilterDecision m_decision { WebMockContentFilterDecisionAllow };
    WebMockContentFilterDecision m_unblockRequestDecision { WebMockContentFilterDecisionBlock };
    String m_blockedString;
    String m_modifiedRequestURL;
};

} // namespace WebCore

#endif // __cplusplus

#endif // MockContentFilterSettings_h
