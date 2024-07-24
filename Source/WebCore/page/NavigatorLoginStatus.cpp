/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "NavigatorLoginStatus.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "JSDOMPromiseDeferred.h"
#include "Navigator.h"
#include "Page.h"
#include "RegistrableDomain.h"
#include "SecurityOrigin.h"

namespace WebCore {

NavigatorLoginStatus* NavigatorLoginStatus::from(Navigator& navigator)
{
    auto* supplement = static_cast<NavigatorLoginStatus*>(Supplement<Navigator>::from(&navigator, supplementName()));
    if (!supplement) {
        auto newSupplement = makeUnique<NavigatorLoginStatus>(navigator);
        supplement = newSupplement.get();
        provideTo(&navigator, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

ASCIILiteral NavigatorLoginStatus::supplementName()
{
    return "NavigatorLoginStatus"_s;
}

void NavigatorLoginStatus::setStatus(Navigator& navigator, IsLoggedIn isLoggedIn, Ref<DeferredPromise>&& promise)
{
    NavigatorLoginStatus::from(navigator)->setStatus(isLoggedIn, WTFMove(promise));
}

void NavigatorLoginStatus::isLoggedIn(Navigator& navigator, Ref<DeferredPromise>&& promise)
{
    NavigatorLoginStatus::from(navigator)->isLoggedIn(WTFMove(promise));
}

bool NavigatorLoginStatus::hasSameOrigin() const
{
    RefPtr document = m_navigator.document();
    if (!document)
        return false;
    Ref origin = document->securityOrigin();
    bool isSameSite = true;
    for (RefPtr parentDocument = document->parentDocument(); parentDocument; parentDocument = parentDocument->parentDocument()) {
        if (!origin->isSameOriginAs(parentDocument->securityOrigin())) {
            isSameSite = false;
            break;
        }
    }
    return isSameSite;
}

void NavigatorLoginStatus::setStatus(IsLoggedIn isLoggedIn, Ref<DeferredPromise>&& promise)
{
    RefPtr document = m_navigator.document();
    if (!document || !hasSameOrigin()) {
        promise->reject();
        return;
    }

    RefPtr page = document->page();
    if (!page) {
        promise->reject();
        return;
    }
    page->chrome().client().setLoginStatus(RegistrableDomain::uncheckedCreateFromHost(document->securityOrigin().host()), isLoggedIn, [promise = WTFMove(promise)] {
        promise->resolve();
    });
}

void NavigatorLoginStatus::isLoggedIn(Ref<DeferredPromise>&& promise)
{
    RefPtr document = m_navigator.document();
    if (!document) {
        promise->reject();
        return;
    }

    RefPtr page = document->page();
    if (!page) {
        promise->reject();
        return;
    }
    page->chrome().client().isLoggedIn(RegistrableDomain::uncheckedCreateFromHost(document->securityOrigin().host()), [promise = WTFMove(promise)] (bool isLoggedIn) {
        promise->resolve<IDLBoolean>(isLoggedIn);
    });
}

} // namespace WebCore
