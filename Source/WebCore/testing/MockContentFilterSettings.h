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
    WEBCORE_TESTSUPPORT_EXPORT static void reset();
    static ASCIILiteral unblockURLHost() { return "mock-unblock"_s; }

    // Trick the generated bindings into thinking we're RefCounted.
    void ref() { }
    void deref() { }

    bool enabled() const { return m_enabled; }
    WEBCORE_TESTSUPPORT_EXPORT void setEnabled(bool);

    const String& blockedString() const { return m_blockedString; }
    WEBCORE_TESTSUPPORT_EXPORT void setBlockedString(const String&);

    DecisionPoint decisionPoint() const { return m_decisionPoint; }
    WEBCORE_TESTSUPPORT_EXPORT void setDecisionPoint(DecisionPoint);

    Decision decision() const { return m_decision; }
    WEBCORE_TESTSUPPORT_EXPORT void setDecision(Decision);

    Decision unblockRequestDecision() const { return m_unblockRequestDecision; }
    WEBCORE_TESTSUPPORT_EXPORT void setUnblockRequestDecision(Decision);

    WEBCORE_TESTSUPPORT_EXPORT const String& unblockRequestURL() const;

    const String& modifiedRequestURL() const { return m_modifiedRequestURL; }
    WEBCORE_TESTSUPPORT_EXPORT void setModifiedRequestURL(const String&);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<MockContentFilterSettings> decode(Decoder&);

    MockContentFilterSettings() = default;
    MockContentFilterSettings(const MockContentFilterSettings&) = default;
    MockContentFilterSettings& operator=(const MockContentFilterSettings&) = default;
private:

    bool m_enabled { false };
    DecisionPoint m_decisionPoint { DecisionPoint::AfterResponse };
    Decision m_decision { Decision::Allow };
    Decision m_unblockRequestDecision { Decision::Block };
    String m_blockedString;
    String m_modifiedRequestURL;
};

template<class Encoder>
void MockContentFilterSettings::encode(Encoder& encoder) const
{
    encoder << m_enabled;
    encoder << decisionPoint();
    encoder << decision();
    encoder << unblockRequestDecision();
    encoder << blockedString();
    encoder << modifiedRequestURL();
}

template<class Decoder>
std::optional<MockContentFilterSettings> MockContentFilterSettings::decode(Decoder& decoder )
{
    std::optional<bool> enabled;
    decoder >> enabled;
    if (!enabled)
        return std::nullopt;
    std::optional<DecisionPoint> decisionPoint;
    decoder >> decisionPoint;
    if (!decisionPoint)
        return std::nullopt;
    std::optional<WebCore::MockContentFilterSettings::Decision> decision;
    decoder >> decision;
    if (!decision)
        return std::nullopt;
    std::optional<WebCore::MockContentFilterSettings::Decision> unblockRequestDecision;
    decoder >> unblockRequestDecision;
    if (!unblockRequestDecision)
        return std::nullopt;
    std::optional<String> blockedString;
    decoder >> blockedString;
    if (!blockedString)
        return std::nullopt;
    std::optional<String> modifiedRequestURL;
    decoder >> modifiedRequestURL;
    if (!modifiedRequestURL)
        return std::nullopt;

    MockContentFilterSettings settings;
    settings.setEnabled(*enabled);
    settings.setDecisionPoint(*decisionPoint);
    settings.setDecision(*decision);
    settings.setUnblockRequestDecision(*unblockRequestDecision);
    settings.setBlockedString(WTFMove(*blockedString));
    settings.setModifiedRequestURL(WTFMove(*modifiedRequestURL));

    return settings;
}
} // namespace WebCore
