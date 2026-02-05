/*
 * Copyright (C) 2019-2025 Apple Inc. All rights reserved.
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
#include "DocumentEventLoop.h"
#include "DocumentQuirks.h"
#include "DocumentSecurityOrigin.h"
#include "DocumentView.h"
#include "EventLoop.h"
#include "FrameLoader.h"
#include "JSDOMPromiseDeferred.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "NetworkStorageSession.h"
#include "Page.h"
#include "RegistrableDomain.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "UserGestureIndicator.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DocumentStorageAccess);

DocumentStorageAccess::DocumentStorageAccess(Document& document)
    : m_document(document)
{
}

DocumentStorageAccess::~DocumentStorageAccess() = default;

DocumentStorageAccess* DocumentStorageAccess::from(Document& document)
{
    RefPtr frame = document.frame();
    RefPtr page = frame ? frame->page() : nullptr;

    if (!page || !page->settings().storageAccessAPIEnabled())
        return nullptr;

    if (!Supplement<Document>::from(&document, supplementName())) {
        auto newSupplement = makeUniqueWithoutRefCountedCheck<DocumentStorageAccess>(document);
        provideTo(&document, supplementName(), WTF::move(newSupplement));
    }
    return downcast<DocumentStorageAccess>(Supplement<Document>::from(&document, supplementName()));
}

void DocumentStorageAccess::hasStorageAccess(Document& document, Ref<DeferredPromise>&& promise)
{
    RefPtr storageAccess = DocumentStorageAccess::from(document);
    if (!storageAccess) {
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }
    storageAccess->hasStorageAccess(WTF::move(promise));
}

static bool hasSameOriginAsAllAncestors(const Document& document)
{
    if (document.isTopDocument())
        return true;

    Ref securityOrigin = document.securityOrigin();
    for (RefPtr parentDocument = document.parentDocument(); parentDocument; parentDocument = parentDocument->parentDocument()) {
        if (!securityOrigin->equal(protect(parentDocument->securityOrigin())))
            break;
        if (parentDocument->isTopDocument())
            return true;
    }
    return false;
}

std::optional<bool> DocumentStorageAccess::hasStorageAccessQuickCheck()
{
    Ref document = m_document.get();
    if (!document->isSecureContext())
        return false;

    RefPtr frame = document->frame();
    if (frame && hasFrameSpecificStorageAccess())
        return true;

    if (!frame || protect(document->securityOrigin())->isOpaque())
        return false;

    if (hasSameOriginAsAllAncestors(document))
        return true;

    if (!frame->page())
        return false;

    return std::nullopt;
}

void DocumentStorageAccess::hasStorageAccess(Ref<DeferredPromise>&& promise)
{
    Ref document = m_document.get();
    if (!document->isFullyActive()) {
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }

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

    page->chrome().client().hasStorageAccess(RegistrableDomain::uncheckedCreateFromHost(protect(document->securityOrigin())->host()), RegistrableDomain::uncheckedCreateFromHost(protect(document->topOrigin())->host()), *frame, [weakThis = WeakPtr { *this }, promise = WTF::move(promise)] (bool hasAccess) {
        if (!weakThis)
            return;

        promise->resolve<IDLBoolean>(hasAccess);
    });
}

bool DocumentStorageAccess::hasStorageAccessForDocumentQuirk(Document& document)
{
    RefPtr storageAccess = DocumentStorageAccess::from(document);
    if (!storageAccess)
        return false;

    auto quickCheckResult = storageAccess->hasStorageAccessQuickCheck();
    if (quickCheckResult)
        return *quickCheckResult;
    return false;
}

void DocumentStorageAccess::requestStorageAccess(Document& document, Ref<DeferredPromise>&& promise)
{
    RefPtr storageAccess = DocumentStorageAccess::from(document);
    if (!storageAccess) {
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }
    storageAccess->requestStorageAccess(WTF::move(promise));
}

std::optional<StorageAccessQuickResult> DocumentStorageAccess::requestStorageAccessQuickCheck()
{
    Ref document = m_document.get();
    if (!document->isSecureContext())
        return StorageAccessQuickResult::Reject;

    RefPtr frame = document->frame();
    if (frame && hasFrameSpecificStorageAccess())
        return StorageAccessQuickResult::Grant;

    Ref securityOrigin = document->securityOrigin();
    if (!frame || securityOrigin->isOpaque() || !isAllowedToRequestStorageAccess())
        return StorageAccessQuickResult::Reject;

    if (hasSameOriginAsAllAncestors(document))
        return StorageAccessQuickResult::Grant;

    if (securityOrigin->isSameSiteAs(protect(document->topOrigin())))
        return std::nullopt;

    // If there is a sandbox, it has to allow the storage access API to be called.
    if (!document->sandboxFlags().isEmpty() && document->isSandboxed(SandboxFlag::StorageAccessByUserActivation))
        return StorageAccessQuickResult::Reject;

    return std::nullopt;
}

void DocumentStorageAccess::requestStorageAccess(Ref<DeferredPromise>&& promise)
{
    Ref document = m_document.get();
    if (!document->isFullyActive()) {
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }

    auto quickCheckResult = requestStorageAccessQuickCheck();
    if (quickCheckResult) {
        *quickCheckResult == StorageAccessQuickResult::Grant ? promise->resolve() : promise->reject(ExceptionCode::NotAllowedError);
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

    auto hasOrShouldIgnoreUserGesture = frame->requestSkipUserActivationCheckForStorageAccess(RegistrableDomain { document->url() }) || UserGestureIndicator::processingUserGesture() ? HasOrShouldIgnoreUserGesture::Yes : HasOrShouldIgnoreUserGesture::No;
    page->chrome().client().requestStorageAccess(RegistrableDomain::uncheckedCreateFromHost(protect(document->securityOrigin())->host()), RegistrableDomain::uncheckedCreateFromHost(protect(document->topOrigin())->host()), *frame, m_storageAccessScope, hasOrShouldIgnoreUserGesture, [weakThis = WeakPtr { *this }, promise = WTF::move(promise)] (RequestStorageAccessResult result) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        // Consume the user gesture only if the user explicitly denied access.
        bool shouldPreserveUserGesture;
        switch (result.wasGranted) {
        case StorageAccessWasGranted::Yes:
        case StorageAccessWasGranted::YesWithException:
            shouldPreserveUserGesture = true;
            break;
        case StorageAccessWasGranted::No:
            shouldPreserveUserGesture = result.promptWasShown == StorageAccessPromptWasShown::No;
        }

        Ref document = protectedThis->m_document.get();
        if (shouldPreserveUserGesture) {
            document->checkedEventLoop()->queueMicrotask([weakThis] {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->enableTemporaryTimeUserGesture();
            });
        }

        switch (result.wasGranted) {
        case StorageAccessWasGranted::Yes:
            promise->resolve();
            break;
        case StorageAccessWasGranted::YesWithException: {
            promise->reject(ExceptionCode::NoModificationAllowedError);
            if (RefPtr frame = document->frame()) {
                RegistrableDomain domain { document->securityOrigin().data() };
                frame->storageAccessExceptionReceivedForDomain(domain);
            }
            break;
        }
        case StorageAccessWasGranted::No:
            if (result.promptWasShown == StorageAccessPromptWasShown::Yes)
                protectedThis->setWasExplicitlyDeniedFrameSpecificStorageAccess();
            promise->reject(ExceptionCode::NotAllowedError);
        }

        if (shouldPreserveUserGesture) {
            document->checkedEventLoop()->queueMicrotask([weakThis] {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->consumeTemporaryTimeUserGesture();
            });
        }
    });
}

void DocumentStorageAccess::ref() const
{
    m_document->ref();
}

void DocumentStorageAccess::deref() const
{
    m_document->deref();
}

Ref<Document> DocumentStorageAccess::protectedDocument() const
{
    return m_document.get();
}

void DocumentStorageAccess::requestStorageAccessForDocumentQuirk(Document& document, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    RefPtr storageAccess = DocumentStorageAccess::from(document);
    if (!storageAccess) {
        completionHandler(StorageAccessWasGranted::No);
        return;
    }
    storageAccess->requestStorageAccessForDocumentQuirk(WTF::move(completionHandler));
}

void DocumentStorageAccess::requestStorageAccessForDocumentQuirk(CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    auto quickCheckResult = requestStorageAccessQuickCheck();
    if (quickCheckResult) {
        *quickCheckResult == StorageAccessQuickResult::Grant ? completionHandler(StorageAccessWasGranted::Yes) : completionHandler(StorageAccessWasGranted::No);
        return;
    }
    requestStorageAccessQuirk(RegistrableDomain::uncheckedCreateFromHost(protect(protectedDocument()->securityOrigin())->host()), WTF::move(completionHandler));
}

void DocumentStorageAccess::requestStorageAccessForNonDocumentQuirk(Document& hostingDocument, RegistrableDomain&& requestingDomain, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    RefPtr storageAccess = DocumentStorageAccess::from(hostingDocument);
    if (!storageAccess) {
        completionHandler(StorageAccessWasGranted::No);
        return;
    }
    storageAccess->requestStorageAccessForNonDocumentQuirk(WTF::move(requestingDomain), WTF::move(completionHandler));
}

void DocumentStorageAccess::requestStorageAccessForNonDocumentQuirk(RegistrableDomain&& requestingDomain, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    if (!m_document->frame() || !m_document->frame()->page() || !isAllowedToRequestStorageAccess()) {
        completionHandler(StorageAccessWasGranted::No);
        return;
    }
    requestStorageAccessQuirk(WTF::move(requestingDomain), WTF::move(completionHandler));
}

void DocumentStorageAccess::requestStorageAccessQuirk(RegistrableDomain&& requestingDomain, CompletionHandler<void(StorageAccessWasGranted)>&& completionHandler)
{
    Ref document = m_document.get();
    RELEASE_ASSERT(document->frame() && document->frame()->page());
    RefPtr page = document->frame()->page();

    auto topFrameDomain = RegistrableDomain(page->mainFrameURL());

    RefPtr frame = document->frame();
    page->chrome().client().requestStorageAccess(WTF::move(requestingDomain), WTF::move(topFrameDomain), *frame, m_storageAccessScope, HasOrShouldIgnoreUserGesture::Yes, [weakThis = WeakPtr { *this }, completionHandler = WTF::move(completionHandler)] (RequestStorageAccessResult result) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        // Consume the user gesture only if the user explicitly denied access.
        bool shouldPreserveUserGesture = result.wasGranted == StorageAccessWasGranted::Yes || result.promptWasShown == StorageAccessPromptWasShown::No;

        if (shouldPreserveUserGesture) {
            protectedThis->protectedDocument()->checkedEventLoop()->queueMicrotask([weakThis] {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->enableTemporaryTimeUserGesture();
            });
        }

        switch (result.wasGranted) {
        case StorageAccessWasGranted::Yes:
        case StorageAccessWasGranted::YesWithException:
            completionHandler(StorageAccessWasGranted::Yes);
            break;
        case StorageAccessWasGranted::No:
            if (result.promptWasShown == StorageAccessPromptWasShown::Yes)
                protectedThis->setWasExplicitlyDeniedFrameSpecificStorageAccess();
            completionHandler(StorageAccessWasGranted::No);
        }

        if (shouldPreserveUserGesture) {
            protectedThis->protectedDocument()->checkedEventLoop()->queueMicrotask([weakThis] {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->consumeTemporaryTimeUserGesture();
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
