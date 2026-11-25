/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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

#include <WebCore/Event.h>
#include <WebCore/SecurityPolicyViolationEventDisposition.h>

namespace WebCore {

struct SecurityPolicyViolationEventInit : EventInit {
    RefPtr<StringImpl> documentURI;
    RefPtr<StringImpl> referrer;
    RefPtr<StringImpl> blockedURI;
    RefPtr<StringImpl> violatedDirective;
    RefPtr<StringImpl> effectiveDirective;
    RefPtr<StringImpl> originalPolicy;
    RefPtr<StringImpl> sourceFile;
    RefPtr<StringImpl> sample;
    SecurityPolicyViolationEventDisposition disposition { SecurityPolicyViolationEventDisposition::Enforce };
    unsigned short statusCode { 0 };
    unsigned lineNumber { 0 };
    unsigned columnNumber { 0 };
};

class SecurityPolicyViolationEvent final : public Event {
    WTF_MAKE_TZONE_ALLOCATED(SecurityPolicyViolationEvent);
public:
    using Disposition = SecurityPolicyViolationEventDisposition;
    using Init = SecurityPolicyViolationEventInit;

    static Ref<SecurityPolicyViolationEvent> create(AtomStringImpl* type, Init&& initializer = { }, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new SecurityPolicyViolationEvent(type, WTF::move(initializer), isTrusted));
    }

    const RefPtr<StringImpl>& documentURI() const LIFETIME_BOUND { return m_documentURI; }
    const RefPtr<StringImpl>& referrer() const LIFETIME_BOUND { return m_referrer; }
    const RefPtr<StringImpl>& blockedURI() const LIFETIME_BOUND { return m_blockedURI; }
    const RefPtr<StringImpl>& violatedDirective() const LIFETIME_BOUND { return m_violatedDirective; }
    const RefPtr<StringImpl>& effectiveDirective() const LIFETIME_BOUND { return m_effectiveDirective; }
    const RefPtr<StringImpl>& originalPolicy() const LIFETIME_BOUND { return m_originalPolicy; }
    const RefPtr<StringImpl>& sourceFile() const LIFETIME_BOUND { return m_sourceFile; }
    const RefPtr<StringImpl>& sample() const LIFETIME_BOUND { return m_sample; }
    Disposition disposition() const { return m_disposition; }
    unsigned short statusCode() const { return m_statusCode; }
    unsigned lineNumber() const { return m_lineNumber; }
    unsigned columnNumber() const { return m_columnNumber; }

private:
    SecurityPolicyViolationEvent()
        : Event(EventInterfaceType::SecurityPolicyViolationEvent)
    {
    }

    SecurityPolicyViolationEvent(AtomStringImpl* type, Init&& initializer, IsTrusted isTrusted)
        : Event(EventInterfaceType::SecurityPolicyViolationEvent, type, WTF::move(initializer), isTrusted)
        , m_documentURI(WTF::move(initializer.documentURI))
        , m_referrer(WTF::move(initializer.referrer))
        , m_blockedURI(WTF::move(initializer.blockedURI))
        , m_violatedDirective(WTF::move(initializer.violatedDirective))
        , m_effectiveDirective(WTF::move(initializer.effectiveDirective))
        , m_originalPolicy(WTF::move(initializer.originalPolicy))
        , m_sourceFile(WTF::move(initializer.sourceFile))
        , m_sample(WTF::move(initializer.sample))
        , m_disposition(initializer.disposition)
        , m_statusCode(initializer.statusCode)
        , m_lineNumber(initializer.lineNumber)
        , m_columnNumber(initializer.columnNumber)
    {
    }

    RefPtr<StringImpl> m_documentURI;
    RefPtr<StringImpl> m_referrer;
    RefPtr<StringImpl> m_blockedURI;
    RefPtr<StringImpl> m_violatedDirective;
    RefPtr<StringImpl> m_effectiveDirective;
    RefPtr<StringImpl> m_originalPolicy;
    RefPtr<StringImpl> m_sourceFile;
    RefPtr<StringImpl> m_sample;
    Disposition m_disposition { Disposition::Enforce };
    unsigned short m_statusCode;
    unsigned m_lineNumber;
    unsigned m_columnNumber;
};

} // namespace WebCore
