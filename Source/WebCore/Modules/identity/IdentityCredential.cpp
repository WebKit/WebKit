/*
 * Copyright (C) 2026 Shopify Inc. All rights reserved.
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
#include "IdentityCredential.h"

#if ENABLE(FEDCM)

#include "CredentialRequestOptions.h"
#include "DocumentPage.h"
#include "FedCMProxy.h"
#include "FrameDestructionObserverInlines.h"
#include "IdentityCredentialCreateOptions.h"
#include "IdentityCredentialRequestOptions.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "Page.h"
#include <wtf/UUID.h>

namespace WebCore {

Ref<IdentityCredential> IdentityCredential::create(const String& token, bool isAutoSelected)
{
    return adoptRef(*new IdentityCredential(token, isAutoSelected));
}

IdentityCredential::~IdentityCredential() = default;

IdentityCredential::IdentityCredential(const String& token, bool isAutoSelected)
    : BasicCredential(createVersion4UUIDString(), Type::Identity, Discovery::Remote)
    , m_token(token)
    , m_isAutoSelected(isAutoSelected)
{
}

void IdentityCredential::discoverFromExternalSource(const Document& document, CredentialPromise&& promise, CredentialRequestOptions&& options)
{
    ASSERT(options.identity);

    RefPtr frame = document.frame();
    RefPtr window = document.window();
    if (!frame || !window) {
        promise.reject(ExceptionCode::InvalidStateError, "Preconditions for calling .get() are not met."_s);
        return;
    }

    RefPtr page = frame->page();
    if (!page) {
        promise.reject(ExceptionCode::InvalidStateError, "Preconditions for calling .get() are not met."_s);
        return;
    }

    auto& identityOptions = *options.identity;
    if (identityOptions.providers.isEmpty()) {
        promise.reject(Exception { ExceptionCode::TypeError, "At least one provider must be specified."_s });
        return;
    }

    for (auto& provider : identityOptions.providers) {
        if (provider.configURL.isEmpty()) {
            promise.reject(Exception { ExceptionCode::TypeError, "configURL is required for each provider."_s });
            return;
        }
        if (provider.clientId.isEmpty()) {
            promise.reject(Exception { ExceptionCode::TypeError, "clientId is required for each provider."_s });
            return;
        }
    }

    if (!window->consumeTransientActivation()) {
        promise.reject(Exception { ExceptionCode::NotAllowedError, "Calling get() needs to be triggered by an activation triggering user event."_s });
        return;
    }

    page->fedCMProxy().requestCredential(document, WTF::move(promise), WTF::move(identityOptions));
}

void IdentityCredential::createIdentityCredential(const Document& document, CredentialPromise&& promise, IdentityCredentialCreateOptions&& options)
{
    RefPtr frame = document.frame();
    if (!frame) {
        promise.reject(ExceptionCode::InvalidStateError, "Preconditions for calling .create() are not met."_s);
        return;
    }

    RefPtr page = frame->page();
    if (!page) {
        promise.reject(ExceptionCode::InvalidStateError, "Preconditions for calling .create() are not met."_s);
        return;
    }

    if (options.format.isEmpty()) {
        promise.reject(Exception { ExceptionCode::TypeError, "format is required."_s });
        return;
    }

    if (options.configURL.isEmpty()) {
        promise.reject(Exception { ExceptionCode::TypeError, "configURL is required."_s });
        return;
    }

    page->fedCMProxy().createCredential(document, WTF::move(promise), WTF::move(options));
}

} // namespace WebCore

#endif // ENABLE(FEDCM)
