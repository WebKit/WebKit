/*
 * Copyright (C) 2018-2025 Apple Inc. All rights reserved.
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
#include "BarProp.h"
#include "CSSRuleList.h"
#include "CSSStyleProperties.h"
#include "CookieStore.h"
#include "CustomElementRegistry.h"
#include "DocumentSecurityOrigin.h"
#include "DocumentView.h"
#include "ExceptionOr.h"
#include "Frame.h"
#include "FrameConsoleClient.h"
#include "FrameLoader.h"
#include "HTTPParsers.h"
#include "History.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "Location.h"
#include "MediaQueryList.h"
#include "Navigation.h"
#include "Navigator.h"
#include "NodeList.h"
#include "Page.h"
#include "Performance.h"
#include "PlatformStrategies.h"
#include "RemoteDOMWindow.h"
#include "ResourceLoadObserver.h"
#include "ScheduledAction.h"
#include "Screen.h"
#include "SecurityOrigin.h"
#include "StyleMedia.h"
#include "VisualViewport.h"
#include "WebCoreOpaqueRoot.h"
#include "WebKitPoint.h"
#include "WindowProxy.h"
#include "dom/SandboxFlags.h"
#include "page/RemoteFrame.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TypeCasts.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DOMWindow);

DOMWindow::DOMWindow(GlobalWindowIdentifier&& identifier, DOMWindowType type)
    : m_identifier(WTF::move(identifier))
    , m_type(type)
{
}

DOMWindow::~DOMWindow() = default;

ExceptionOr<RefPtr<SecurityOrigin>> DOMWindow::createTargetOriginForPostMessage(const String& targetOrigin, Document& sourceDocument)
{
    RefPtr<SecurityOrigin> targetSecurityOrigin;
    if (targetOrigin == "/"_s)
        targetSecurityOrigin = sourceDocument.securityOrigin();
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
    if (document.canNavigate(protectedFrame().get()) != CanNavigateState::Able)
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

    if (!(page->openedByDOM() || page->checkedBackForward()->count() <= 1)) {
        checkedConsole()->addMessage(MessageSource::JS, MessageLevel::Warning, "Can't close the window since it was not opened by JavaScript"_s);
        return;
    }

    RefPtr localFrame = dynamicDowncast<LocalFrame>(frame);
    if (localFrame && !localFrame->loader().shouldClose())
        return;

    ResourceLoadObserver::singleton().updateCentralStatisticsStore([] { });

    page->setIsClosing();
    closePage();
}

FrameConsoleClient* DOMWindow::console() const
{
    RefPtr frame = dynamicDowncast<LocalFrame>(this->frame());
    return frame ? &frame->console() : nullptr;
}

CheckedPtr<FrameConsoleClient> DOMWindow::checkedConsole() const
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

RefPtr<Document> DOMWindow::protectedDocumentIfLocal()
{
    return documentIfLocal();
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
        return downcast<LocalDOMWindow>(*this).postMessage(globalObject, incumbentWindow, message, WTF::move(options));
    case DOMWindowType::Remote:
        return downcast<RemoteDOMWindow>(*this).postMessage(globalObject, incumbentWindow, message, WTF::move(options));
    }
    RELEASE_ASSERT_NOT_REACHED();
}

ExceptionOr<void> DOMWindow::postMessage(JSC::JSGlobalObject& globalObject, LocalDOMWindow& incumbentWindow, JSC::JSValue message, String&& targetOrigin, Vector<JSC::Strong<JSC::JSObject>>&& transfer)
{
    return postMessage(globalObject, incumbentWindow, message, WindowPostMessageOptions { { WTF::move(transfer) }, WTF::move(targetOrigin) });
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
    return localThis->requestAnimationFrame(WTF::move(callback));
}

ExceptionOr<int> DOMWindow::webkitRequestAnimationFrame(Ref<RequestAnimationFrameCallback>&& callback)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->webkitRequestAnimationFrame(WTF::move(callback));
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
    return localThis->requestIdleCallback(WTF::move(callback), options);
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
    localThis->createImageBitmap(WTF::move(source), WTF::move(options), WTF::move(promise));
    return { };
}

ExceptionOr<void> DOMWindow::createImageBitmap(ImageBitmap::Source&& source, int sx, int sy, int sw, int sh, ImageBitmapOptions&& options, ImageBitmap::Promise&& promise)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    localThis->createImageBitmap(WTF::move(source), sx, sy, sw, sh, WTF::move(options), WTF::move(promise));
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
    return localThis->setTimeout(WTF::move(action), timeout, WTF::move(arguments));
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
    return localThis->setInterval(WTF::move(action), timeout, WTF::move(arguments));
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
    return WindowOrWorkerGlobalScope::structuredClone(lexicalGlobalObject, relevantGlobalObject, value, WTF::move(options));
}

ExceptionOr<String> DOMWindow::btoa(const String& stringToEncode)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return WindowOrWorkerGlobalScope::btoa(stringToEncode);
}

ExceptionOr<String> DOMWindow::atob(const String& stringToEncode)
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return WindowOrWorkerGlobalScope::atob(stringToEncode);
}

#if ENABLE(DECLARATIVE_WEB_PUSH)
ExceptionOr<PushManager&> DOMWindow::pushManager()
{
    auto* localThis = dynamicDowncast<LocalDOMWindow>(*this);
    if (!localThis)
        return Exception { ExceptionCode::SecurityError };
    return localThis->pushManager();
}
#endif

bool DOMWindow::isCurrentlyDisplayedInFrame() const
{
    RefPtr frame = this->frame();
    return frame && frame->window() == this;
}

void DOMWindow::printErrorMessage(const String& message) const
{
    if (message.isEmpty())
        return;

    if (CheckedPtr frameConsole = console())
        frameConsole->addMessage(MessageSource::JS, MessageLevel::Error, message);
}

String DOMWindow::crossDomainAccessErrorMessage(const LocalDOMWindow& activeWindow, IncludeTargetOrigin includeTargetOrigin)
{
    const URL& activeWindowURL = activeWindow.document()->url();
    if (activeWindowURL.isNull())
        return String();

    RefPtr remoteFrame = (m_type == DOMWindowType::Remote) ? dynamicDowncast<RemoteDOMWindow>(*this)->frame() : nullptr;
    RefPtr localDocument = documentIfLocal();
    // We can't figure anything out if we are operating on a RemoteDOMWindow and don't have a remote frame
    if (!localDocument && !remoteFrame)
        return String();
    Ref activeOrigin = protect(activeWindow.document())->securityOrigin();
    const Ref targetOrigin = localDocument ? localDocument->securityOrigin() : remoteFrame->frameDocumentSecurityOriginOrOpaque();
    ASSERT(!activeOrigin->isSameOriginDomain(targetOrigin));

    // FIXME: This message, and other console messages, have extra newlines. Should remove them.
    String message;
    if (includeTargetOrigin == IncludeTargetOrigin::Yes)
        message = makeString("Blocked a frame with origin \""_s, activeOrigin->toString(), "\" from accessing a frame with origin \""_s, targetOrigin->toString(), "\". "_s);
    else
        message = makeString("Blocked a frame with origin \""_s, activeOrigin->toString(), "\" from accessing a cross-origin frame. "_s);

    // Sandbox errors: Use the origin of the frames' location, rather than their actual origin (since we know that at least one will be "null").
    URL activeURL = activeWindow.document()->url();
    RefPtr<const SecurityOrigin> remoteFrameSecurityOrigin = (m_type == DOMWindowType::Remote) ? remoteFrame->frameDocumentSecurityOriginOrOpaque() : RefPtr<const SecurityOrigin>();
    URL targetURL = localDocument ? localDocument->url() : remoteFrameSecurityOrigin->toURL();
    bool localSandboxed = (localDocument && localDocument->isSandboxed(SandboxFlag::Origin));

    if (localSandboxed || activeWindow.document()->isSandboxed(SandboxFlag::Origin)) {
        if (includeTargetOrigin == IncludeTargetOrigin::Yes)
            message = makeString("Blocked a frame at \""_s, SecurityOrigin::create(activeURL).get().toString(), "\" from accessing a frame at \""_s, SecurityOrigin::create(targetURL).get().toString(), "\". "_s);
        else
            message = makeString("Blocked a frame at \""_s, SecurityOrigin::create(activeURL).get().toString(), "\" from accessing a cross-origin frame. "_s);

        if (localSandboxed && activeWindow.document()->isSandboxed(SandboxFlag::Origin))
            return makeString("Sandbox access violation: "_s, message, " Both frames are sandboxed and lack the \"allow-same-origin\" flag."_s);
        if (localSandboxed)
            return makeString("Sandbox access violation: "_s, message, " The frame being accessed is sandboxed and lacks the \"allow-same-origin\" flag."_s);
        return makeString("Sandbox access violation: "_s, message, " The frame requesting access is sandboxed and lacks the \"allow-same-origin\" flag."_s);
    }

    if (includeTargetOrigin == IncludeTargetOrigin::Yes) {
        // Protocol errors: Use the URL's protocol rather than the origin's protocol so that we get a useful message for non-heirarchal URLs like 'data:'.
        if (targetOrigin->protocol() != activeOrigin->protocol())
            return makeString(message, " The frame requesting access has a protocol of \""_s, activeURL.protocol(), "\", the frame being accessed has a protocol of \""_s, targetURL.protocol(), "\". Protocols must match.\n"_s);

        // 'document.domain' errors.
        if (targetOrigin->domainWasSetInDOM() && activeOrigin->domainWasSetInDOM())
            return makeString(message, "The frame requesting access set \"document.domain\" to \""_s, activeOrigin->domain(), "\", the frame being accessed set it to \""_s, targetOrigin->domain(), "\". Both must set \"document.domain\" to the same value to allow access."_s);
        if (activeOrigin->domainWasSetInDOM())
            return makeString(message, "The frame requesting access set \"document.domain\" to \""_s, activeOrigin->domain(), "\", but the frame being accessed did not. Both must set \"document.domain\" to the same value to allow access."_s);
        if (targetOrigin->domainWasSetInDOM())
            return makeString(message, "The frame being accessed set \"document.domain\" to \""_s, targetOrigin->domain(), "\", but the frame requesting access did not. Both must set \"document.domain\" to the same value to allow access."_s);
    }

    // Default.
    return makeString(message, "Protocols, domains, and ports must match."_s);
}

bool DOMWindow::isInsecureScriptAccess(const LocalDOMWindow& activeWindow, const String& urlString)
{
    if (!WTF::protocolIsJavaScript(urlString))
        return false;

    // If this LocalDOMWindow isn't currently active in the Frame, then there's no
    // way we should allow the access.
    // FIXME: Remove this check if we're able to disconnect LocalDOMWindow from
    // Frame on navigation: https://bugs.webkit.org/show_bug.cgi?id=62054
    if (isCurrentlyDisplayedInFrame()) {
        // FIXME: Is there some way to eliminate the need for a separate "activeWindow == this" check?
        if (&activeWindow == this)
            return false;

        // FIXME: The name canAccess seems to be a roundabout way to ask "can execute script".
        // Can we name the SecurityOrigin function better to make this more clear?

        // This check only makes sense with LocalDOMWindows as RemoteDOMWindows necessarily have different origins
        RefPtr localDocument = documentIfLocal();
        if (localDocument && protect(protect(activeWindow.document())->securityOrigin())->isSameOriginDomain(protect(localDocument->securityOrigin())))
            return false;
    }

    activeWindow.printErrorMessage(crossDomainAccessErrorMessage(activeWindow, IncludeTargetOrigin::Yes));
    return true;
}

bool DOMWindow::passesSetLocationSecurityChecks(const LocalDOMWindow& activeWindow, const URL& completedURL, CanNavigateState& navigationState)
{
    ASSERT(navigationState != CanNavigateState::Unchecked);
    if (!isCurrentlyDisplayedInFrame())
        return false;

    RefPtr activeDocument = activeWindow.document();
    if (!activeDocument)
        return false;

    RefPtr frame = this->frame();
    if (navigationState != CanNavigateState::Able) [[unlikely]]
        navigationState = activeDocument->canNavigate(frame.get(), completedURL);
    if (navigationState == CanNavigateState::Unable)
        return false;

    if (isInsecureScriptAccess(activeWindow, completedURL.string()))
        return false;
    return true;
}

} // namespace WebCore
