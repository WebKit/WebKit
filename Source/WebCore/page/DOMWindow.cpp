/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "DOMWindow.h"

#include "BackForwardController.h"
#include "CSSRuleList.h"
#include "CSSStyleDeclaration.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTTPParsers.h"
#include "LocalDOMWindow.h"
#include "Location.h"
#include "MediaQueryList.h"
#include "NodeList.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "RemoteDOMWindow.h"
#include "ResourceLoadObserver.h"
#include "ScheduledAction.h"
#include "SecurityOrigin.h"
#include "WebCoreOpaqueRoot.h"
#include "WebKitPoint.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(DOMWindow);

DOMWindow::DOMWindow(GlobalWindowIdentifier&& identifier, DOMWindowType type)
    : m_identifier(WTFMove(identifier))
    , m_type(type)
{
}

DOMWindow::~DOMWindow() = default;

ExceptionOr<RefPtr<SecurityOrigin>> DOMWindow::createTargetOriginForPostMessage(const String& targetOrigin, Document& sourceDocument)
{
    RefPtr<SecurityOrigin> targetSecurityOrigin;
    if (targetOrigin == "/"_s)
        targetSecurityOrigin = &sourceDocument.securityOrigin();
    else if (targetOrigin != "*"_s) {
        targetSecurityOrigin = SecurityOrigin::createFromString(targetOrigin);
        // It doesn't make sense target a postMessage at an opaque origin
        // because there's no way to represent an opaque origin in a string.
        if (targetSecurityOrigin->isOpaque())
            return Exception { ExceptionCode::SyntaxError };
    }
    return targetSecurityOrigin;
}

Location& DOMWindow::location()
{
    if (!m_location)
        m_location = Location::create(*this);
    return *m_location;
}

bool DOMWindow::closed() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return true;

    RefPtr page = frame->page();
    return !page || page->isClosing();
}

void DOMWindow::close(Document& document)
{
    if (!document.canNavigate(protectedFrame().get()))
        return;
    close();
}

void DOMWindow::close()
{
    RefPtr frame = this->frame();
    if (!frame)
        return;

    RefPtr page = frame->page();
    if (!page)
        return;

    if (!frame->isMainFrame())
        return;

    if (!(page->openedByDOM() || page->backForward().count() <= 1)) {
        checkedConsole()->addMessage(MessageSource::JS, MessageLevel::Warning, "Can't close the window since it was not opened by JavaScript"_s);
        return;
    }

    RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
    if (localFrame && !localFrame->checkedLoader()->shouldClose())
        return;

    ResourceLoadObserver::shared().updateCentralStatisticsStore([] { });

    page->setIsClosing();
    closePage();
}

PageConsoleClient* DOMWindow::console() const
{
    auto* frame = this->frame();
    return frame && frame->page() ? &frame->page()->console() : nullptr;
}

CheckedPtr<PageConsoleClient> DOMWindow::checkedConsole() const
{
    return console();
}

RefPtr<Frame> DOMWindow::protectedFrame() const
{
    return frame();
}

WebCoreOpaqueRoot root(DOMWindow* window)
{
    return WebCoreOpaqueRoot { window };
}

WindowProxy* DOMWindow::opener() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return nullptr;

    RefPtr openerFrame = frame->opener();
    if (!openerFrame)
        return nullptr;

    return &openerFrame->windowProxy();
}

WindowProxy* DOMWindow::top() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return nullptr;

    if (!frame->page())
        return nullptr;

    return &frame->tree().top().windowProxy();
}

WindowProxy* DOMWindow::parent() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return nullptr;

    RefPtr parentFrame = frame->tree().parent();
    if (parentFrame)
        return &parentFrame->windowProxy();

    return &frame->windowProxy();
}

ExceptionOr<AtomString> DOMWindow::name() const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->name();
}

ExceptionOr<void> DOMWindow::setName(const AtomString& name)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    localThis->setName(name);
    return { };
}

ExceptionOr<String> DOMWindow::status() const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->status();
}

ExceptionOr<void> DOMWindow::setStatus(const String& status)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    localThis->setStatus(status);
    return { };
}

unsigned DOMWindow::length() const
{
    switch (m_type) {
    case DOMWindowType::Local:
        return downcast<LocalDOMWindow>(*this).length();
    case DOMWindowType::Remote:
        return downcast<RemoteDOMWindow>(*this).length();
    }
    RELEASE_ASSERT_NOT_REACHED();
}

Document* DOMWindow::documentIfLocal()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return nullptr;
    return localThis->document();
}

ExceptionOr<Document*> DOMWindow::document() const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->document();
}

ExceptionOr<History&> DOMWindow::history()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->history();
}

ExceptionOr<CustomElementRegistry&> DOMWindow::ensureCustomElementRegistry()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->ensureCustomElementRegistry();
}

ExceptionOr<BarProp&> DOMWindow::locationbar()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->locationbar();
}

ExceptionOr<BarProp&> DOMWindow::menubar()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->menubar();
}

ExceptionOr<BarProp&> DOMWindow::personalbar()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->personalbar();
}

ExceptionOr<BarProp&> DOMWindow::scrollbars()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->scrollbars();
}

ExceptionOr<BarProp&> DOMWindow::statusbar()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->statusbar();
}

ExceptionOr<BarProp&> DOMWindow::toolbar()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->toolbar();
}

ExceptionOr<Navigation&> DOMWindow::navigation()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->navigation();
}

ExceptionOr<int> DOMWindow::outerHeight() const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->outerHeight();
}

ExceptionOr<int> DOMWindow::outerWidth() const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->outerWidth();
}

ExceptionOr<int> DOMWindow::innerHeight() const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->innerHeight();
}

ExceptionOr<int> DOMWindow::innerWidth() const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->innerWidth();
}

ExceptionOr<int> DOMWindow::screenX() const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->screenX();
}

ExceptionOr<int> DOMWindow::screenY() const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->screenY();
}

ExceptionOr<int> DOMWindow::screenLeft() const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->screenLeft();
}

ExceptionOr<int> DOMWindow::screenTop() const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->screenTop();
}

ExceptionOr<int> DOMWindow::scrollX() const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->scrollX();
}

ExceptionOr<int> DOMWindow::scrollY() const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->scrollY();
}

ExceptionOr<HTMLFrameOwnerElement*> DOMWindow::frameElement() const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->frameElement();
}

ExceptionOr<Navigator&> DOMWindow::navigator()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->navigator();
}

ExceptionOr<bool> DOMWindow::offscreenBuffering() const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->offscreenBuffering();
}

ExceptionOr<CookieStore&> DOMWindow::cookieStore()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->cookieStore();
}

ExceptionOr<Screen&> DOMWindow::screen()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->screen();
}

ExceptionOr<double> DOMWindow::devicePixelRatio() const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->devicePixelRatio();
}

ExceptionOr<StyleMedia&> DOMWindow::styleMedia()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->styleMedia();
}

ExceptionOr<VisualViewport&> DOMWindow::visualViewport()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->visualViewport();
}

ExceptionOr<Storage*> DOMWindow::localStorage()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return { { } };
    return localThis->localStorage();
}

ExceptionOr<Storage*> DOMWindow::sessionStorage()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return { { } };
    return localThis->sessionStorage();
}

ExceptionOr<String> DOMWindow::origin() const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->origin();
}

ExceptionOr<bool> DOMWindow::isSecureContext() const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->isSecureContext();
}

ExceptionOr<bool> DOMWindow::crossOriginIsolated() const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->crossOriginIsolated();
}

void DOMWindow::focus(LocalDOMWindow& incumbentWindow)
{
    switch (m_type) {
    case DOMWindowType::Local:
        return downcast<LocalDOMWindow>(*this).focus(incumbentWindow);
    case DOMWindowType::Remote:
        return downcast<RemoteDOMWindow>(*this).focus(incumbentWindow);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void DOMWindow::blur()
{
    switch (m_type) {
    case DOMWindowType::Local:
        return downcast<LocalDOMWindow>(*this).blur();
    case DOMWindowType::Remote:
        return downcast<RemoteDOMWindow>(*this).blur();
    }
    RELEASE_ASSERT_NOT_REACHED();
}

ExceptionOr<void> DOMWindow::print()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    localThis->print();
    return { };
}

ExceptionOr<void> DOMWindow::stop()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    localThis->stop();
    return { };
}

ExceptionOr<Performance&> DOMWindow::performance() const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->performance();
}

ExceptionOr<void> DOMWindow::postMessage(JSC::JSGlobalObject& globalObject, LocalDOMWindow& incumbentWindow, JSC::JSValue message, WindowPostMessageOptions&& options)
{
    switch (m_type) {
    case DOMWindowType::Local:
        return downcast<LocalDOMWindow>(*this).postMessage(globalObject, incumbentWindow, message, WTFMove(options));
    case DOMWindowType::Remote:
        return downcast<RemoteDOMWindow>(*this).postMessage(globalObject, incumbentWindow, message, WTFMove(options));
    }
    RELEASE_ASSERT_NOT_REACHED();
}

ExceptionOr<void> DOMWindow::postMessage(JSC::JSGlobalObject& globalObject, LocalDOMWindow& incumbentWindow, JSC::JSValue message, String&& targetOrigin, Vector<JSC::Strong<JSC::JSObject>>&& transfer)
{
    return postMessage(globalObject, incumbentWindow, message, WindowPostMessageOptions { WTFMove(targetOrigin), WTFMove(transfer) });
}

ExceptionOr<Ref<CSSStyleDeclaration>> DOMWindow::getComputedStyle(Element& element, const String& pseudoElt) const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->getComputedStyle(element, pseudoElt);
}

ExceptionOr<RefPtr<WebCore::MediaQueryList>> DOMWindow::matchMedia(const String& media)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->matchMedia(media);
}

ExceptionOr<Crypto&> DOMWindow::crypto() const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->crypto();
}

ExceptionOr<RefPtr<WindowProxy>> DOMWindow::open(LocalDOMWindow& activeWindow, LocalDOMWindow& firstWindow, const String& urlString, const AtomString& frameName, const String& windowFeaturesString)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return { { } };
    return localThis->open(activeWindow, firstWindow, urlString, frameName, windowFeaturesString);
}

ExceptionOr<void> DOMWindow::alert(const String& message)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    localThis->alert(message);
    return { };
}

ExceptionOr<bool> DOMWindow::confirmForBindings(const String& message)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->confirmForBindings(message);
}

ExceptionOr<String> DOMWindow::prompt(const String& message, const String& defaultValue)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->prompt(message, defaultValue);
}

ExceptionOr<void> DOMWindow::captureEvents()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    localThis->captureEvents();
    return { };
}

ExceptionOr<void> DOMWindow::releaseEvents()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    localThis->releaseEvents();
    return { };
}

ExceptionOr<bool> DOMWindow::find(const String& string, bool caseSensitive, bool backwards, bool wrap, bool wholeWord, bool searchInFrames, bool showDialog) const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->find(string, caseSensitive, backwards, wrap, wholeWord, searchInFrames, showDialog);
}

ExceptionOr<int> DOMWindow::requestAnimationFrame(Ref<RequestAnimationFrameCallback>&& callback)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->requestAnimationFrame(WTFMove(callback));
}

ExceptionOr<int> DOMWindow::webkitRequestAnimationFrame(Ref<RequestAnimationFrameCallback>&& callback)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->webkitRequestAnimationFrame(WTFMove(callback));
}

ExceptionOr<void> DOMWindow::cancelAnimationFrame(int id)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    localThis->cancelAnimationFrame(id);
    return { };
}

ExceptionOr<int> DOMWindow::requestIdleCallback(Ref<IdleRequestCallback>&& callback, const IdleRequestOptions& options)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->requestIdleCallback(WTFMove(callback), options);
}

ExceptionOr<void> DOMWindow::cancelIdleCallback(int id)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    localThis->cancelIdleCallback(id);
    return { };
}

ExceptionOr<void> DOMWindow::createImageBitmap(ImageBitmap::Source&& source, ImageBitmapOptions&& options, ImageBitmap::Promise&& promise)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    localThis->createImageBitmap(WTFMove(source), WTFMove(options), WTFMove(promise));
    return { };
}

ExceptionOr<void> DOMWindow::createImageBitmap(ImageBitmap::Source&& source, int sx, int sy, int sw, int sh, ImageBitmapOptions&& options, ImageBitmap::Promise&& promise)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    localThis->createImageBitmap(WTFMove(source), sx, sy, sw, sh, WTFMove(options), WTFMove(promise));
    return { };
}

ExceptionOr<RefPtr<CSSRuleList>> DOMWindow::getMatchedCSSRules(Element* element, const String& pseudoElt, bool authorOnly) const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->getMatchedCSSRules(element, pseudoElt, authorOnly);
}

ExceptionOr<RefPtr<WebKitPoint>> DOMWindow::webkitConvertPointFromPageToNode(Node* node, const WebKitPoint* point) const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->webkitConvertPointFromPageToNode(node, point);
}

ExceptionOr<RefPtr<WebKitPoint>> DOMWindow::webkitConvertPointFromNodeToPage(Node* node, const WebKitPoint* point) const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->webkitConvertPointFromNodeToPage(node, point);
}

ExceptionOr<Ref<NodeList>> DOMWindow::collectMatchingElementsInFlatTree(Node& node, const String& selectors)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->collectMatchingElementsInFlatTree(node, selectors);
}

ExceptionOr<RefPtr<Element>> DOMWindow::matchingElementInFlatTree(Node& node, const String& selectors)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->matchingElementInFlatTree(node, selectors);
}

ExceptionOr<void> DOMWindow::scrollBy(const ScrollToOptions& options) const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    localThis->scrollBy(options);
    return { };
}

ExceptionOr<void> DOMWindow::scrollBy(double x, double y) const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    localThis->scrollBy(x, y);
    return { };
}

ExceptionOr<void> DOMWindow::scrollTo(const ScrollToOptions& options, ScrollClamping clamping, ScrollSnapPointSelectionMethod method, std::optional<FloatSize> originalScrollDelta) const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    localThis->scrollTo(options, clamping, method, originalScrollDelta);
    return { };
}

ExceptionOr<void> DOMWindow::scrollTo(double x, double y, ScrollClamping clamping) const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    localThis->scrollTo(x, y, clamping);
    return { };
}

ExceptionOr<void> DOMWindow::moveBy(int x, int y) const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    localThis->moveBy(x, y);
    return { };
}

ExceptionOr<void> DOMWindow::moveTo(int x, int y) const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    localThis->moveTo(x, y);
    return { };
}

ExceptionOr<void> DOMWindow::resizeBy(int x, int y) const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    localThis->resizeBy(x, y);
    return { };
}

ExceptionOr<void> DOMWindow::resizeTo(int width, int height) const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    localThis->resizeTo(width, height);
    return { };
}

ExceptionOr<DOMSelection*> DOMWindow::getSelection()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->getSelection();
}

ExceptionOr<int> DOMWindow::setTimeout(std::unique_ptr<ScheduledAction> action, int timeout, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->setTimeout(WTFMove(action), timeout, WTFMove(arguments));
}

ExceptionOr<void> DOMWindow::clearTimeout(int timeoutId)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    localThis->clearTimeout(timeoutId);
    return { };
}

ExceptionOr<int> DOMWindow::setInterval(std::unique_ptr<ScheduledAction> action, int timeout, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return { { } };
    return localThis->setInterval(WTFMove(action), timeout, WTFMove(arguments));
}

ExceptionOr<void> DOMWindow::clearInterval(int timeoutId)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    localThis->clearInterval(timeoutId);
    return { };
}

#if ENABLE(ORIENTATION_EVENTS)
ExceptionOr<IntDegrees> DOMWindow::orientation() const
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->orientation();
}
#endif

ExceptionOr<void> DOMWindow::reportError(JSDOMGlobalObject& globalObject, JSC::JSValue error)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    WindowOrWorkerGlobalScope::reportError(globalObject, error);
    return { };
}

ExceptionOr<JSC::JSValue> DOMWindow::structuredClone(JSDOMGlobalObject& lexicalGlobalObject, JSDOMGlobalObject& relevantGlobalObject, JSC::JSValue value, StructuredSerializeOptions&& options)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return WindowOrWorkerGlobalScope::structuredClone(lexicalGlobalObject, relevantGlobalObject, value, WTFMove(options));
}

ExceptionOr<String> DOMWindow::btoa(const String& stringToEncode)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return Base64Utilities::btoa(stringToEncode);
}

ExceptionOr<String> DOMWindow::atob(const String& stringToEncode)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return Base64Utilities::atob(stringToEncode);
}

} // namespace WebCore
