/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Event.h"

namespace WebCore {

class SecurityPolicyViolationEvent final : public Event {
    WTF_MAKE_ISO_ALLOCATED(SecurityPolicyViolationEvent);
public:
    static Ref<SecurityPolicyViolationEvent> create(const AtomString& type, CanBubble canBubble, IsCancelable cancelable, const String& documentURI, const String& referrer, const String& blockedURI, const String& violatedDirective, const String& effectiveDirective, const String& originalPolicy, const String& sourceFile, unsigned short statusCode, int lineNumber, int columnNumber)
    {
        return adoptRef(*new SecurityPolicyViolationEvent(type, canBubble, cancelable, documentURI, referrer, blockedURI, violatedDirective, effectiveDirective, originalPolicy, sourceFile, statusCode, lineNumber, columnNumber));
    }

    static Ref<SecurityPolicyViolationEvent> createForBindings()
    {
        return adoptRef(*new SecurityPolicyViolationEvent());
    }

    struct Init : EventInit {
        String documentURI;
        String referrer;
        String blockedURI;
        String violatedDirective;
        String effectiveDirective;
        String originalPolicy;
        String sourceFile;
        unsigned short statusCode { 0 };
        int lineNumber { 0 };
        int columnNumber { 0 };

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static bool decode(Decoder&, Init&);
    };

    static Ref<SecurityPolicyViolationEvent> create(const AtomString& type, const Init& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new SecurityPolicyViolationEvent(type, initializer, isTrusted));
    }

    const String& documentURI() const { return m_documentURI; }
    const String& referrer() const { return m_referrer; }
    const String& blockedURI() const { return m_blockedURI; }
    const String& violatedDirective() const { return m_violatedDirective; }
    const String& effectiveDirective() const { return m_effectiveDirective; }
    const String& originalPolicy() const { return m_originalPolicy; }
    const String& sourceFile() const { return m_sourceFile; }
    unsigned short statusCode() const { return m_statusCode; }
    int lineNumber() const { return m_lineNumber; }
    int columnNumber() const { return m_columnNumber; }

    EventInterface eventInterface() const final { return SecurityPolicyViolationEventInterfaceType; }

private:
    SecurityPolicyViolationEvent()
    {
    }

    SecurityPolicyViolationEvent(const AtomString& type, CanBubble canBubble, IsCancelable cancelable, const String& documentURI, const String& referrer, const String& blockedURI, const String& violatedDirective, const String& effectiveDirective, const String& originalPolicy, const String& sourceFile, unsigned short statusCode, int lineNumber, int columnNumber)
        : Event(type, canBubble, cancelable)
        , m_documentURI(documentURI)
        , m_referrer(referrer)
        , m_blockedURI(blockedURI)
        , m_violatedDirective(violatedDirective)
        , m_effectiveDirective(effectiveDirective)
        , m_originalPolicy(originalPolicy)
        , m_sourceFile(sourceFile)
        , m_statusCode(statusCode)
        , m_lineNumber(lineNumber)
        , m_columnNumber(columnNumber)
    {
    }

    SecurityPolicyViolationEvent(const AtomString& type, const Init& initializer, IsTrusted isTrusted)
        : Event(type, initializer, isTrusted)
        , m_documentURI(initializer.documentURI)
        , m_referrer(initializer.referrer)
        , m_blockedURI(initializer.blockedURI)
        , m_violatedDirective(initializer.violatedDirective)
        , m_effectiveDirective(initializer.effectiveDirective)
        , m_originalPolicy(initializer.originalPolicy)
        , m_sourceFile(initializer.sourceFile)
        , m_statusCode(initializer.statusCode)
        , m_lineNumber(initializer.lineNumber)
        , m_columnNumber(initializer.columnNumber)
    {
    }

    String m_documentURI;
    String m_referrer;
    String m_blockedURI;
    String m_violatedDirective;
    String m_effectiveDirective;
    String m_originalPolicy;
    String m_sourceFile;
    unsigned short m_statusCode;
    int m_lineNumber;
    int m_columnNumber;
};

template<class Encoder>
void SecurityPolicyViolationEvent::Init::encode(Encoder& encoder) const
{
    encoder << static_cast<const EventInit&>(*this);
    encoder << documentURI;
    encoder << referrer;
    encoder << blockedURI;
    encoder << violatedDirective;
    encoder << effectiveDirective;
    encoder << originalPolicy;
    encoder << sourceFile;
    encoder << statusCode;
    encoder << lineNumber;
    encoder << columnNumber;
}

template<class Decoder>
bool SecurityPolicyViolationEvent::Init::decode(Decoder& decoder, SecurityPolicyViolationEvent::Init& eventInit)
{
    if (!decoder.decode(static_cast<EventInit&>(eventInit)))
        return false;
    if (!decoder.decode(eventInit.documentURI))
        return false;
    if (!decoder.decode(eventInit.referrer))
        return false;
    if (!decoder.decode(eventInit.blockedURI))
        return false;
    if (!decoder.decode(eventInit.violatedDirective))
        return false;
    if (!decoder.decode(eventInit.effectiveDirective))
        return false;
    if (!decoder.decode(eventInit.originalPolicy))
        return false;
    if (!decoder.decode(eventInit.sourceFile))
        return false;
    if (!decoder.decode(eventInit.statusCode))
        return false;
    if (!decoder.decode(eventInit.lineNumber))
        return false;
    if (!decoder.decode(eventInit.columnNumber))
        return false;
    return true;
}

} // namespace WebCore
