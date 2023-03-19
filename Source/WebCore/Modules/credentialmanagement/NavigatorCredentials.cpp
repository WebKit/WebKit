/*
 * Copyright (C) 2017 Google Inc. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "NavigatorCredentials.h"

#if ENABLE(WEB_AUTHN)

#include "Document.h"
#include "LocalFrame.h"
#include "Navigator.h"

namespace WebCore {

NavigatorCredentials::NavigatorCredentials() = default;

NavigatorCredentials::~NavigatorCredentials() = default;

const char* NavigatorCredentials::supplementName()
{
    return "NavigatorCredentials";
}

CredentialsContainer* NavigatorCredentials::credentials(WeakPtr<Document, WeakPtrImplWithEventTargetData>&& document)
{
    if (!m_credentialsContainer)
        m_credentialsContainer = CredentialsContainer::create(WTFMove(document));

    return m_credentialsContainer.get();
}

CredentialsContainer* NavigatorCredentials::credentials(Navigator& navigator)
{
    if (!navigator.frame() || !navigator.frame()->document())
        return nullptr;
    return NavigatorCredentials::from(&navigator)->credentials(*navigator.frame()->document());
}

NavigatorCredentials* NavigatorCredentials::from(Navigator* navigator)
{
    NavigatorCredentials* supplement = static_cast<NavigatorCredentials*>(Supplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        auto newSupplement = makeUnique<NavigatorCredentials>();
        supplement = newSupplement.get();
        provideTo(navigator, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
