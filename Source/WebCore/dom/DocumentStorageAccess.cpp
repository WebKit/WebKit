/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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

Optional<bool> DocumentStorageAccess::hasStorageAccessQuickCheck()
{
    ASSERT(m_document.settings().storageAccessAPIEnabled());

    auto* frame = m_document.frame();
    if (frame && hasFrameSpecificStorageAccess())
        return true;

    if (!frame || m_document.securityOrigin().isUnique())
        return false;

    if (frame->isMainFrame())
        return true;

    auto& securityOrigin = m_document.securityOrigin();
    auto& topSecurityOrigin = m_document.topDocument().securityOrigin();
    if (securityOrigin.equal(&topSecurityOrigin))
        return true;

    auto* page = frame->page();
    if (!page)
        return false;

    return WTF::nullopt;
}

void DocumentStorageAccess::hasStorageAccess(Ref<DeferredPromise>&& promise)
{
    ASSERT(m_document.settings().storageAccessAPIEnabled());

    auto quickCheckResult = hasStorageAccessQuickCheck();
    if (quickCheckResult) {
        promise->resolve<IDLBoolean>(*quickCheckResult);
        return;
    }

    // The existence of a frame and page has been checked in requestStorageAccessQuickCheck().
    auto* frame = m_document.frame();
    if (!frame) {
        ASSERT_NOT_REACHED();
        promise->resolve<IDLBoolean>(false);
        return;
    }
    auto* page = frame->page();
    if (!page) {
        ASSERT_NOT_REACHED();
        promise->resolve<IDLBoolean>(false);
        return;
    }

    page->chrome().client().hasStorageAccess(RegistrableDomain::uncheckedCreateFromHost(m_document.securityOrigin().host()), RegistrableDomain::uncheckedCreateFromHost(m_document.topDocument().securityOrigin().host()), *frame, [weakThis = makeWeakPtr(*this), promise = WTFMove(promise)] (bool hasAccess) {
        if (!weakThis)
            return;

        promise->resolve<IDLBoolean>(hasAccess);
    });
}

bool DocumentStorageAccess::hasStorageAccessForDocumentQuirk(Document& document)
{
    auto quickCheckResult = DocumentStorageAccess::from(document)->hasStorageAccessQuickCheck();
    if (quickCheckResult)
        return *quickCheckResult;
    return false;
}

void DocumentStorageAccess::requestStorageAccess(Document& document, Ref<DeferredPromise>&& promise)
{
    DocumentStorageAccess::from(document)->requestStorageAccess(WTFMove(promise));
}

Optional<StorageAccessQuickResult> DocumentStorageAccess::requestStorageAccessQuickCheck()
{
    ASSERT(m_document.settings().storageAccessAPIEnabled());

    auto* frame = m_document.frame();
    if (frame && hasFrameSpecificStorageAccess())
        return StorageAccessQuickResult::Grant;

    if (!frame || m_document.securityOrigin().isUnique() || !isAllowedToRequestFrameSpecificStorageAccess())
        return StorageAccessQuickResult::Reject;

    if (frame->isMainFrame())
        return StorageAccessQuickResult::Grant;

    auto& topDocument = m_document.topDocument();
    auto& topSecurityOrigin = topDocument.securityOrigin();
    auto& securityOrigin = m_document.securityOrigin();
    if (securityOrigin.equal(&topSecurityOrigin))
        return StorageAccessQuickResult::Grant;

    // If there is a sandbox, it has to allow the storage access API to be called.
    if (m_document.sandboxFlags() != SandboxNone && m_document.isSandboxed(SandboxStorageAccessByUserActivation))
        return StorageAccessQuickResult::Reject;

    // The iframe has to be a direct child of the top document.
    if (&topDocument != m_document.parentDocument())
        return StorageAccessQuickResult::Reject;

    if (!UserGestureIndicator::processingUserGesture())
        return StorageAccessQuickResult::Reject;

    return WTF::nullopt;
}

void DocumentStorageAccess::requestStorageAccess(Ref<DeferredPromise>&& promise)
{
    ASSERT(m_document.settings().storageAccessAPIEnabled());

    auto quickCheckResult = requestStorageAccessQuickCheck();
    if (quickCheckResult) {
        *quickCheckResult == StorageAccessQuickResult::Grant ? promise->resolve() : promise->reject();
        return;
    }

    // The existence of a frame and page has been checked in requestStorageAccessQuickCheck().
    auto* frame = m_document.frame();
    if (!frame) {
        ASSERT_NOT_REACHED();
        promise->reject();
        return;
    }
    auto* page = frame->page();
    if (!page) {
        ASSERT_NOT_REACHED();
        promise->reject();
        return;
    }

    if (page->settings().storageAccessAPIPerPageScopeEnabled())
        m_storageAccessScope = StorageAccessScope::PerPage;

    page->chrome().client().requestStorageAccess(RegistrableDomain::uncheckedCreateFromHost(m_document.securityOrigin().host()), RegistrableDomain::uncheckedCreateFromHost(m_document.topDocument().securityOrigin().host()), *frame, m_storageAccessScope, [this, weakThis = makeWeakPtr(*this), promise = WTFMove(promise)] (RequestStorageAccessResult result) mutable {
        if (!weakThis)
            return;

        // Consume the user gesture only if the user explicitly denied access.
        bool shouldPreserveUserGesture = result.wasGranted == StorageAccessWasGranted::Yes || result.promptWasShown == StorageAccessPromptWasShown::No;

        if (shouldPreserveUserGesture) {
            m_document.eventLoop().queueMicrotask([this, weakThis] {
                if (weakThis)
                    enableTemporaryTimeUserGesture();
            });
        }

        if (result.wasGranted == StorageAccessWasGranted::Yes)
            promise->resolve();
        else {
            if (result.promptWasShown == StorageAccessPromptWasShown::Yes)
                setWasExplicitlyDeniedFrameSpecificStorageAccess();
            promise->reject();
        }

        if (shouldPreserveUserGesture) {
            m_document.eventLoop().queueMicrotask([this, weakThis] {
                if (weakThis)
                    consumeTemporaryTimeUserGesture();
            });
        }
    });
}

void DocumentStorageAccess::requestStorageAccessForDocumentQuirk(Document& document, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    DocumentStorageAccess::from(document)->requestStorageAccessForDocumentQuirk(WTFMove(completionHandler));
}

void DocumentStorageAccess::requestStorageAccessForDocumentQuirk(CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    auto quickCheckResult = requestStorageAccessQuickCheck();
    if (quickCheckResult) {
        *quickCheckResult == StorageAccessQuickResult::Grant ? completionHandler(StorageAccessWasGranted::Yes) : completionHandler(StorageAccessWasGranted::No);
        return;
    }
    requestStorageAccessQuirk(RegistrableDomain::uncheckedCreateFromHost(m_document.securityOrigin().host()), WTFMove(completionHandler));
}

void DocumentStorageAccess::requestStorageAccessForNonDocumentQuirk(Document& hostingDocument, RegistrableDomain&& requestingDomain, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    DocumentStorageAccess::from(hostingDocument)->requestStorageAccessForNonDocumentQuirk(WTFMove(requestingDomain), WTFMove(completionHandler));
}

void DocumentStorageAccess::requestStorageAccessForNonDocumentQuirk(RegistrableDomain&& requestingDomain, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    if (!m_document.frame() || !m_document.frame()->page()) {
        completionHandler(StorageAccessWasGranted::No);
        return;
    }
    requestStorageAccessQuirk(WTFMove(requestingDomain), WTFMove(completionHandler));
}

void DocumentStorageAccess::requestStorageAccessQuirk(RegistrableDomain&& requestingDomain, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    ASSERT(m_document.settings().storageAccessAPIEnabled());
    RELEASE_ASSERT(m_document.frame() && m_document.frame()->page());

    m_document.frame()->page()->chrome().client().requestStorageAccess(WTFMove(requestingDomain), RegistrableDomain::uncheckedCreateFromHost(m_document.topDocument().securityOrigin().host()), *m_document.frame(), m_storageAccessScope, [this, weakThis = makeWeakPtr(*this), completionHandler = WTFMove(completionHandler)] (RequestStorageAccessResult result) mutable {
        if (!weakThis)
            return;

        // Consume the user gesture only if the user explicitly denied access.
        bool shouldPreserveUserGesture = result.wasGranted == StorageAccessWasGranted::Yes || result.promptWasShown == StorageAccessPromptWasShown::No;

        if (shouldPreserveUserGesture) {
            m_document.eventLoop().queueMicrotask([this, weakThis] {
                if (weakThis)
                    enableTemporaryTimeUserGesture();
            });
        }

        if (result.wasGranted == StorageAccessWasGranted::Yes)
            completionHandler(StorageAccessWasGranted::Yes);
        else {
            if (result.promptWasShown == StorageAccessPromptWasShown::Yes)
                setWasExplicitlyDeniedFrameSpecificStorageAccess();
            completionHandler(StorageAccessWasGranted::No);
        }

        if (shouldPreserveUserGesture) {
            m_document.eventLoop().queueMicrotask([this, weakThis] {
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
