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

#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "EventLoop.h"
#include "FrameLoader.h"
#include "JSDOMPromiseDeferred.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "NetworkStorageSession.h"
#include "Page.h"
#include "Quirks.h"
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

ASCIILiteral DocumentStorageAccess::supplementName()
{
    return "DocumentStorageAccess"_s;
}

void DocumentStorageAccess::hasStorageAccess(Document& document, Ref<DeferredPromise>&& promise)
{
    DocumentStorageAccess::from(document)->hasStorageAccess(WTFMove(promise));
}

std::optional<bool> DocumentStorageAccess::hasStorageAccessQuickCheck()
{
    Ref document = m_document.get();

    RefPtr frame = document->frame();
    if (frame && hasFrameSpecificStorageAccess())
        return true;

    Ref securityOrigin = document->securityOrigin();
    if (!frame || securityOrigin->isOpaque())
        return false;

    if (frame->isMainFrame())
        return true;

    if (securityOrigin->equal(&document->topOrigin()))
        return true;

    if (!frame->page())
        return false;

    return std::nullopt;
}

void DocumentStorageAccess::hasStorageAccess(Ref<DeferredPromise>&& promise)
{
    Ref document = m_document.get();

    auto quickCheckResult = hasStorageAccessQuickCheck();
    if (quickCheckResult) {
        promise->resolve<IDLBoolean>(*quickCheckResult);
        return;
    }

    // The existence of a frame and page has been checked in requestStorageAccessQuickCheck().
    RefPtr frame = document->frame();
    if (!frame) {
        ASSERT_NOT_REACHED();
        promise->resolve<IDLBoolean>(false);
        return;
    }
    RefPtr page = frame->page();
    if (!page) {
        ASSERT_NOT_REACHED();
        promise->resolve<IDLBoolean>(false);
        return;
    }

    page->chrome().client().hasStorageAccess(RegistrableDomain::uncheckedCreateFromHost(document->securityOrigin().host()), RegistrableDomain::uncheckedCreateFromHost(document->topOrigin().host()), *frame, [weakThis = WeakPtr { *this }, promise = WTFMove(promise)] (bool hasAccess) {
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

std::optional<StorageAccessQuickResult> DocumentStorageAccess::requestStorageAccessQuickCheck()
{
    Ref document = m_document.get();

    RefPtr frame = document->frame();
    if (frame && hasFrameSpecificStorageAccess())
        return StorageAccessQuickResult::Grant;

    auto& securityOrigin = document->securityOrigin();
    if (!frame || securityOrigin.isOpaque() || !isAllowedToRequestStorageAccess())
        return StorageAccessQuickResult::Reject;

    if (frame->isMainFrame())
        return StorageAccessQuickResult::Grant;

    if (securityOrigin.equal(&document->topOrigin()))
        return StorageAccessQuickResult::Grant;

    // If there is a sandbox, it has to allow the storage access API to be called.
    if (document->sandboxFlags() != SandboxNone && document->isSandboxed(SandboxStorageAccessByUserActivation))
        return StorageAccessQuickResult::Reject;

    if (!UserGestureIndicator::processingUserGesture())
        return StorageAccessQuickResult::Reject;

    return std::nullopt;
}

void DocumentStorageAccess::requestStorageAccess(Ref<DeferredPromise>&& promise)
{
    Ref document = m_document.get();

    auto quickCheckResult = requestStorageAccessQuickCheck();
    if (quickCheckResult) {
        *quickCheckResult == StorageAccessQuickResult::Grant ? promise->resolve() : promise->reject();
        return;
    }

    // The existence of a frame and page has been checked in requestStorageAccessQuickCheck().
    RefPtr frame = document->frame();
    if (!frame) {
        ASSERT_NOT_REACHED();
        promise->reject();
        return;
    }
    RefPtr page = frame->page();
    if (!page) {
        ASSERT_NOT_REACHED();
        promise->reject();
        return;
    }

    if (!page->settings().storageAccessAPIPerPageScopeEnabled())
        m_storageAccessScope = StorageAccessScope::PerFrame;

    page->chrome().client().requestStorageAccess(RegistrableDomain::uncheckedCreateFromHost(document->securityOrigin().host()), RegistrableDomain::uncheckedCreateFromHost(document->topOrigin().host()), *frame, m_storageAccessScope, [this, weakThis = WeakPtr { *this }, promise = WTFMove(promise)] (RequestStorageAccessResult result) mutable {
        if (!weakThis)
            return;

        // Consume the user gesture only if the user explicitly denied access.
        bool shouldPreserveUserGesture = result.wasGranted == StorageAccessWasGranted::Yes || result.promptWasShown == StorageAccessPromptWasShown::No;

        if (shouldPreserveUserGesture) {
            protectedDocument()->eventLoop().queueMicrotask([this, weakThis] {
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
            protectedDocument()->eventLoop().queueMicrotask([this, weakThis] {
                if (weakThis)
                    consumeTemporaryTimeUserGesture();
            });
        }
    });
}

Ref<Document> DocumentStorageAccess::protectedDocument() const
{
    return m_document.get();
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
    requestStorageAccessQuirk(RegistrableDomain::uncheckedCreateFromHost(m_document->securityOrigin().host()), WTFMove(completionHandler));
}

void DocumentStorageAccess::requestStorageAccessForNonDocumentQuirk(Document& hostingDocument, RegistrableDomain&& requestingDomain, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    DocumentStorageAccess::from(hostingDocument)->requestStorageAccessForNonDocumentQuirk(WTFMove(requestingDomain), WTFMove(completionHandler));
}

void DocumentStorageAccess::requestStorageAccessForNonDocumentQuirk(RegistrableDomain&& requestingDomain, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    if (!m_document->frame() || !m_document->frame()->page() || !isAllowedToRequestStorageAccess()) {
        completionHandler(StorageAccessWasGranted::No);
        return;
    }
    requestStorageAccessQuirk(WTFMove(requestingDomain), WTFMove(completionHandler));
}

void DocumentStorageAccess::requestStorageAccessQuirk(RegistrableDomain&& requestingDomain, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    Ref document = m_document.get();
    RELEASE_ASSERT(document->frame() && document->frame()->page());
    RefPtr page = document->frame()->page();

    auto topFrameDomain = RegistrableDomain(page->mainFrameURL());

    RefPtr frame = document->frame();
    page->chrome().client().requestStorageAccess(WTFMove(requestingDomain), WTFMove(topFrameDomain), *frame, m_storageAccessScope, [this, weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)] (RequestStorageAccessResult result) mutable {
        if (!weakThis)
            return;

        // Consume the user gesture only if the user explicitly denied access.
        bool shouldPreserveUserGesture = result.wasGranted == StorageAccessWasGranted::Yes || result.promptWasShown == StorageAccessPromptWasShown::No;

        if (shouldPreserveUserGesture) {
            protectedDocument()->eventLoop().queueMicrotask([this, weakThis] {
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
            protectedDocument()->eventLoop().queueMicrotask([this, weakThis] {
                if (weakThis)
                    consumeTemporaryTimeUserGesture();
            });
        }
    });
}

void DocumentStorageAccess::enableTemporaryTimeUserGesture()
{
    m_temporaryUserGesture = makeUnique<UserGestureIndicator>(IsProcessingUserGesture::Yes, protectedDocument().ptr());
}

void DocumentStorageAccess::consumeTemporaryTimeUserGesture()
{
    m_temporaryUserGesture = nullptr;
}

bool DocumentStorageAccess::hasFrameSpecificStorageAccess() const
{
    RefPtr frame = m_document->frame();
    return frame && frame->loader().client().hasFrameSpecificStorageAccess();
}

} // namespace WebCore
