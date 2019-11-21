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
#include "DocumentStorageAccess.h"

#if ENABLE(RESOURCE_LOAD_STATISTICS)

#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "EventLoop.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "JSDOMPromiseDeferred.h"
#include "Page.h"
#include "RegistrableDomain.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "UserGestureIndicator.h"

namespace WebCore {

DocumentStorageAccess::DocumentStorageAccess(Document& document)
    : m_document(document)
{
}

DocumentStorageAccess::~DocumentStorageAccess() = default;

DocumentStorageAccess* DocumentStorageAccess::from(Document& document)
{
    auto* supplement = static_cast<DocumentStorageAccess*>(Supplement<Document>::from(&document, supplementName()));
    if (!supplement) {
        auto newSupplement = makeUnique<DocumentStorageAccess>(document);
        supplement = newSupplement.get();
        provideTo(&document, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

const char* DocumentStorageAccess::supplementName()
{
    return "DocumentStorageAccess";
}

void DocumentStorageAccess::hasStorageAccess(Document& document, Ref<DeferredPromise>&& promise)
{
    DocumentStorageAccess::from(document)->hasStorageAccess(WTFMove(promise));
}

void DocumentStorageAccess::requestStorageAccess(Document& document, Ref<DeferredPromise>&& promise)
{
    DocumentStorageAccess::from(document)->requestStorageAccess(WTFMove(promise));
}

void DocumentStorageAccess::hasStorageAccess(Ref<DeferredPromise>&& promise)
{
    ASSERT(m_document.settings().storageAccessAPIEnabled());

    auto* frame = m_document.frame();
    if (frame && hasFrameSpecificStorageAccess()) {
        promise->resolve<IDLBoolean>(true);
        return;
    }
    
    if (!frame || m_document.securityOrigin().isUnique()) {
        promise->resolve<IDLBoolean>(false);
        return;
    }
    
    if (frame->isMainFrame()) {
        promise->resolve<IDLBoolean>(true);
        return;
    }
    
    auto& securityOrigin = m_document.securityOrigin();
    auto& topSecurityOrigin = m_document.topDocument().securityOrigin();
    if (securityOrigin.equal(&topSecurityOrigin)) {
        promise->resolve<IDLBoolean>(true);
        return;
    }
    
    auto* page = frame->page();
    if (!page) {
        promise->reject();
        return;
    }
    
    auto subFrameDomain = RegistrableDomain::uncheckedCreateFromHost(securityOrigin.host());
    auto topFrameDomain = RegistrableDomain::uncheckedCreateFromHost(topSecurityOrigin.host());
    page->chrome().client().hasStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), *frame, [weakThis = makeWeakPtr(*this), promise = WTFMove(promise)] (bool hasAccess) {
        if (!weakThis)
            return;

        promise->resolve<IDLBoolean>(hasAccess);
    });
}

void DocumentStorageAccess::requestStorageAccess(Ref<DeferredPromise>&& promise)
{
    ASSERT(m_document.settings().storageAccessAPIEnabled());
    
    auto* frame = m_document.frame();
    if (frame && hasFrameSpecificStorageAccess()) {
        promise->resolve();
        return;
    }
    
    if (!frame || m_document.securityOrigin().isUnique() || !isAllowedToRequestFrameSpecificStorageAccess()) {
        promise->reject();
        return;
    }
    
    if (frame->isMainFrame()) {
        promise->resolve();
        return;
    }
    
    auto& topDocument = m_document.topDocument();
    auto& topSecurityOrigin = topDocument.securityOrigin();
    auto& securityOrigin = m_document.securityOrigin();
    if (securityOrigin.equal(&topSecurityOrigin)) {
        promise->resolve();
        return;
    }
    
    // If there is a sandbox, it has to allow the storage access API to be called.
    if (m_document.sandboxFlags() != SandboxNone && m_document.isSandboxed(SandboxStorageAccessByUserActivation)) {
        promise->reject();
        return;
    }
    
    // The iframe has to be a direct child of the top document.
    if (&topDocument != m_document.parentDocument()) {
        promise->reject();
        return;
    }
    
    if (!UserGestureIndicator::processingUserGesture()) {
        promise->reject();
        return;
    }

    auto* page = frame->page();
    if (!page) {
        promise->reject();
        return;
    }
    
    auto subFrameDomain = RegistrableDomain::uncheckedCreateFromHost(securityOrigin.host());
    auto topFrameDomain = RegistrableDomain::uncheckedCreateFromHost(topSecurityOrigin.host());
    
    page->chrome().client().requestStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), *frame, [this, weakThis = makeWeakPtr(*this), promise = WTFMove(promise)] (StorageAccessWasGranted wasGranted, StorageAccessPromptWasShown promptWasShown) mutable {
        if (!weakThis)
            return;

        // Consume the user gesture only if the user explicitly denied access.
        bool shouldPreserveUserGesture = wasGranted == StorageAccessWasGranted::Yes || promptWasShown == StorageAccessPromptWasShown::No;

        if (shouldPreserveUserGesture) {
            m_document.eventLoop().queueMicrotask([this, weakThis = makeWeakPtr(*this)] {
                if (weakThis)
                    enableTemporaryTimeUserGesture();
            });
        }

        if (wasGranted == StorageAccessWasGranted::Yes)
            promise->resolve();
        else {
            if (promptWasShown == StorageAccessPromptWasShown::Yes)
                setWasExplicitlyDeniedFrameSpecificStorageAccess();
            promise->reject();
        }

        if (shouldPreserveUserGesture) {
            m_document.eventLoop().queueMicrotask([this, weakThis = makeWeakPtr(*this)] {
                if (weakThis)
                    consumeTemporaryTimeUserGesture();
            });
        }
    });
}

void DocumentStorageAccess::enableTemporaryTimeUserGesture()
{
    m_temporaryUserGesture = makeUnique<UserGestureIndicator>(ProcessingUserGesture, &m_document);
}

void DocumentStorageAccess::consumeTemporaryTimeUserGesture()
{
    m_temporaryUserGesture = nullptr;
}

bool DocumentStorageAccess::hasFrameSpecificStorageAccess() const
{
    auto* frame = m_document.frame();
    return frame && frame->loader().client().hasFrameSpecificStorageAccess();
}

} // namespace WebCore

#endif // ENABLE(RESOURCE_LOAD_STATISTICS)
