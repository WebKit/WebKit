/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "NavigatorIdentity.h"

#if ENABLE(WEB_AUTHN)

#include "Document.h"
#include "LocalFrame.h"
#include "Navigator.h"

namespace WebCore {

NavigatorIdentity::NavigatorIdentity() = default;

NavigatorIdentity::~NavigatorIdentity() = default;

ASCIILiteral NavigatorIdentity::supplementName()
{
    return "NavigatorIdentity"_s;
}

CredentialsContainer* NavigatorIdentity::identity(WeakPtr<Document, WeakPtrImplWithEventTargetData>&& document)
{
    if (!m_credentialsContainer)
        m_credentialsContainer = IdentityCredentialsContainer::create(WTFMove(document));

    return m_credentialsContainer.get();
}

CredentialsContainer* NavigatorIdentity::identity(Navigator& navigator)
{
    if (!navigator.frame() || !navigator.frame()->document())
        return nullptr;
    return NavigatorIdentity::from(&navigator)->identity(*navigator.frame()->document());
}

NavigatorIdentity* NavigatorIdentity::from(Navigator* navigator)
{
    NavigatorIdentity* supplement = static_cast<NavigatorIdentity*>(Supplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        auto newSupplement = makeUnique<NavigatorIdentity>();
        supplement = newSupplement.get();
        provideTo(navigator, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
