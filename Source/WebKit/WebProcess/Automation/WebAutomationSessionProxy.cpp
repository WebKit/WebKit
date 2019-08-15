/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "WebAutomationSessionProxy.h"

#include "AutomationProtocolObjects.h"
#include "CoordinateSystem.h"
#include "WebAutomationSessionMessages.h"
#include "WebAutomationSessionProxyMessages.h"
#include "WebAutomationSessionProxyScriptSource.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebImage.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/JSStringRefPrivate.h>
#include <JavaScriptCore/OpaqueJSString.h>
#include <WebCore/CookieJar.h>
#include <WebCore/DOMRect.h>
#include <WebCore/DOMRectList.h>
#include <WebCore/DOMWindow.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameTree.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLFrameElement.h>
#include <WebCore/HTMLIFrameElement.h>
#include <WebCore/HTMLOptGroupElement.h>
#include <WebCore/HTMLOptionElement.h>
#include <WebCore/HTMLSelectElement.h>
#include <WebCore/JSElement.h>
#include <WebCore/RenderElement.h>
#include <wtf/UUID.h>

#if ENABLE(DATALIST_ELEMENT)
#include <WebCore/HTMLDataListElement.h>
#endif

namespace WebKit {

template <typename T>
static JSObjectRef toJSArray(JSContextRef context, const Vector<T>& data, JSValueRef (*converter)(JSContextRef, const T&), JSValueRef* exception)
{
    ASSERT_ARG(converter, converter);

    if (data.isEmpty())
        return JSObjectMakeArray(context, 0, nullptr, exception);

    Vector<JSValueRef, 8> convertedData;
    convertedData.reserveCapacity(data.size());

    for (auto& originalValue : data) {
        JSValueRef convertedValue = converter(context, originalValue);
        JSValueProtect(context, convertedValue);
        convertedData.uncheckedAppend(convertedValue);
    }

    JSObjectRef array = JSObjectMakeArray(context, convertedData.size(), convertedData.data(), exception);

    for (auto& convertedValue : convertedData)
        JSValueUnprotect(context, convertedValue);

    return array;
}

static inline JSValueRef toJSValue(JSContextRef context, const String& string)
{
    return JSValueMakeString(context, OpaqueJSString::tryCreate(string).get());
}

static inline JSValueRef callPropertyFunction(JSContextRef context, JSObjectRef object, const String& propertyName, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    ASSERT_ARG(object, object);
    ASSERT_ARG(object, JSValueIsObject(context, object));

    JSObjectRef function = const_cast<JSObjectRef>(JSObjectGetProperty(context, object, OpaqueJSString::tryCreate(propertyName).get(), exception));
    ASSERT(JSObjectIsFunction(context, function));

    return JSObjectCallAsFunction(context, function, object, argumentCount, arguments, exception);
}

WebAutomationSessionProxy::WebAutomationSessionProxy(const String& sessionIdentifier)
    : m_sessionIdentifier(sessionIdentifier)
{
    WebProcess::singleton().addMessageReceiver(Messages::WebAutomationSessionProxy::messageReceiverName(), *this);
}

WebAutomationSessionProxy::~WebAutomationSessionProxy()
{
    WebProcess::singleton().removeMessageReceiver(Messages::WebAutomationSessionProxy::messageReceiverName());
}

static JSValueRef evaluate(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    ASSERT_ARG(argumentCount, argumentCount == 1);
    ASSERT_ARG(arguments, JSValueIsString(context, arguments[0]));

    if (argumentCount != 1)
        return JSValueMakeUndefined(context);

    auto script = adoptRef(JSValueToStringCopy(context, arguments[0], exception));
    return JSEvaluateScript(context, script.get(), nullptr, nullptr, 0, exception);
}

static JSValueRef createUUID(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return toJSValue(context, createCanonicalUUIDString().convertToASCIIUppercase());
}

static JSValueRef evaluateJavaScriptCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    ASSERT_ARG(argumentCount, argumentCount == 4);
    ASSERT_ARG(arguments, JSValueIsNumber(context, arguments[0]));
    ASSERT_ARG(arguments, JSValueIsNumber(context, arguments[1]));
    ASSERT_ARG(arguments, JSValueIsString(context, arguments[2]));
    ASSERT_ARG(arguments, JSValueIsBoolean(context, arguments[3]));

    auto automationSessionProxy = WebProcess::singleton().automationSessionProxy();
    if (!automationSessionProxy)
        return JSValueMakeUndefined(context);

    WebCore::FrameIdentifier frameID = WebCore::frameIdentifierFromID(JSValueToNumber(context, arguments[0], exception));
    uint64_t callbackID = JSValueToNumber(context, arguments[1], exception);
    auto result = adoptRef(JSValueToStringCopy(context, arguments[2], exception));

    bool resultIsErrorName = JSValueToBoolean(context, arguments[3]);

    if (resultIsErrorName) {
        if (result->string() == "JavaScriptTimeout") {
            String errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::JavaScriptTimeout);
            automationSessionProxy->didEvaluateJavaScriptFunction(frameID, callbackID, String(), errorType);
        } else {
            ASSERT_NOT_REACHED();
            String errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::InternalError);
            automationSessionProxy->didEvaluateJavaScriptFunction(frameID, callbackID, String(), errorType);
        }
    } else
        automationSessionProxy->didEvaluateJavaScriptFunction(frameID, callbackID, result->string(), String());

    return JSValueMakeUndefined(context);
}

JSObjectRef WebAutomationSessionProxy::scriptObjectForFrame(WebFrame& frame)
{
    if (JSObjectRef scriptObject = m_webFrameScriptObjectMap.get(frame.frameID()))
        return scriptObject;

    JSValueRef exception = nullptr;
    JSGlobalContextRef context = frame.jsContext();

    JSValueRef sessionIdentifier = toJSValue(context, m_sessionIdentifier);
    JSObjectRef evaluateFunction = JSObjectMakeFunctionWithCallback(context, nullptr, evaluate);
    JSObjectRef createUUIDFunction = JSObjectMakeFunctionWithCallback(context, nullptr, createUUID);

    String script = StringImpl::createWithoutCopying(WebAutomationSessionProxyScriptSource, sizeof(WebAutomationSessionProxyScriptSource));

    JSObjectRef scriptObjectFunction = const_cast<JSObjectRef>(JSEvaluateScript(context, OpaqueJSString::tryCreate(script).get(), nullptr, nullptr, 0, &exception));
    ASSERT(JSValueIsObject(context, scriptObjectFunction));

    JSValueRef arguments[] = { sessionIdentifier, evaluateFunction, createUUIDFunction };
    JSObjectRef scriptObject = const_cast<JSObjectRef>(JSObjectCallAsFunction(context, scriptObjectFunction, nullptr, WTF_ARRAY_LENGTH(arguments), arguments, &exception));
    ASSERT(JSValueIsObject(context, scriptObject));

    JSValueProtect(context, scriptObject);
    m_webFrameScriptObjectMap.add(frame.frameID(), scriptObject);

    return scriptObject;
}

WebCore::Element* WebAutomationSessionProxy::elementForNodeHandle(WebFrame& frame, const String& nodeHandle)
{
    // Don't use scriptObjectForFrame() since we can assume if the script object
    // does not exist, there are no nodes mapped to handles. Using scriptObjectForFrame()
    // will make a new script object if it can't find one, preventing us from returning fast.
    JSObjectRef scriptObject = m_webFrameScriptObjectMap.get(frame.frameID());
    if (!scriptObject)
        return nullptr;

    JSGlobalContextRef context = frame.jsContext();

    JSValueRef functionArguments[] = {
        toJSValue(context, nodeHandle)
    };

    JSValueRef result = callPropertyFunction(context, scriptObject, "nodeForIdentifier"_s, WTF_ARRAY_LENGTH(functionArguments), functionArguments, nullptr);
    JSObjectRef element = JSValueToObject(context, result, nullptr);
    if (!element)
        return nullptr;

    auto elementWrapper = JSC::jsDynamicCast<WebCore::JSElement*>(toJS(context)->vm(), toJS(element));
    if (!elementWrapper)
        return nullptr;

    return &elementWrapper->wrapped();
}

void WebAutomationSessionProxy::didClearWindowObjectForFrame(WebFrame& frame)
{
    WebCore::FrameIdentifier frameID = frame.frameID();
    if (JSObjectRef scriptObject = m_webFrameScriptObjectMap.take(frameID))
        JSValueUnprotect(frame.jsContext(), scriptObject);

    String errorMessage = "Callback was not called before the unload event."_s;
    String errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::JavaScriptError);

    auto pendingFrameCallbacks = m_webFramePendingEvaluateJavaScriptCallbacksMap.take(frameID);
    for (uint64_t callbackID : pendingFrameCallbacks)
        WebProcess::singleton().parentProcessConnection()->send(Messages::WebAutomationSession::DidEvaluateJavaScriptFunction(callbackID, errorMessage, errorType), 0);
}

void WebAutomationSessionProxy::evaluateJavaScriptFunction(WebCore::PageIdentifier pageID, Optional<WebCore::FrameIdentifier> optionalFrameID, const String& function, Vector<String> arguments, bool expectsImplicitCallbackArgument, int callbackTimeout, uint64_t callbackID)
{
    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page) {
        WebProcess::singleton().parentProcessConnection()->send(Messages::WebAutomationSession::DidEvaluateJavaScriptFunction(callbackID, { },
            Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::WindowNotFound)), 0);
        return;
    }
    WebFrame* frame = optionalFrameID ? WebProcess::singleton().webFrame(*optionalFrameID) : page->mainWebFrame();
    if (!frame) {
        WebProcess::singleton().parentProcessConnection()->send(Messages::WebAutomationSession::DidEvaluateJavaScriptFunction(callbackID, { },
            Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::FrameNotFound)), 0);
        return;
    }

    JSObjectRef scriptObject = scriptObjectForFrame(*frame);
    ASSERT(scriptObject);

    auto frameID = frame->frameID();
    JSValueRef exception = nullptr;
    JSGlobalContextRef context = frame->jsContext();

    if (expectsImplicitCallbackArgument) {
        auto result = m_webFramePendingEvaluateJavaScriptCallbacksMap.add(frameID, Vector<uint64_t>());
        result.iterator->value.append(callbackID);
    }

    JSValueRef functionArguments[] = {
        toJSValue(context, function),
        toJSArray(context, arguments, toJSValue, &exception),
        JSValueMakeBoolean(context, expectsImplicitCallbackArgument),
        JSValueMakeNumber(context, frameID.toUInt64()),
        JSValueMakeNumber(context, callbackID),
        JSObjectMakeFunctionWithCallback(context, nullptr, evaluateJavaScriptCallback),
        JSValueMakeNumber(context, callbackTimeout)
    };

    {
        WebCore::UserGestureIndicator gestureIndicator(WebCore::ProcessingUserGesture, frame->coreFrame()->document());
        callPropertyFunction(context, scriptObject, "evaluateJavaScriptFunction"_s, WTF_ARRAY_LENGTH(functionArguments), functionArguments, &exception);
    }

    if (!exception)
        return;

    String errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::JavaScriptError);

    String exceptionMessage;
    if (JSValueIsObject(context, exception)) {
        JSValueRef nameValue = JSObjectGetProperty(context, const_cast<JSObjectRef>(exception), OpaqueJSString::tryCreate("name"_s).get(), nullptr);
        auto exceptionName = adoptRef(JSValueToStringCopy(context, nameValue, nullptr))->string();
        if (exceptionName == "NodeNotFound")
            errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::NodeNotFound);
        else if (exceptionName == "InvalidElementState")
            errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::InvalidElementState);
        else if (exceptionName == "InvalidParameter")
            errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::InvalidParameter);
        else if (exceptionName == "InvalidSelector")
            errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::InvalidSelector);

        JSValueRef messageValue = JSObjectGetProperty(context, const_cast<JSObjectRef>(exception), OpaqueJSString::tryCreate("message"_s).get(), nullptr);
        exceptionMessage = adoptRef(JSValueToStringCopy(context, messageValue, nullptr))->string();
    } else
        exceptionMessage = adoptRef(JSValueToStringCopy(context, exception, nullptr))->string();

    didEvaluateJavaScriptFunction(frameID, callbackID, exceptionMessage, errorType);
}

void WebAutomationSessionProxy::didEvaluateJavaScriptFunction(WebCore::FrameIdentifier frameID, uint64_t callbackID, const String& result, const String& errorType)
{
    auto findResult = m_webFramePendingEvaluateJavaScriptCallbacksMap.find(frameID);
    if (findResult != m_webFramePendingEvaluateJavaScriptCallbacksMap.end()) {
        findResult->value.removeFirst(callbackID);
        ASSERT(!findResult->value.contains(callbackID));
        if (findResult->value.isEmpty())
            m_webFramePendingEvaluateJavaScriptCallbacksMap.remove(findResult);
    }

    WebProcess::singleton().parentProcessConnection()->send(Messages::WebAutomationSession::DidEvaluateJavaScriptFunction(callbackID, result, errorType), 0);
}

void WebAutomationSessionProxy::resolveChildFrameWithOrdinal(WebCore::PageIdentifier pageID, Optional<WebCore::FrameIdentifier> frameID, uint32_t ordinal, CompletionHandler<void(Optional<String>, Optional<WebCore::FrameIdentifier>)>&& completionHandler)
{
    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page) {
        String windowNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::WindowNotFound);
        completionHandler(windowNotFoundErrorType, WTF::nullopt);
        return;
    }

    String frameNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::FrameNotFound);

    WebFrame* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : page->mainWebFrame();
    if (!frame) {
        completionHandler(frameNotFoundErrorType, WTF::nullopt);
        return;
    }

    WebCore::Frame* coreFrame = frame->coreFrame();
    if (!coreFrame) {
        completionHandler(frameNotFoundErrorType, WTF::nullopt);
        return;
    }

    WebCore::Frame* coreChildFrame = coreFrame->tree().scopedChild(ordinal);
    if (!coreChildFrame) {
        completionHandler(frameNotFoundErrorType, WTF::nullopt);
        return;
    }

    WebFrame* childFrame = WebFrame::fromCoreFrame(*coreChildFrame);
    if (!childFrame) {
        completionHandler(frameNotFoundErrorType, WTF::nullopt);
        return;
    }

    completionHandler(WTF::nullopt, childFrame->frameID());
}

void WebAutomationSessionProxy::resolveChildFrameWithNodeHandle(WebCore::PageIdentifier pageID, Optional<WebCore::FrameIdentifier> frameID, const String& nodeHandle, CompletionHandler<void(Optional<String>, Optional<WebCore::FrameIdentifier>)>&& completionHandler)
{
    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page) {
        String windowNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::WindowNotFound);
        completionHandler(windowNotFoundErrorType, WTF::nullopt);
        return;
    }

    String frameNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::FrameNotFound);

    WebFrame* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : page->mainWebFrame();
    if (!frame) {
        completionHandler(frameNotFoundErrorType, WTF::nullopt);
        return;
    }

    WebCore::Element* coreElement = elementForNodeHandle(*frame, nodeHandle);
    if (!is<WebCore::HTMLFrameElementBase>(coreElement)) {
        completionHandler(frameNotFoundErrorType, WTF::nullopt);
        return;
    }

    WebCore::Frame* coreFrameFromElement = downcast<WebCore::HTMLFrameElementBase>(*coreElement).contentFrame();
    if (!coreFrameFromElement) {
        completionHandler(frameNotFoundErrorType, WTF::nullopt);
        return;
    }

    WebFrame* frameFromElement = WebFrame::fromCoreFrame(*coreFrameFromElement);
    if (!frameFromElement) {
        completionHandler(frameNotFoundErrorType, WTF::nullopt);
        return;
    }

    completionHandler(WTF::nullopt, frameFromElement->frameID());
}

void WebAutomationSessionProxy::resolveChildFrameWithName(WebCore::PageIdentifier pageID, Optional<WebCore::FrameIdentifier> frameID, const String& name, CompletionHandler<void(Optional<String>, Optional<WebCore::FrameIdentifier>)>&& completionHandler)
{
    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page) {
        String windowNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::WindowNotFound);
        completionHandler(windowNotFoundErrorType, WTF::nullopt);
        return;
    }

    String frameNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::FrameNotFound);

    WebFrame* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : page->mainWebFrame();
    if (!frame) {
        completionHandler(frameNotFoundErrorType, WTF::nullopt);
        return;
    }

    WebCore::Frame* coreFrame = frame->coreFrame();
    if (!coreFrame) {
        completionHandler(frameNotFoundErrorType, WTF::nullopt);
        return;
    }

    WebCore::Frame* coreChildFrame = coreFrame->tree().scopedChild(name);
    if (!coreChildFrame) {
        completionHandler(frameNotFoundErrorType, WTF::nullopt);
        return;
    }

    WebFrame* childFrame = WebFrame::fromCoreFrame(*coreChildFrame);
    if (!childFrame) {
        completionHandler(frameNotFoundErrorType, WTF::nullopt);
        return;
    }

    completionHandler(WTF::nullopt, childFrame->frameID());
}

void WebAutomationSessionProxy::resolveParentFrame(WebCore::PageIdentifier pageID, Optional<WebCore::FrameIdentifier> frameID, CompletionHandler<void(Optional<String>, Optional<WebCore::FrameIdentifier>)>&& completionHandler)
{
    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page) {
        String windowNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::WindowNotFound);
        completionHandler(windowNotFoundErrorType, WTF::nullopt);
        return;
    }

    String frameNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::FrameNotFound);

    WebFrame* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : page->mainWebFrame();
    if (!frame) {
        completionHandler(frameNotFoundErrorType, WTF::nullopt);
        return;
    }

    WebFrame* parentFrame = frame->parentFrame();
    if (!parentFrame) {
        completionHandler(frameNotFoundErrorType, WTF::nullopt);
        return;
    }

    completionHandler(WTF::nullopt, parentFrame->frameID());
}

void WebAutomationSessionProxy::focusFrame(WebCore::PageIdentifier pageID, Optional<WebCore::FrameIdentifier> frameID)
{
    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page)
        return;

    WebFrame* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : page->mainWebFrame();
    if (!frame)
        return;

    WebCore::Frame* coreFrame = frame->coreFrame();
    if (!coreFrame)
        return;

    WebCore::Document* coreDocument = coreFrame->document();
    if (!coreDocument)
        return;

    WebCore::DOMWindow* coreDOMWindow = coreDocument->domWindow();
    if (!coreDOMWindow)
        return;

    coreDOMWindow->focus(true);
}

static WebCore::Element* containerElementForElement(WebCore::Element& element)
{
    // §13. Element State.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-container.
    if (is<WebCore::HTMLOptionElement>(element)) {
        auto& optionElement = downcast<WebCore::HTMLOptionElement>(element);
#if ENABLE(DATALIST_ELEMENT)
        if (auto* parentElement = optionElement.ownerDataListElement())
            return parentElement;
#endif
        if (auto* parentElement = optionElement.ownerSelectElement())
            return parentElement;

        return nullptr;
    }

    if (is<WebCore::HTMLOptGroupElement>(element)) {
        if (auto* parentElement = downcast<WebCore::HTMLOptGroupElement>(element).ownerSelectElement())
            return parentElement;

        return nullptr;
    }

    return &element;
}

static WebCore::FloatRect convertRectFromFrameClientToRootView(WebCore::FrameView* frameView, WebCore::FloatRect clientRect)
{
    if (!frameView->delegatesScrolling())
        return frameView->contentsToRootView(frameView->clientToDocumentRect(clientRect));

    // If the frame delegates scrolling, contentsToRootView doesn't take into account scroll/zoom/scale.
    auto& frame = frameView->frame();
    clientRect.scale(frame.pageZoomFactor() * frame.frameScaleFactor());
    clientRect.moveBy(frameView->contentsScrollPosition());
    return clientRect;
}

static WebCore::FloatPoint convertPointFromFrameClientToRootView(WebCore::FrameView* frameView, WebCore::FloatPoint clientPoint)
{
    if (!frameView->delegatesScrolling())
        return frameView->contentsToRootView(frameView->clientToDocumentPoint(clientPoint));

    // If the frame delegates scrolling, contentsToRootView doesn't take into account scroll/zoom/scale.
    auto& frame = frameView->frame();
    clientPoint.scale(frame.pageZoomFactor() * frame.frameScaleFactor());
    clientPoint.moveBy(frameView->contentsScrollPosition());
    return clientPoint;
}

void WebAutomationSessionProxy::computeElementLayout(WebCore::PageIdentifier pageID, Optional<WebCore::FrameIdentifier> frameID, String nodeHandle, bool scrollIntoViewIfNeeded, CoordinateSystem coordinateSystem, CompletionHandler<void(Optional<String>, WebCore::IntRect, Optional<WebCore::IntPoint>, bool)>&& completionHandler)
{
    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page) {
        String windowNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::WindowNotFound);
        completionHandler(windowNotFoundErrorType, { }, WTF::nullopt, false);
        return;
    }

    WebFrame* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : page->mainWebFrame();
    if (!frame || !frame->coreFrame() || !frame->coreFrame()->view()) {
        String frameNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::FrameNotFound);
        completionHandler(frameNotFoundErrorType, { }, WTF::nullopt, false);
        return;
    }

    WebCore::Element* coreElement = elementForNodeHandle(*frame, nodeHandle);
    if (!coreElement) {
        String nodeNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::NodeNotFound);
        completionHandler(nodeNotFoundErrorType, { }, WTF::nullopt, false);
        return;
    }

    auto* containerElement = containerElementForElement(*coreElement);
    if (scrollIntoViewIfNeeded && containerElement) {
        // §14.1 Element Click. Step 4. Scroll into view the element’s container.
        // https://w3c.github.io/webdriver/webdriver-spec.html#element-click
        containerElement->scrollIntoViewIfNeeded(false);
        // FIXME: Wait in an implementation-specific way up to the session implicit wait timeout for the element to become in view.
    }

    WebCore::FrameView* frameView = frame->coreFrame()->view();
    WebCore::FrameView* mainView = frame->coreFrame()->mainFrame().view();

    WebCore::IntRect resultElementBounds;
    Optional<WebCore::IntPoint> resultInViewCenterPoint;
    bool isObscured = false;

    auto elementBoundsInRootCoordinates = convertRectFromFrameClientToRootView(frameView, coreElement->boundingClientRect());
    switch (coordinateSystem) {
    case CoordinateSystem::Page:
        resultElementBounds = enclosingIntRect(mainView->absoluteToDocumentRect(mainView->rootViewToContents(elementBoundsInRootCoordinates)));
        break;
    case CoordinateSystem::LayoutViewport:
        resultElementBounds = enclosingIntRect(mainView->absoluteToLayoutViewportRect(elementBoundsInRootCoordinates));
        break;
    }

    // If an <option> or <optgroup> does not have an associated <select> or <datalist> element, then give up.
    if (!containerElement) {
        String elementNotInteractableErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::ElementNotInteractable);
        completionHandler(elementNotInteractableErrorType, resultElementBounds, resultInViewCenterPoint, isObscured);
        return;
    }

    // §12.1 Element Interactability.
    // https://www.w3.org/TR/webdriver/#dfn-in-view-center-point
    auto* firstElementRect = containerElement->getClientRects()->item(0);
    if (!firstElementRect) {
        String elementNotInteractableErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::ElementNotInteractable);
        completionHandler(elementNotInteractableErrorType, resultElementBounds, resultInViewCenterPoint, isObscured);
        return;
    }

    // The W3C WebDriver specification does not explicitly intersect the element with the visual viewport.
    // Do that here so that the IVCP for an element larger than the viewport is within the viewport.
    // See spec bug here: https://github.com/w3c/webdriver/issues/1402
    auto viewportRect = frameView->documentToClientRect(frameView->visualViewportRect());
    auto elementRect = WebCore::FloatRect(firstElementRect->x(), firstElementRect->y(), firstElementRect->width(), firstElementRect->height());
    auto visiblePortionOfElementRect = intersection(viewportRect, elementRect);

    // If the element is entirely outside the viewport, still calculate it's bounds.
    if (visiblePortionOfElementRect.isEmpty()) {
        completionHandler(WTF::nullopt, resultElementBounds, resultInViewCenterPoint, isObscured);
        return;
    }

    auto elementInViewCenterPoint = visiblePortionOfElementRect.center();
    auto elementList = containerElement->treeScope().elementsFromPoint(elementInViewCenterPoint);
    auto index = elementList.findMatching([containerElement] (auto& item) { return item.get() == containerElement; });
    if (elementList.isEmpty() || index == notFound) {
        // We hit this case if the element is visibility:hidden or opacity:0, in which case it will not hit test
        // at the calculated IVCP. An element is technically not "in view" if it is not within its own paint/hit test tree,
        // so it cannot have an in-view center point either. And without an IVCP, the definition of 'obscured' makes no sense.
        // See <https://w3c.github.io/webdriver/webdriver-spec.html#dfn-in-view>.
        String elementNotInteractableErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::ElementNotInteractable);
        completionHandler(elementNotInteractableErrorType, resultElementBounds, resultInViewCenterPoint, isObscured);
        return;
    }

    // Check the case where a non-descendant element hit tests before the target element. For example, a child <option>
    // of a <select> does not obscure the <select>, but two sibling <div> that overlap at the IVCP will obscure each other.
    // Node::isDescendantOf() is not self-inclusive, so that is explicitly checked here.
    isObscured = elementList[0] != containerElement && !elementList[0]->isDescendantOf(containerElement);

    auto inViewCenterPointInRootCoordinates = convertPointFromFrameClientToRootView(frameView, elementInViewCenterPoint);
    switch (coordinateSystem) {
    case CoordinateSystem::Page:
        resultInViewCenterPoint = roundedIntPoint(mainView->absoluteToDocumentPoint(inViewCenterPointInRootCoordinates));
        break;
    case CoordinateSystem::LayoutViewport:
        resultInViewCenterPoint = roundedIntPoint(mainView->absoluteToLayoutViewportPoint(inViewCenterPointInRootCoordinates));
        break;
    }

    completionHandler(WTF::nullopt, resultElementBounds, resultInViewCenterPoint, isObscured);
}

void WebAutomationSessionProxy::selectOptionElement(WebCore::PageIdentifier pageID, Optional<WebCore::FrameIdentifier> frameID, String nodeHandle, CompletionHandler<void(Optional<String>)>&& completionHandler)
{
    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page) {
        String windowNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::WindowNotFound);
        completionHandler(windowNotFoundErrorType);
        return;
    }

    WebFrame* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : page->mainWebFrame();
    if (!frame || !frame->coreFrame() || !frame->coreFrame()->view()) {
        String frameNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::FrameNotFound);
        completionHandler(frameNotFoundErrorType);
        return;
    }

    WebCore::Element* coreElement = elementForNodeHandle(*frame, nodeHandle);
    if (!coreElement || (!is<WebCore::HTMLOptionElement>(coreElement) && !is<WebCore::HTMLOptGroupElement>(coreElement))) {
        String nodeNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::NodeNotFound);
        completionHandler(nodeNotFoundErrorType);
        return;
    }

    String elementNotInteractableErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::ElementNotInteractable);
    if (is<WebCore::HTMLOptGroupElement>(coreElement)) {
        completionHandler(elementNotInteractableErrorType);
        return;
    }

    auto& optionElement = downcast<WebCore::HTMLOptionElement>(*coreElement);
    auto* selectElement = optionElement.ownerSelectElement();
    if (!selectElement) {
        completionHandler(elementNotInteractableErrorType);
        return;
    }

    if (!selectElement->isDisabledFormControl() && !optionElement.isDisabledFormControl()) {
        // FIXME: According to the spec we should fire mouse over, move and down events, then input and change, and finally mouse up and click.
        // optionSelectedByUser() will fire input and change events if needed, but all other events should be fired manually here.
        selectElement->optionSelectedByUser(optionElement.index(), true, selectElement->multiple());
    }
    completionHandler(WTF::nullopt);
}

static WebCore::IntRect snapshotRectForScreenshot(WebPage& page, WebCore::Element* element, bool clipToViewport)
{
    auto* frameView = page.mainFrameView();
    if (!frameView)
        return { };

    if (element) {
        if (!element->renderer())
            return { };

        WebCore::LayoutRect topLevelRect;
        WebCore::IntRect elementRect = WebCore::snappedIntRect(element->renderer()->paintingRootRect(topLevelRect));
        if (clipToViewport)
            elementRect.intersect(frameView->visibleContentRect());

        return elementRect;
    }

    if (auto* frameView = page.mainFrameView())
        return clipToViewport ? frameView->visibleContentRect() : WebCore::IntRect(WebCore::IntPoint(0, 0), frameView->contentsSize());

    return { };
}

void WebAutomationSessionProxy::takeScreenshot(WebCore::PageIdentifier pageID, Optional<WebCore::FrameIdentifier> frameID, String nodeHandle, bool scrollIntoViewIfNeeded, bool clipToViewport, uint64_t callbackID)
{
    ShareableBitmap::Handle handle;

    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page) {
        String windowNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::WindowNotFound);
        WebProcess::singleton().parentProcessConnection()->send(Messages::WebAutomationSession::DidTakeScreenshot(callbackID, handle, windowNotFoundErrorType), 0);
        return;
    }

    WebFrame* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : page->mainWebFrame();
    if (!frame || !frame->coreFrame()) {
        String frameNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::FrameNotFound);
        WebProcess::singleton().parentProcessConnection()->send(Messages::WebAutomationSession::DidTakeScreenshot(callbackID, handle, frameNotFoundErrorType), 0);
        return;
    }

    WebCore::Element* coreElement = nullptr;
    if (!nodeHandle.isEmpty()) {
        coreElement = elementForNodeHandle(*frame, nodeHandle);
        if (!coreElement) {
            String nodeNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::NodeNotFound);
            WebProcess::singleton().parentProcessConnection()->send(Messages::WebAutomationSession::DidTakeScreenshot(callbackID, handle, nodeNotFoundErrorType), 0);
            return;
        }
    }

    if (coreElement && scrollIntoViewIfNeeded)
        coreElement->scrollIntoViewIfNeeded(false);

    String screenshotErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::ScreenshotError);
    WebCore::IntRect snapshotRect = snapshotRectForScreenshot(*page, coreElement, clipToViewport);
    if (snapshotRect.isEmpty()) {
        WebProcess::singleton().parentProcessConnection()->send(Messages::WebAutomationSession::DidTakeScreenshot(callbackID, handle, screenshotErrorType), 0);
        return;
    }

    RefPtr<WebImage> image = page->scaledSnapshotWithOptions(snapshotRect, 1, SnapshotOptionsShareable);
    if (!image) {
        WebProcess::singleton().parentProcessConnection()->send(Messages::WebAutomationSession::DidTakeScreenshot(callbackID, handle, screenshotErrorType), 0);
        return;
    }

    image->bitmap().createHandle(handle, SharedMemory::Protection::ReadOnly);
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebAutomationSession::DidTakeScreenshot(callbackID, handle, { }), 0);
}

void WebAutomationSessionProxy::getCookiesForFrame(WebCore::PageIdentifier pageID, Optional<WebCore::FrameIdentifier> frameID, CompletionHandler<void(Optional<String>, Vector<WebCore::Cookie>)>&& completionHandler)
{
    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page) {
        String windowNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::WindowNotFound);
        completionHandler(windowNotFoundErrorType, Vector<WebCore::Cookie>());
        return;
    }

    WebFrame* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : page->mainWebFrame();
    if (!frame || !frame->coreFrame() || !frame->coreFrame()->document()) {
        String frameNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::FrameNotFound);
        completionHandler(frameNotFoundErrorType, Vector<WebCore::Cookie>());
        return;
    }

    // This returns the same list of cookies as when evaluating `document.cookies` in JavaScript.
    auto& document = *frame->coreFrame()->document();
    Vector<WebCore::Cookie> foundCookies;
    if (!document.cookieURL().isEmpty())
        page->corePage()->cookieJar().getRawCookies(document, document.cookieURL(), foundCookies);

    completionHandler(WTF::nullopt, foundCookies);
}

void WebAutomationSessionProxy::deleteCookie(WebCore::PageIdentifier pageID, Optional<WebCore::FrameIdentifier> frameID, String cookieName, CompletionHandler<void(Optional<String>)>&& completionHandler)
{
    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page) {
        String windowNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::WindowNotFound);
        completionHandler(windowNotFoundErrorType);
        return;
    }

    WebFrame* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : page->mainWebFrame();
    if (!frame || !frame->coreFrame() || !frame->coreFrame()->document()) {
        String frameNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::FrameNotFound);
        completionHandler(frameNotFoundErrorType);
        return;
    }

    auto& document = *frame->coreFrame()->document();
    page->corePage()->cookieJar().deleteCookie(document, document.cookieURL(), cookieName);

    completionHandler(WTF::nullopt);
}

} // namespace WebKit
