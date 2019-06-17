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

#include "JSValueInWrappedObject.h"
#include "PaymentRequestUpdateEvent.h"
#include <JavaScriptCore/Strong.h>
#include <wtf/Variant.h>
#include <wtf/text/WTFString.h>

namespace JSC {
class JSObject;
}

namespace WebCore {

class PaymentMethodChangeEvent final : public PaymentRequestUpdateEvent {
public:
    template<typename... Args> static Ref<PaymentMethodChangeEvent> create(Args&&... args)
    {
        return adoptRef(*new PaymentMethodChangeEvent(std::forward<Args>(args)...));
    }

    using MethodDetailsFunction = std::function<JSC::Strong<JSC::JSObject>(JSC::ExecState&)>;
    using MethodDetailsType = Variant<JSValueInWrappedObject, MethodDetailsFunction>;

    const String& methodName() const { return m_methodName; }
    const MethodDetailsType& methodDetails() const { return m_methodDetails; }
    JSValueInWrappedObject& cachedMethodDetails() { return m_cachedMethodDetails; }

    // Event
    EventInterface eventInterface() const override;
    
    struct Init final : PaymentRequestUpdateEventInit {
        String methodName;
        JSC::Strong<JSC::JSObject> methodDetails;
    };

private:
    PaymentMethodChangeEvent(const AtomString& type, Init&&);
    PaymentMethodChangeEvent(const AtomString& type, const String& methodName, MethodDetailsFunction&&);

    String m_methodName;
    MethodDetailsType m_methodDetails;
    JSValueInWrappedObject m_cachedMethodDetails;
};

} // namespace WebCore

#endif // ENABLE(PAYMENT_REQUEST)
