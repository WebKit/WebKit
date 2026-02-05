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
#include "DocumentFullscreen.h"

#if ENABLE(FULLSCREEN_API)

#include "Chrome.h"
#include "ChromeClient.h"
#include "ContainerNodeInlines.h"
#include "Document.h"
#include "DocumentQuirks.h"
#include "DocumentView.h"
#include "Element.h"
#include "ElementInlines.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameInlines.h"
#include "HTMLDialogElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLVideoElement.h"
#include "JSDOMPromiseDeferred.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "Logging.h"
#include "NodeInlines.h"
#include "NodeList.h"
#include "Page.h"
#include "PseudoClassChangeInvalidation.h"
#include "QualifiedName.h"
#include "RenderBlock.h"
#include "RenderVideoInlines.h"
#include "SVGElementTypeHelpers.h"
#include "SVGSVGElement.h"
#include "Settings.h"
#include "UserGestureIndicator.h"
#include <ranges>
#include <wtf/LoggerHelper.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(MATHML)
#include "MathMLMathElement.h"
#endif

namespace WebCore {

// MARK: - Constructor.

WTF_MAKE_TZONE_ALLOCATED_IMPL(DocumentFullscreen);

DocumentFullscreen::DocumentFullscreen(Document& document)
    : m_document(document)
#if !RELEASE_LOG_DISABLED
    , m_logIdentifier(LoggerHelper::uniqueLogIdentifier())
#endif
{
}

// MARK: - Fullscreen element.
// https://fullscreen.spec.whatwg.org/#fullscreen-element

Element* DocumentFullscreen::fullscreenElement() const
{
    for (Ref element : document().topLayerElements() | std::views::reverse) {
        if (element->hasFullscreenFlag())
            return element.unsafePtr();
    }

    return nullptr;
}

// MARK: - fullscreenEnabled attribute.
// https://fullscreen.spec.whatwg.org/#dom-document-fullscreenenabled

bool DocumentFullscreen::fullscreenEnabled(Document& document)
{
    if (!document.isFullyActive())
        return false;
    return protect(document.fullscreen())->enabledByPermissionsPolicy();
}

bool DocumentFullscreen::enabledByPermissionsPolicy() const
{
    // The fullscreenEnabled attribute must return true if the context object and all ancestor
    // browsing context's documents have their fullscreen enabled flag set, or false otherwise.

    // Top-level browsing contexts are implied to have their allowFullscreen attribute set.
    return PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Fullscreen, protect(document()));
}

// MARK: - Fullscreen element ready check.
// https://fullscreen.spec.whatwg.org/#fullscreen-element-ready-check

static ASCIILiteral fullscreenElementReadyCheck(DocumentFullscreen::FullscreenCheckType checkType, Element& element)
{
    if (!element.isConnected())
        return "Cannot request fullscreen on a disconnected element."_s;

    if (element.isPopoverShowing())
        return "Cannot request fullscreen on an open popover."_s;

    if (checkType == DocumentFullscreen::EnforceIFrameAllowFullscreenRequirement && !PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Fullscreen, protect(element.document())))
        return "Fullscreen API is disabled by permissions policy."_s;

    return { };
};

// MARK: - requestFullscreen() steps.
// https://fullscreen.spec.whatwg.org/#dom-element-requestfullscreen

void DocumentFullscreen::requestFullscreen(Ref<Element>&& element, FullscreenCheckType checkType, CompletionHandler<void(ExceptionOr<void>)>&& completionHandler, HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    auto identifier = LOGIDENTIFIER;

    if (protect(document())->quirks().shouldEnterNativeFullscreenWhenCallingElementRequestFullscreenQuirk()) {
        // Translate the request to enter fullscreen into requesting native fullscreen
        // for the largest inner video element.
        auto maybeVideoList = element->querySelectorAll("video"_s);
        if (maybeVideoList.hasException()) {
            completionHandler({ });
            return;
        }

#if ENABLE(VIDEO)
        Ref videoList = maybeVideoList.releaseReturnValue();

        RefPtr<HTMLVideoElement> largestVideo = nullptr;
        unsigned largestArea = 0;
        for (unsigned index = 0; index < videoList->length(); ++index) {
            RefPtr video = downcast<HTMLVideoElement>(videoList->item(index));
            if (!video)
                continue;

            CheckedPtr renderer = video->renderer();
            if (!renderer)
                continue;

            auto area = renderer->videoBox().area();
            if (area.hasOverflowed())
                continue;

            if (area > largestArea)
                largestVideo = video;
        }
        if (largestVideo)
            largestVideo->webkitRequestFullscreen();
#endif

        completionHandler({ });
        return;
    }

    enum class EmitErrorEvent : bool { No, Yes };
    auto handleError = [element, identifier, weakThis = WeakPtr { *this }](ASCIILiteral message, EmitErrorEvent emitErrorEvent, CompletionHandler<void(ExceptionOr<void>)>&& completionHandler) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return completionHandler(Exception { ExceptionCode::TypeError, message });
        ERROR_LOG_WITH_THIS(protectedThis, identifier, message);
        if (emitErrorEvent == EmitErrorEvent::Yes) {
            protectedThis->m_pendingEvents.append(std::pair { EventType::Error, WTF::move(element) });
            protect(protectedThis->document())->scheduleRenderingUpdate(RenderingUpdateStep::Fullscreen);
        }
        completionHandler(Exception { ExceptionCode::TypeError, message });
    };

    // If pendingDoc is not fully active, then reject promise with a TypeError exception and return promise.
    if (!protect(document())->isFullyActive())
        return handleError("Cannot request fullscreen on a document that is not fully active."_s, EmitErrorEvent::No, WTF::move(completionHandler));

    auto isElementTypeAllowedForFullscreen = [] (const auto& element) {
        if (is<HTMLElement>(element) || is<SVGSVGElement>(element))
            return true;
#if ENABLE(MATHML)
        if (is<MathMLMathElement>(element))
            return true;
#endif
        return false;
    };

    // If any of the following conditions are true, terminate these steps and queue a task to fire
    // an event named fullscreenerror with its bubbles attribute set to true on the context object's
    // node document:
    if (!isElementTypeAllowedForFullscreen(element))
        return handleError("Cannot request fullscreen on a non-HTML element."_s, EmitErrorEvent::Yes, WTF::move(completionHandler));

    if (is<HTMLDialogElement>(element))
        return handleError("Cannot request fullscreen on a <dialog> element."_s, EmitErrorEvent::Yes, WTF::move(completionHandler));

    if (auto error = fullscreenElementReadyCheck(checkType, element))
        return handleError(error, EmitErrorEvent::Yes, WTF::move(completionHandler));

    if (RefPtr window = document().window(); !window || !window->consumeTransientActivation())
        return handleError("Cannot request fullscreen without transient activation."_s, EmitErrorEvent::Yes, WTF::move(completionHandler));

    if (UserGestureIndicator::processingUserGesture() && UserGestureIndicator::currentUserGesture()->gestureType() == UserGestureType::EscapeKey)
        return handleError("Cannot request fullscreen with Escape key as current gesture."_s, EmitErrorEvent::Yes, WTF::move(completionHandler));

    // There is a previously-established user preference, security risk, or platform limitation.
    RefPtr page = this->page();
    if (!page || !page->isDocumentFullscreenEnabled())
        return handleError("Fullscreen API is disabled."_s, EmitErrorEvent::Yes, WTF::move(completionHandler));

    bool hasKeyboardAccess = true;
    if (!page->chrome().client().supportsFullScreenForElement(element, hasKeyboardAccess)) {
        // The new full screen API does not accept a "flags" parameter, so fall back to disallowing
        // keyboard input if the chrome client refuses to allow keyboard input.
        hasKeyboardAccess = false;

        if (!page->chrome().client().supportsFullScreenForElement(element, hasKeyboardAccess))
            return handleError("Cannot request fullscreen with unsupported element."_s, EmitErrorEvent::Yes, WTF::move(completionHandler));
    }

    INFO_LOG(identifier);

    m_pendingFullscreenElement = element.ptr();

    protect(document())->eventLoop().queueTask(TaskSource::MediaElement, [weakThis = WeakPtr { *this }, element = WTF::move(element), scope = CompletionHandlerScope(WTF::move(completionHandler)), hasKeyboardAccess, checkType, handleError, identifier, mode]() mutable {
        auto completionHandler = scope.release();
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return completionHandler(Exception { ExceptionCode::TypeError });

        // Don't allow fullscreen if it has been cancelled or a different fullscreen elementAdd commentMore actions
        // has requested fullscreen.
        if (protectedThis->m_pendingFullscreenElement != element.ptr())
            return handleError("Fullscreen request aborted by a fullscreen request for another element."_s, EmitErrorEvent::Yes, WTF::move(completionHandler));

        // Don't allow fullscreen if we're inside an exitFullscreen operation.
        if (protectedThis->m_pendingExitFullscreen)
            return handleError("Fullscreen request aborted by a request to exit fullscreen."_s, EmitErrorEvent::Yes, WTF::move(completionHandler));

        // Don't allow fullscreen if document is hidden.
        Ref document = protectedThis->document();
        if ((document->hidden() && mode != HTMLMediaElementEnums::VideoFullscreenModeInWindow) || protectedThis->m_pendingFullscreenElement != element.ptr())
            return handleError("Cannot request fullscreen in a hidden document."_s, EmitErrorEvent::Yes, WTF::move(completionHandler));

        // Fullscreen element ready check.
        if (auto error = fullscreenElementReadyCheck(checkType, element))
            return handleError(error, EmitErrorEvent::Yes, WTF::move(completionHandler));

        // Don't allow if element changed document.
        if (&element->document() != document.ptr())
            return handleError("Cannot request fullscreen because the associated document has changed."_s, EmitErrorEvent::Yes, WTF::move(completionHandler));

        // A descendant browsing context's document has a non-empty fullscreen element stack.
        bool descendantHasNonEmptyStack = false;
        for (RefPtr descendant = protectedThis->frame() ? protectedThis->frame()->tree().traverseNext() : nullptr; descendant; descendant = descendant->tree().traverseNext()) {
            auto* localFrame = dynamicDowncast<LocalFrame>(descendant.get());
            if (!localFrame)
                continue;
            if (protect(protect(localFrame->document())->fullscreen())->fullscreenElement()) {
                descendantHasNonEmptyStack = true;
                break;
            }
        }
        if (descendantHasNonEmptyStack)
            return handleError("Cannot request fullscreen because a descendant document already has a fullscreen element."_s, EmitErrorEvent::Yes, WTF::move(completionHandler));

        // 5. Return, and run the remaining steps asynchronously.
        // 6. Optionally, perform some animation.
        protectedThis->m_areKeysEnabledInFullscreen = hasKeyboardAccess;

        RefPtr page = protectedThis->page();
        if (!page)
            return handleError("Invalid state when requesting fullscreen."_s, EmitErrorEvent::Yes, WTF::move(completionHandler));

        INFO_LOG_WITH_THIS(protectedThis, identifier, "task - success");

        page->chrome().client().enterFullScreenForElement(element, mode, WTF::move(completionHandler), [weakThis = WTF::move(weakThis)](bool success) {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis || !success)
                return true;
            return protectedThis->didEnterFullscreen();
        });

        // 7. Optionally, display a message indicating how the user can exit displaying the context object fullscreen.
    });
}

ExceptionOr<void> DocumentFullscreen::willEnterFullscreen(Element& element, HTMLMediaElementEnums::VideoFullscreenMode mode)
{
#if !ENABLE(VIDEO)
    UNUSED_PARAM(mode);
#endif

    if (backForwardCacheState() != Document::NotInBackForwardCache) {
        ERROR_LOG(LOGIDENTIFIER, "Document in the BackForwardCache; bailing");
        return Exception { ExceptionCode::TypeError };
    }

    RefPtr page = this->page();
    if (!page) {
        ERROR_LOG(LOGIDENTIFIER, "Document no longer in page; bailing");
        return Exception { ExceptionCode::TypeError };
    }

    // FIXME: Should we enforce the iframe requirement here? (webkit.org/b/288951)
    if (auto error = fullscreenElementReadyCheck(FullscreenCheckType::ExemptIFrameAllowFullscreenRequirement, element)) {
        ERROR_LOG(LOGIDENTIFIER, error);
        return Exception { ExceptionCode::TypeError, error };
    }

    // If pending fullscreen element is unset or another element's was requested,
    // issue a cancel fullscreen request to the client
    if (m_pendingFullscreenElement != &element) {
        INFO_LOG(LOGIDENTIFIER, "Pending element mismatch; issuing exit fullscreen request");
        page->chrome().client().exitFullScreenForElement(&element, [weakThis = WeakPtr { *this }] {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            protectedThis->didExitFullscreen([] (auto) { });
        });
        return Exception { ExceptionCode::TypeError, "Element requested for fullscreen has changed."_s };
    }

    INFO_LOG(LOGIDENTIFIER);
    ASSERT(page->isDocumentFullscreenEnabled());

#if ENABLE(VIDEO)
    if (RefPtr mediaElement = dynamicDowncast<HTMLMediaElement>(element))
        mediaElement->willBecomeFullscreenElement(mode);
    else
#endif
        element.willBecomeFullscreenElement();

    ASSERT(&element == m_pendingFullscreenElement);
    m_pendingFullscreenElement = nullptr;

    m_fullscreenElement = element;

    Vector<Ref<Element>> ancestors { { element } };
    for (RefPtr<Frame> frame = element.document().frame(); frame; frame = frame->tree().parent()) {
        if (RefPtr ownerElement = frame->ownerElement())
            ancestors.append(ownerElement.releaseNonNull());
    }

    bool elementWasFullscreen = &element == protect(protect(element.document())->fullscreen())->fullscreenElement();
    for (auto ancestor : ancestors | std::views::reverse)
        elementEnterFullscreen(ancestor);

    if (RefPtr iframe = dynamicDowncast<HTMLIFrameElement>(element); iframe && !elementWasFullscreen)
        iframe->setIFrameFullscreenFlag(true);

    return { };
}

void DocumentFullscreen::elementEnterFullscreen(Element& element)
{
    Ref document = element.document();
    if (&element == protect(document->fullscreen())->fullscreenElement())
        return;

    RefPtr hideUntil = element.topmostPopoverAncestor(Element::TopLayerElementType::Other);
    document->hideAllPopoversUntil(hideUntil.get(), FocusPreviousElement::No, FireEvents::No);

    auto containingBlockBeforeStyleResolution = SingleThreadWeakPtr<RenderBlock> { };
    if (CheckedPtr renderer = element.renderer())
        containingBlockBeforeStyleResolution = renderer->containingBlock();

    element.setFullscreenFlag(true);
    document->resolveStyle(Document::ResolveStyleType::Rebuild);

    // Remove before adding, so we always add at the end of the top layer.
    if (element.isInTopLayer())
        element.removeFromTopLayer();
    element.addToTopLayer();

    queueFullscreenChangeEventForDocument(document);

    RenderElement::markRendererDirtyAfterTopLayerChange(element.checkedRenderer().get(), containingBlockBeforeStyleResolution.get());
}

bool DocumentFullscreen::didEnterFullscreen()
{
    RefPtr fullscreenElement = m_fullscreenElement;
    if (!fullscreenElement) {
        ERROR_LOG(LOGIDENTIFIER, "No fullscreenElement; bailing");
        return false;
    }

    if (backForwardCacheState() != Document::NotInBackForwardCache) {
        ERROR_LOG(LOGIDENTIFIER, "Document in the BackForwardCache; bailing");
        return false;
    }
    INFO_LOG(LOGIDENTIFIER);

    fullscreenElement->didBecomeFullscreenElement();
    return true;
}

// MARK: - Simple fullscreen document (exit helper).
// https://fullscreen.spec.whatwg.org/#simple-fullscreen-document

bool DocumentFullscreen::isSimpleFullscreenDocument() const
{
    bool foundFullscreenFlag = false;
    for (Ref element : document().topLayerElements()) {
        if (element->hasFullscreenFlag()) {
            if (foundFullscreenFlag)
                return false;
            foundFullscreenFlag = true;
        }
    }
    return foundFullscreenFlag;
}

Page* DocumentFullscreen::page() const
{
    return document().page();
}

// MARK: - Simple helper to get document frame
LocalFrame* DocumentFullscreen::frame() const
{
    return document().frame();
}

// MARK: - Collect documents to unfullscreen (exit helper).
// https://fullscreen.spec.whatwg.org/#collect-documents-to-unfullscreen

static Vector<Ref<Document>> documentsToUnfullscreen(Frame& firstFrame)
{
    Vector<Ref<Document>> documents;
    for (RefPtr frame = firstFrame; frame; frame = frame->tree().parent()) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        RefPtr document = localFrame->document();
        if (!document)
            continue;
        documents.append(*document);
        ASSERT(protect(document->fullscreen())->fullscreenElement());
        if (!protect(document->fullscreen())->isSimpleFullscreenDocument())
            break;
        if (RefPtr iframe = dynamicDowncast<HTMLIFrameElement>(document->ownerElement()); iframe && iframe->hasIFrameFullscreenFlag())
            break;
    }
    return documents;
}

// MARK: - Clear fullscreen flags (exit helper).
// https://fullscreen.spec.whatwg.org/#unfullscreen-an-element

static void clearFullscreenFlags(Element& element)
{
    element.setFullscreenFlag(false);
    if (auto* iframe = dynamicDowncast<HTMLIFrameElement>(element))
        iframe->setIFrameFullscreenFlag(false);
}

// MARK: - Exit fullscreen.
// https://fullscreen.spec.whatwg.org/#exit-fullscreen

void DocumentFullscreen::exitFullscreen(Document& document, Ref<DeferredPromise>&& promise)
{
    if (!document.isFullyActive() || !protect(document.fullscreen())->fullscreenElement()) {
        promise->reject(Exception { ExceptionCode::TypeError, "Not in fullscreen"_s });
        return;
    }
    protect(document.fullscreen())->exitFullscreen([promise = WTF::move(promise)](auto result) {
        if (result.hasException())
            promise->reject(result.releaseException());
        else
            promise->resolve();
    });
}

void DocumentFullscreen::webkitExitFullscreen(Document& document)
{
    if (protect(document.fullscreen())->fullscreenElement())
        protect(document.fullscreen())->exitFullscreen([] (auto) { });
}

void DocumentFullscreen::exitFullscreen(CompletionHandler<void(ExceptionOr<void>)>&& completionHandler)
{
    INFO_LOG(LOGIDENTIFIER);

    m_pendingExitFullscreen = true;
    auto resetPendingExitFullscreenScope = makeScopeExit([weakThis = WeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->m_pendingExitFullscreen = false;
    });

    Ref exitingDocument = document();
    auto mode = ExitMode::NoResize;
    Vector<Ref<Document>> exitDocuments;
    if (RefPtr exitingFrame = exitingDocument->frame())
        exitDocuments = documentsToUnfullscreen(*exitingFrame);

    RefPtr mainFrameDocument = this->mainFrameDocument();

    bool exitsTopDocument = exitDocuments.containsIf([&](auto& document) {
        return document.ptr() == mainFrameDocument.get();
    });
    if (!mainFrameDocument || (exitsTopDocument && protect(mainFrameDocument->fullscreen())->isSimpleFullscreenDocument())) {
        mode = ExitMode::Resize;
        if (mainFrameDocument)
            exitingDocument = *mainFrameDocument;
    }

    if (RefPtr element = protect(exitingDocument->fullscreen())->fullscreenElement(); element && !element->isConnected()) {
        queueFullscreenChangeEventForDocument(exitingDocument);
        clearFullscreenFlags(*element);
        element->removeFromTopLayer();
    }

    // Return promise, and run the remaining steps in parallel.
    exitingDocument->eventLoop().queueTask(TaskSource::MediaElement, [scope = CompletionHandlerScope(WTF::move(completionHandler)), resetPendingExitFullscreenScope = WTF::move(resetPendingExitFullscreenScope), weakThis = WeakPtr { *this }, mode, identifier = LOGIDENTIFIER]() mutable {
        auto completionHandler = scope.release();
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return completionHandler({ });

        RefPtr page = protectedThis->page();
        if (!page) {
            ERROR_LOG_WITH_THIS(protectedThis, identifier, "task - Document not in page; bailing.");
            return completionHandler({ });
        }

        // If there is a pending fullscreen element but no fullscreen element
        // there is a pending task in requestFullscreenForElement(). Cause it to cancel and fire an error
        // by clearing the pending fullscreen element.
        RefPtr exitedFullscreenElement = protectedThis->fullscreenElement();
        if (!exitedFullscreenElement && protectedThis->m_pendingFullscreenElement) {
            INFO_LOG_WITH_THIS(protectedThis, identifier, "task - Cancelling pending fullscreen request.");
            protectedThis->m_pendingFullscreenElement = nullptr;
            return completionHandler({ });
        }

        // Notify the chrome of the new full screen element.
        if (mode == ExitMode::Resize) {
            page->chrome().client().exitFullScreenForElement(exitedFullscreenElement.get(), [weakThis = WTF::move(weakThis), completionHandler = WTF::move(completionHandler), resetPendingExitFullscreenScope = WTF::move(resetPendingExitFullscreenScope)] mutable {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return completionHandler({ });
                protectedThis->didExitFullscreen(WTF::move(completionHandler));
            });
        } else {
            if (RefPtr frame = protectedThis->document().frame())
                protectedThis->finishExitFullscreen(*frame, ExitMode::NoResize);

            // We just popped off one fullscreen element out of the top layer, query the new one.
            protectedThis->m_pendingFullscreenElement = protectedThis->fullscreenElement();
            if (protectedThis->m_pendingFullscreenElement) {
                page->chrome().client().enterFullScreenForElement(Ref { *protectedThis->m_pendingFullscreenElement }, HTMLMediaElementEnums::VideoFullscreenModeStandard, WTF::move(completionHandler), [weakThis = WTF::move(weakThis), resetPendingExitFullscreenScope = WTF::move(resetPendingExitFullscreenScope)](bool success) mutable {
                    RefPtr protectedThis = weakThis.get();
                    if (!protectedThis || !success)
                        return true;
                    return protectedThis->didEnterFullscreen();
                });
            } else
                completionHandler({ });
        }
    });
}

void DocumentFullscreen::finishExitFullscreen(Frame& currentFrame, ExitMode mode)
{
    RefPtr currentLocalFrame = dynamicDowncast<LocalFrame>(currentFrame);
    if (currentLocalFrame && currentLocalFrame->document() && !protect(protect(currentLocalFrame->document())->fullscreen())->fullscreenElement())
        return;

    // Let descendantDocs be an ordered set consisting of doc’s descendant browsing contexts' active documents whose fullscreen element is non-null, if any, in tree order.
    Vector<Ref<Document>> descendantDocuments;
    for (RefPtr descendant = currentFrame.tree().traverseNext(); descendant; descendant = descendant->tree().traverseNext()) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(descendant);
        if (!localFrame)
            continue;
        if (RefPtr document = localFrame->document(); document && protect(document->fullscreen())->fullscreenElement())
            descendantDocuments.append(document.releaseNonNull());
    }

    auto unfullscreenDocument = [](const Ref<Document>& document) {
        Vector<Ref<Element>> toRemove;
        for (Ref element : document->topLayerElements()) {
            if (!element->hasFullscreenFlag())
                continue;
            clearFullscreenFlags(element);
            toRemove.append(element);
        }
        for (Ref element : toRemove)
            element->removeFromTopLayer();
    };

    auto exitDocuments = documentsToUnfullscreen(currentFrame);
    for (Ref exitDocument : exitDocuments) {
        queueFullscreenChangeEventForDocument(exitDocument);
        if (mode == ExitMode::Resize)
            unfullscreenDocument(exitDocument);
        else {
            RefPtr fullscreenElement = protect(exitDocument->fullscreen())->fullscreenElement();
            clearFullscreenFlags(*fullscreenElement);
            fullscreenElement->removeFromTopLayer();
        }
    }

    for (Ref descendantDocument : descendantDocuments | std::views::reverse) {
        queueFullscreenChangeEventForDocument(descendantDocument);
        unfullscreenDocument(descendantDocument);
    }
}

bool DocumentFullscreen::willExitFullscreen()
{
    RefPtr fullscreenElement = fullscreenOrPendingElement();
    if (!fullscreenElement) {
        ERROR_LOG(LOGIDENTIFIER, "No fullscreenOrPendingElement(); bailing");
        return false;
    }

    if (backForwardCacheState() != Document::NotInBackForwardCache) {
        ERROR_LOG(LOGIDENTIFIER, "Document in the BackForwardCache; bailing");
        return false;
    }
    INFO_LOG(LOGIDENTIFIER);

    fullscreenElement->willStopBeingFullscreenElement();
    return true;
}

void DocumentFullscreen::didExitFullscreen(CompletionHandler<void(ExceptionOr<void>)>&& completionHandler)
{
    if (backForwardCacheState() != Document::NotInBackForwardCache) {
        ERROR_LOG(LOGIDENTIFIER, "Document in the BackForwardCache; bailing");
        return completionHandler(Exception { ExceptionCode::TypeError });
    }
    INFO_LOG(LOGIDENTIFIER);

    if (RefPtr frame = document().frame())
        finishExitFullscreen(frame->protectedMainFrame(), ExitMode::Resize);

    if (RefPtr exitedFullscreenElement = fullscreenOrPendingElement())
        exitedFullscreenElement->didStopBeingFullscreenElement();

    m_areKeysEnabledInFullscreen = false;
    m_fullscreenElement = nullptr;
    m_pendingFullscreenElement = nullptr;

    completionHandler({ });
}

// MARK: - Removing steps.
// https://fullscreen.spec.whatwg.org/#removing-steps

void DocumentFullscreen::exitRemovedFullscreenElement(Element& element)
{
    ASSERT(element.hasFullscreenFlag());

    if (fullscreenElement() == &element) {
        INFO_LOG(LOGIDENTIFIER, "Fullscreen element removed; exiting fullscreen");
        exitFullscreen([] (auto) { });
    } else
        clearFullscreenFlags(element);
}

// MARK: - Fully exit fullscreen.
// Removes all fullscreen elements from the top layer for all documents.
// https://fullscreen.spec.whatwg.org/#fully-exit-fullscreen

void DocumentFullscreen::fullyExitFullscreen()
{
    RefPtr<Document> rootFrameDocument;
    if (RefPtr frame = document().frame())
        rootFrameDocument = frame->rootFrame().document();

    if (!rootFrameDocument || !protect(rootFrameDocument->fullscreen())->fullscreenElement()) {
        // If there is a pending fullscreen element but no top document fullscreen element,Add commentMore actions
        // there is a pending task in enterFullscreen(). Cause it to cancel and fire an error
        // by clearing the pending fullscreen element.
        m_pendingFullscreenElement = nullptr;
        INFO_LOG(LOGIDENTIFIER, "Cancelling pending fullscreen request.");
        return;
    }

    INFO_LOG(LOGIDENTIFIER);

    m_pendingExitFullscreen = true;
    auto resetPendingExitFullscreenScope = makeScopeExit([weakThis = WeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->m_pendingExitFullscreen = false;
    });

    protect(document())->eventLoop().queueTask(TaskSource::MediaElement, [weakThis = WeakPtr { *this }, resetPendingExitFullscreenScope = WTF::move(resetPendingExitFullscreenScope), rootFrameDocument = WTF::move(rootFrameDocument), identifier = LOGIDENTIFIER] mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (!rootFrameDocument->page()) {
            INFO_LOG_WITH_THIS(protectedThis, identifier, "Top document has no page.");
            return;
        }

        // This triggers finishExitFullscreen with ExitMode::Resize, which fully exits the document.
        if (RefPtr fullscreenElement = protect(rootFrameDocument->fullscreen())->fullscreenElement()) {
            rootFrameDocument->page()->chrome().client().exitFullScreenForElement(fullscreenElement.get(), [weakThis = WTF::move(weakThis), resetPendingExitFullscreenScope = WTF::move(resetPendingExitFullscreenScope)] mutable {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;
                protectedThis->didExitFullscreen([] (auto) { });
            });
        } else
            INFO_LOG_WITH_THIS(protectedThis, identifier, "Top document has no fullscreen element");
    });
}

static bool hasJSEventListener(Node& node, const AtomString& eventType)
{
    for (const auto& listener : node.eventListeners(eventType)) {
        if (listener->callback().type() == EventListener::JSEventListenerType)
            return true;
    }

    return false;
}

// MARK: - Fullscreen rendering update steps / event dispatching.
// https://fullscreen.spec.whatwg.org/#run-the-fullscreen-steps

void DocumentFullscreen::dispatchPendingEvents()
{
    // Steps 1-2:
    auto pendingEvents = std::exchange(m_pendingEvents, { });

    // Step 3:
    while (!pendingEvents.isEmpty()) {
        auto [eventType, element] = pendingEvents.takeFirst();

        // Gaining or losing fullscreen state may change viewport arguments
        protect(element->document())->updateViewportArguments();
        if (&element->document() != &document())
            protect(document())->updateViewportArguments();

#if ENABLE(VIDEO)
        if (eventType == EventType::Change) {
            if (RefPtr mediaElement = dynamicDowncast<HTMLMediaElement>(element.get()))
                mediaElement->enteredOrExitedFullscreen();
        }
#endif
        // Let target be element if element is connected and its node document is document, and otherwise let target be document.
        Ref target = [&]() -> Node& {
            if (element->isConnected() && &element->document() == &document())
                return element;
            return document();
        }();

        switch (eventType) {
        case EventType::Change: {
            Ref targetDocument = target->document();
            target->dispatchEvent(Event::create(eventNames().fullscreenchangeEvent, Event::CanBubble::Yes, Event::IsCancelable::No, Event::IsComposed::Yes));
            bool shouldEmitUnprefixed = !(hasJSEventListener(target, eventNames().webkitfullscreenchangeEvent) && hasJSEventListener(target, eventNames().fullscreenchangeEvent)) && !(hasJSEventListener(targetDocument, eventNames().webkitfullscreenchangeEvent) && hasJSEventListener(targetDocument, eventNames().fullscreenchangeEvent));
            if (shouldEmitUnprefixed)
                target->dispatchEvent(Event::create(eventNames().webkitfullscreenchangeEvent, Event::CanBubble::Yes, Event::IsCancelable::No, Event::IsComposed::Yes));
            break;
        }
        case EventType::Error:
            target->dispatchEvent(Event::create(eventNames().fullscreenerrorEvent, Event::CanBubble::Yes, Event::IsCancelable::No, Event::IsComposed::Yes));
            target->dispatchEvent(Event::create(eventNames().webkitfullscreenerrorEvent, Event::CanBubble::Yes, Event::IsCancelable::No, Event::IsComposed::Yes));
            break;
        }
    }
}

void DocumentFullscreen::queueFullscreenChangeEventForDocument(Document& document)
{
    RefPtr target = protect(document.fullscreen())->fullscreenElement();
    if (!target) {
        ASSERT_NOT_REACHED();
        return;
    }
    protect(document.fullscreen())->queueFullscreenChangeEventForElement(*target);
    document.scheduleRenderingUpdate(RenderingUpdateStep::Fullscreen);
}

// MARK: - Fullscreen animation pseudo-class.

bool DocumentFullscreen::isAnimatingFullscreen() const
{
    return m_isAnimatingFullscreen;
}

void DocumentFullscreen::setAnimatingFullscreen(bool flag)
{
    if (m_isAnimatingFullscreen == flag)
        return;

    INFO_LOG(LOGIDENTIFIER, flag);

    std::optional<Style::PseudoClassChangeInvalidation> styleInvalidation;
    if (RefPtr fullscreenElement = this->fullscreenElement())
        emplace(styleInvalidation, *fullscreenElement, { { CSSSelector::PseudoClass::InternalAnimatingFullscreenTransition, flag } });
    m_isAnimatingFullscreen = flag;
}

void DocumentFullscreen::clear()
{
    m_pendingFullscreenElement = nullptr;
    m_fullscreenElement = nullptr;
}

// MARK: - Log channel.

#if !RELEASE_LOG_DISABLED
WTFLogChannel& DocumentFullscreen::logChannel() const
{
    return LogFullscreen;

}
#endif

}

#endif
