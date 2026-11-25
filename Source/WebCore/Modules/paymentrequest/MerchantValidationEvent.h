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
#include "ExceptionOr.h"
#include <wtf/URL.h>

namespace WebCore {

class DOMPromise;
class Document;

class MerchantValidationEvent final : public Event {
    WTF_MAKE_TZONE_ALLOCATED(MerchantValidationEvent);
public:
    struct Init final : EventInit {
        RefPtr<StringImpl> methodName;
        RefPtr<StringImpl> validationURL;
    };

    NODELETE static Ref<MerchantValidationEvent> create(AtomStringImpl* type, StringImpl* methodName, URL&& validationURL);
    static ExceptionOr<Ref<MerchantValidationEvent>> create(Document&, AtomStringImpl* type, Init&&);

    const RefPtr<StringImpl>& methodName() const LIFETIME_BOUND { return m_methodName; }
    const RefPtr<StringImpl>& validationURL() const LIFETIME_BOUND { return m_validationURL.string(); }
    ExceptionOr<void> complete(Ref<DOMPromise>&&);

private:
    MerchantValidationEvent(AtomStringImpl* type, StringImpl* methodName, URL&& validationURL);
    MerchantValidationEvent(AtomStringImpl* type, RefPtr<StringImpl>&& methodName, URL&& validationURL, Init&&);

    bool m_isCompleted { false };
    RefPtr<StringImpl> m_methodName;
    URL m_validationURL;
};

} // namespace WebCore

#endif // ENABLE(PAYMENT_REQUEST)
