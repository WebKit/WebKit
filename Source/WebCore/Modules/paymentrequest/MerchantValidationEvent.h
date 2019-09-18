/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(PAYMENT_REQUEST)

#include "Event.h"
#include <wtf/URL.h>

namespace WebCore {

class DOMPromise;
class Document;

class MerchantValidationEvent final : public Event {
public:
    struct Init final : EventInit {
        String methodName;
        String validationURL;
    };

    static Ref<MerchantValidationEvent> create(const AtomString& type, const String& methodName, URL&& validationURL);
    static ExceptionOr<Ref<MerchantValidationEvent>> create(Document&, const AtomString& type, Init&&);

    const String& methodName() const { return m_methodName; }
    const String& validationURL() const { return m_validationURL.string(); }
    ExceptionOr<void> complete(Ref<DOMPromise>&&);

private:
    MerchantValidationEvent(const AtomString& type, const String& methodName, URL&& validationURL);
    MerchantValidationEvent(const AtomString& type, String&& methodName, URL&& validationURL, Init&&);

    // Event
    EventInterface eventInterface() const final;

    bool m_isCompleted { false };
    String m_methodName;
    URL m_validationURL;
};

} // namespace WebCore

#endif // ENABLE(PAYMENT_REQUEST)
