/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "Origin.h"

#include "ExceptionOr.h"
#include "JSDOMBindingSecurity.h"
#include "JSDOMBindingSecurityInlines.h"
#include "JSDOMGlobalObject.h"
#include "JSDOMGlobalObjectInlines.h"
#include "JSDOMURL.h"
#include "JSDOMWindow.h"
#include "JSExtendableMessageEvent.h"
#include "JSHTMLAnchorElement.h"
#include "JSHTMLAreaElement.h"
#include "JSMessageEvent.h"
#include "JSOrigin.h"
#include "JSWorkerGlobalScope.h"
#include "LocalDOMWindow.h"
#include "SecurityOrigin.h"
#include <wtf/URL.h>

namespace WebCore {

Origin::Origin(Ref<SecurityOrigin>&& securityOrigin)
    : m_origin(WTF::move(securityOrigin))
{
}

Origin::~Origin() = default;

Ref<Origin> Origin::create()
{
    return adoptRef(*new Origin(SecurityOrigin::createOpaque()));
}

Ref<Origin> Origin::create(Ref<SecurityOrigin>&& securityOrigin)
{
    return adoptRef(*new Origin(WTF::move(securityOrigin)));
}

// https://html.spec.whatwg.org/multipage/browsers.html#dom-origin-from
ExceptionOr<Ref<Origin>> Origin::from(ScriptExecutionContext& context, JSC::JSValue value)
{
    if (value.isString()) {
        auto url = value.getString(context.globalObject());
        auto parsedURL = URL { url };
        if (!parsedURL.isValid())
            return Exception { ExceptionCode::TypeError, makeString('"', url, "\" cannot be parsed as a URL."_s) };
        return create(SecurityOrigin::create(parsedURL));
    }
    if (auto* jsWindow = toJSDOMGlobalObject<JSDOMWindow>(context.vm(), value)) {
        Ref window = jsWindow->wrapped();
        if (!BindingSecurity::shouldAllowAccessToDOMWindow(context.globalObject(), window))
            return Exception { ExceptionCode::TypeError };
        RefPtr localWindow = dynamicDowncast<LocalDOMWindow>(window);
        if (!localWindow)
            return Exception { ExceptionCode::TypeError };
        if (RefPtr securityOrigin = localWindow->securityOrigin())
            return create(securityOrigin.releaseNonNull());
    } else if (auto* jsWorker = toJSDOMGlobalObject<JSWorkerGlobalScope>(context.vm(), value)) {
        Ref worker = jsWorker->wrapped();
        if (RefPtr securityOrigin = worker->securityOrigin())
            return create(securityOrigin.releaseNonNull());
    } else if (auto* jsMessageEvent = JSC::jsDynamicCast<JSMessageEvent*>(value)) {
        Ref messageEvent = jsMessageEvent->wrapped();
        if (RefPtr securityOrigin = messageEvent->securityOrigin())
            return create(securityOrigin.releaseNonNull());
    } else if (auto* jsExtendableMessageEvent = JSC::jsDynamicCast<JSExtendableMessageEvent*>(value)) {
        Ref extendableMessageEvent = jsExtendableMessageEvent->wrapped();
        if (RefPtr securityOrigin = extendableMessageEvent->securityOrigin())
            return create(securityOrigin.releaseNonNull());
    } else if (auto* jsOrigin = JSC::jsDynamicCast<JSOrigin*>(value)) {
        Ref origin = jsOrigin->wrapped();
        return create(origin->m_origin.copyRef());
    } else if (auto* jsDOMURL = JSC::jsDynamicCast<JSDOMURL*>(value)) {
        Ref domURL = jsDOMURL->wrapped();
        return create(SecurityOrigin::create(domURL->href()));
    } else if (auto* jsAElement = JSC::jsDynamicCast<JSHTMLAnchorElement*>(value)) {
        Ref aElement = jsAElement->wrapped();
        if (aElement->hasAttributeWithoutSynchronization(hrefAttr))
            return create(SecurityOrigin::create(aElement->href()));
    } else if (auto* jsAreaElement = JSC::jsDynamicCast<JSHTMLAreaElement*>(value)) {
        Ref areaElement = jsAreaElement->wrapped();
        if (areaElement->hasAttributeWithoutSynchronization(hrefAttr))
            return create(SecurityOrigin::create(areaElement->href()));
    }
    return Exception { ExceptionCode::TypeError };
}

bool Origin::opaque() const
{
    return m_origin->isOpaque();
}

bool Origin::isSameOrigin(const Origin& other) const
{
    return m_origin->isSameOriginAs(other.m_origin);
}

bool Origin::isSameSite(const Origin& other) const
{
    return m_origin->isSameSiteAs(other.m_origin);
}

} // namespace WebCore
