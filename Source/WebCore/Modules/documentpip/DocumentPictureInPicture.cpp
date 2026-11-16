/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "DocumentPictureInPicture.h"

#if ENABLE(DOCUMENT_PIP)

#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "DocumentPage.h"
#include "DocumentPictureInPictureEvent.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "FrameLoadRequest.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "JSDOMPromiseDeferred.h"
#include "JSWindowProxy.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "Page.h"
#include "WindowFeatures.h"
#include "WindowProxy.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DocumentPictureInPicture);

DocumentPictureInPicture::DocumentPictureInPicture(LocalDOMWindow& window)
    : LocalDOMWindowProperty(&window)
{
}

DocumentPictureInPicture::~DocumentPictureInPicture() = default;

ScriptExecutionContext* DocumentPictureInPicture::scriptExecutionContext() const
{
    RefPtr window = this->window();
    return window ? window->document() : nullptr;
}

LocalFrame* DocumentPictureInPicture::openFrame() const
{
    RefPtr frame = m_lastOpenedFrame;
    if (!frame)
        return nullptr;

    RefPtr window = frame->window();
    if (!window || window->closed())
        return nullptr;

    return m_lastOpenedFrame.get();
}

// https://wicg.github.io/document-picture-in-picture/#dom-documentpictureinpicture-window
WindowProxy* DocumentPictureInPicture::pictureInPictureWindow() const
{
    if (RefPtr frame = openFrame())
        return &frame->windowProxy();
    return nullptr;
}

// https://html.spec.whatwg.org/multipage/document-sequences.html#close-a-top-level-traversable
static void closePictureInPictureFrame(LocalFrame& frame)
{
    RefPtr page = frame.page();
    if (!page || page->isClosing())
        return;

    // Unlike window.close(), closing a top-level traversable does not set its is closing flag, so
    // window.closed only becomes true once the window is destroyed, after pagehide.
    // See https://github.com/whatwg/html/issues/11853.
    if (RefPtr window = frame.window())
        window->closePage();
}

// https://wicg.github.io/document-picture-in-picture/#dom-documentpictureinpicture-requestwindow
void DocumentPictureInPicture::requestWindow(Options&& options, Ref<DeferredPromise>&& promise)
{
    RefPtr window = this->window();
    RefPtr frame = this->frame();
    RefPtr page = frame ? frame->page() : nullptr;
    RefPtr document = window ? window->document() : nullptr;
    if (!window || !frame || !page || !document) {
        promise->reject(Exception { ExceptionCode::InvalidStateError });
        return;
    }

    // 1. If Document Picture-in-Picture support is false, throw a "NotSupportedError" DOMException.
    if (!page->chrome().client().supportsDocumentPictureInPicture()) {
        promise->reject(Exception { ExceptionCode::NotSupportedError, "Document Picture-in-Picture is not supported."_s });
        return;
    }

    // 2. If this's relevant global object's navigable is not a top-level traversable, throw a "NotAllowedError" DOMException.
    if (!frame->isMainFrame()) {
        promise->reject(Exception { ExceptionCode::NotAllowedError, "Document Picture-in-Picture can only be requested from a top-level window."_s });
        return;
    }

    // 3. If this's relevant global object's navigable's Is Document Picture-in-Picture boolean is true, throw a "NotAllowedError" DOMException.
    if (page->isDocumentPictureInPicture()) {
        promise->reject(Exception { ExceptionCode::NotAllowedError, "Document Picture-in-Picture cannot be requested from a Document Picture-in-Picture window."_s });
        return;
    }

    // 4. If this's relevant global object does not have transient activation, throw a "NotAllowedError" DOMException.
    if (!window->hasTransientActivation()) {
        promise->reject(Exception { ExceptionCode::NotAllowedError, "Document Picture-in-Picture requires a user gesture."_s });
        return;
    }

    // 5. & 6. If only one of width and height is greater than zero, throw a RangeError.
    if (!!options.width != !!options.height) {
        promise->reject(Exception { ExceptionCode::RangeError, "Both width and height must be specified."_s });
        return;
    }

    // 7. Consume user activation given this's relevant global object.
    window->consumeTransientActivation();

    // 8. If this's last-opened window is not closed, close its navigable.
    if (RefPtr existingFrame = openFrame())
        closePictureInPictureFrame(*existingFrame);

    // 10. Create a new top-level traversable given this's relevant global object's navigable's active browsing context and "_blank".
    WindowFeatures windowFeatures;
    windowFeatures.popup = true;
    windowFeatures.isDocumentPictureInPicture = true;
    // 13. & 14. Size the window's viewport to the requested width and height. The UI process
    // will clamp these values to fit a user-friendly window size.
    if (options.width && options.height) {
        windowFeatures.width = static_cast<float>(std::min<uint64_t>(options.width, std::numeric_limits<int>::max()));
        windowFeatures.height = static_cast<float>(std::min<uint64_t>(options.height, std::numeric_limits<int>::max()));
    }

    FrameLoadRequest frameLoadRequest { *document, protect(document->securityOrigin()), ResourceRequest { URL { aboutBlankURL() } }, blankTargetFrameName(), InitiatedByMainFrame::Yes };
    auto [newFrame, created] = WebCore::createWindow(*frame, WTF::move(frameLoadRequest), WTF::move(windowFeatures));
    RefPtr newLocalFrame = dynamicDowncast<LocalFrame>(newFrame);
    RefPtr newPage = newLocalFrame ? newLocalFrame->page() : nullptr;
    RefPtr newDocument = newLocalFrame ? newLocalFrame->document() : nullptr;
    if (created != CreatedNewPage::Yes || !newPage || !newDocument) {
        promise->reject(Exception { ExceptionCode::NotAllowedError, "Unable to open a Document Picture-in-Picture window."_s });
        return;
    }

    newPage->setOpenedByDOM();
    newPage->setOpenedByDOMWithOpener(true);

    // 11. Set pip traversable's active document's mode to this's relevant global object's associated Document's mode.
    newDocument->setCompatibilityMode(document->compatibilityMode());

    // The PiP document's URL is about:blank. The initial empty document's URL is otherwise empty,
    // which would make fragment navigations within the PiP document load a new document.
    newDocument->setURL(URL { aboutBlankURL() });

    // The PiP document's base URL falls back to that of the initiator; the initial about:blank
    // document's base URL is computed before its opener is set, so set it explicitly here.
    // See https://github.com/whatwg/html/issues/421.
    newDocument->setBaseURLOverride(document->baseURL());

    // 12. Set pip traversable's Is Document Picture-in-Picture boolean to true.
    newPage->setIsDocumentPictureInPicture(true);

    // 16. Set this's last-opened window to pip traversable's active window.
    m_lastOpenedFrame = newLocalFrame.get();

    Ref windowProxy = newLocalFrame->windowProxy();

    // 17. Queue a global task on the DOM manipulation task source to fire an event named enter.
    protect(document->eventLoop())->queueTask(TaskSource::DOMManipulation, [protectedThis = Ref { *this }, windowProxy] mutable {
        protectedThis->dispatchEvent(DocumentPictureInPictureEvent::create(eventNames().enterEvent, { { }, WTF::move(windowProxy) }, Event::IsTrusted::Yes));
    });

    // 18. Return a promise resolved with pip traversable's active window.
    promise->resolve<IDLInterface<WindowProxy>>(windowProxy.get());
}

// https://wicg.github.io/document-picture-in-picture/#close-any-associated-document-picture-in-picture-windows
void DocumentPictureInPicture::closeAssociatedWindows(Document& document)
{
    // 1. & 2. If document's node navigable is not a top-level traversable, abort these steps.
    RefPtr frame = document.frame();
    if (!frame || !frame->isMainFrame())
        return;

    RefPtr page = frame->page();
    if (!page)
        return;

    // 3. If navigable's Is Document Picture-in-Picture boolean is true, then close navigable and abort these steps.
    if (page->isDocumentPictureInPicture()) {
        closePictureInPictureFrame(*frame);
        return;
    }

    // 4. & 5. If navigable's active window's last-opened window is not closed, close its navigable.
    RefPtr window = document.window();
    RefPtr documentPictureInPicture = window ? window->documentPictureInPictureIfExists() : nullptr;
    if (!documentPictureInPicture)
        return;

    if (RefPtr pictureInPictureFrame = documentPictureInPicture->openFrame())
        closePictureInPictureFrame(*pictureInPictureFrame);
}

} // namespace WebCore

#endif // ENABLE(DOCUMENT_PIP)
