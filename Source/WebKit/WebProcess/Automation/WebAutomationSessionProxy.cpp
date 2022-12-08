/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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
#include "WebAutomationDOMWindowObserver.h"
#include "WebAutomationSessionMessages.h"
#include "WebAutomationSessionProxyMessages.h"
#include "WebAutomationSessionProxyScriptSource.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebImage.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/JSStringRefPrivate.h>
#include <JavaScriptCore/OpaqueJSString.h>
#include <WebCore/AXObjectCache.h>
#include <WebCore/AccessibilityObject.h>
#include <WebCore/CookieJar.h>
#include <WebCore/DOMRect.h>
#include <WebCore/DOMRectList.h>
#include <WebCore/DOMWindow.h>
#include <WebCore/File.h>
#include <WebCore/FileList.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameTree.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLFrameElement.h>
#include <WebCore/HTMLIFrameElement.h>
#include <WebCore/HTMLInputElement.h>
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

using namespace WebCore;

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
    , m_scriptObjectIdentifier(JSC::PrivateName::Description, "automationSessionProxy"_s)
{
    WebProcess::singleton().addMessageReceiver(Messages::WebAutomationSessionProxy::messageReceiverName(), *this);
}

WebAutomationSessionProxy::~WebAutomationSessionProxy()
{
    m_frameObservers.clear();

    WebProcess::singleton().removeMessageReceiver(Messages::WebAutomationSessionProxy::messageReceiverName());
}

static bool isValidNodeHandle(const String& nodeHandle)
{
    // Node identifier has the following format:
    // node-XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
    // 01234567890123456789012345678901234567890
    // where X is a valid hexadecimal value in upper case.
    if (nodeHandle.length() != 41)
        return false;

    if (nodeHandle[0] != 'n' || nodeHandle[1] != 'o' || nodeHandle[2] != 'd' || nodeHandle[3] != 'e')
        return false;

    for (unsigned i = 4; i < 41; ++i) {
        switch (i) {
        case 4:
        case 13:
        case 18:
        case 23:
        case 28:
            if (nodeHandle[i] != '-')
                return false;
            break;
        default:
            if (!(nodeHandle[i] >= '0' && nodeHandle[i] <= '9') && !(nodeHandle[i] >= 'A' && nodeHandle[i] <= 'F'))
                return false;
            break;
        }
    }

    return true;
}

static JSValueRef isValidNodeIdentifier(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    ASSERT_ARG(argumentCount, argumentCount == 1);
    ASSERT_ARG(arguments, JSValueIsString(context, arguments[0]));

    if (argumentCount != 1)
        return JSValueMakeUndefined(context);

    auto nodeIdentifier = adoptRef(JSValueToStringCopy(context, arguments[0], exception));
    return JSValueMakeBoolean(context, isValidNodeHandle(nodeIdentifier->string()));
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
    return toJSValue(context, createVersion4UUIDString().convertToASCIIUppercase());
}

static JSValueRef evaluateJavaScriptCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    ASSERT_ARG(argumentCount, argumentCount == 4);
    ASSERT_ARG(arguments, JSValueIsNumber(context, arguments[0]));
    ASSERT_ARG(arguments, JSValueIsNumber(context, arguments[1]));
    ASSERT_ARG(arguments, JSValueIsNumber(context, arguments[2]));
    ASSERT_ARG(arguments, JSValueIsObject(context, arguments[3]) || JSValueIsString(context, arguments[3]));

    auto automationSessionProxy = WebProcess::singleton().automationSessionProxy();
    if (!automationSessionProxy)
        return JSValueMakeUndefined(context);

    WebCore::FrameIdentifier frameID {
        makeObjectIdentifier<WebCore::FrameIdentifierType>(JSValueToNumber(context, arguments[0], exception)),
        makeObjectIdentifier<WebCore::ProcessIdentifierType>(JSValueToNumber(context, arguments[1], exception))
    };
    uint64_t callbackID = JSValueToNumber(context, arguments[2], exception);
    if (JSValueIsString(context, arguments[3])) {
        auto result = adoptRef(JSValueToStringCopy(context, arguments[3], exception));
        automationSessionProxy->didEvaluateJavaScriptFunction(frameID, callbackID, result->string(), { });
    } else if (JSValueIsObject(context, arguments[3])) {
        JSObjectRef error = JSValueToObject(context, arguments[3], exception);
        JSValueRef nameValue = JSObjectGetProperty(context, error, OpaqueJSString::tryCreate("name"_s).get(), exception);
        String exceptionName = adoptRef(JSValueToStringCopy(context, nameValue, nullptr))->string();
        String errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::JavaScriptError);
        if (exceptionName == "JavaScriptTimeout"_s)
            errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::JavaScriptTimeout);
        else if (exceptionName == "NodeNotFound"_s)
            errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::NodeNotFound);
        else if (exceptionName == "InvalidNodeIdentifier"_s)
            errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::InvalidNodeIdentifier);
        else if (exceptionName == "InvalidElementState"_s)
            errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::InvalidElementState);
        else if (exceptionName == "InvalidParameter"_s)
            errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::InvalidParameter);
        else if (exceptionName == "InvalidSelector"_s)
            errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::InvalidSelector);
        else if (exceptionName == "ElementNotInteractable"_s)
            errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::ElementNotInteractable);

        JSValueRef messageValue = JSObjectGetProperty(context, error, OpaqueJSString::tryCreate("message"_s).get(), exception);
        auto exceptionMessage = adoptRef(JSValueToStringCopy(context, messageValue, exception))->string();
        automationSessionProxy->didEvaluateJavaScriptFunction(frameID, callbackID, exceptionMessage, errorType);
    } else {
        String errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::InternalError);
        automationSessionProxy->didEvaluateJavaScriptFunction(frameID, callbackID, { }, errorType);
    }

    return JSValueMakeUndefined(context);
}

JSObjectRef WebAutomationSessionProxy::scriptObject(JSGlobalContextRef context)
{
    JSC::JSGlobalObject* globalObject = toJS(context);
    JSC::VM& vm = globalObject->vm();
    JSC::JSLockHolder locker(vm);
    auto scriptObjectID = JSC::Identifier::fromUid(m_scriptObjectIdentifier);
    if (!globalObject->hasProperty(globalObject, scriptObjectID))
        return nullptr;

    return const_cast<JSObjectRef>(toRef(globalObject, globalObject->get(globalObject, scriptObjectID)));
}

void WebAutomationSessionProxy::setScriptObject(JSGlobalContextRef context, JSObjectRef object)
{
    JSC::JSGlobalObject* globalObject = toJS(context);
    JSC::VM& vm = globalObject->vm();
    JSC::JSLockHolder locker(vm);
    auto scriptObjectID = JSC::Identifier::fromUid(m_scriptObjectIdentifier);
    JSC::PutPropertySlot slot(globalObject);
    globalObject->methodTable()->put(globalObject, globalObject, scriptObjectID, toJS(globalObject, object), slot);
}

JSObjectRef WebAutomationSessionProxy::scriptObjectForFrame(WebFrame& frame)
{
    JSGlobalContextRef context = frame.jsContext();
    if (auto* scriptObject = this->scriptObject(context))
        return scriptObject;

    JSValueRef exception = nullptr;
    String script = StringImpl::createWithoutCopying(WebAutomationSessionProxyScriptSource, sizeof(WebAutomationSessionProxyScriptSource));
    JSObjectRef scriptObjectFunction = const_cast<JSObjectRef>(JSEvaluateScript(context, OpaqueJSString::tryCreate(script).get(), nullptr, nullptr, 0, &exception));
    ASSERT(JSValueIsObject(context, scriptObjectFunction));

    JSValueRef sessionIdentifier = toJSValue(context, m_sessionIdentifier);
    JSObjectRef evaluateFunction = JSObjectMakeFunctionWithCallback(context, nullptr, evaluate);
    JSObjectRef createUUIDFunction = JSObjectMakeFunctionWithCallback(context, nullptr, createUUID);
    JSObjectRef isValidNodeIdentifierFunction = JSObjectMakeFunctionWithCallback(context, nullptr, isValidNodeIdentifier);
    JSValueRef arguments[] = { sessionIdentifier, evaluateFunction, createUUIDFunction, isValidNodeIdentifierFunction };
    JSObjectRef scriptObject = const_cast<JSObjectRef>(JSObjectCallAsFunction(context, scriptObjectFunction, nullptr, WTF_ARRAY_LENGTH(arguments), arguments, &exception));
    ASSERT(JSValueIsObject(context, scriptObject));

    setScriptObject(context, scriptObject);
    return scriptObject;
}

WebCore::Element* WebAutomationSessionProxy::elementForNodeHandle(WebFrame& frame, const String& nodeHandle)
{
    // Don't use scriptObjectForFrame() since we can assume if the script object
    // does not exist, there are no nodes mapped to handles. Using scriptObjectForFrame()
    // will make a new script object if it can't find one, preventing us from returning fast.
    JSGlobalContextRef context = frame.jsContext();
    auto* scriptObject = this->scriptObject(context);
    if (!scriptObject)
        return nullptr;

    JSValueRef functionArguments[] = {
        toJSValue(context, nodeHandle)
    };

    JSValueRef result = callPropertyFunction(context, scriptObject, "nodeForIdentifier"_s, WTF_ARRAY_LENGTH(functionArguments), functionArguments, nullptr);
    JSObjectRef element = JSValueToObject(context, result, nullptr);
    if (!element)
        return nullptr;

    auto elementWrapper = JSC::jsDynamicCast<WebCore::JSElement*>(toJS(element));
    if (!elementWrapper)
        return nullptr;

    return &elementWrapper->wrapped();
}

WebCore::AccessibilityObject* WebAutomationSessionProxy::getAccessibilityObjectForNode(WebCore::PageIdentifier pageID, std::optional<WebCore::FrameIdentifier> frameID, String nodeHandle, String& errorType)
{
    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page) {
        errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::WindowNotFound);
        return nullptr;
    }

    auto* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : &page->mainWebFrame();
    if (!frame || !frame->coreFrame() || !frame->coreFrame()->view()) {
        errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::FrameNotFound);
        return nullptr;
    }

    if (!isValidNodeHandle(nodeHandle)) {
        errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::InvalidNodeIdentifier);
        return nullptr;
    }

    WebCore::Element* coreElement = elementForNodeHandle(*frame, nodeHandle);
    if (!coreElement) {
        errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::NodeNotFound);
        return nullptr;
    }

    if (!WebCore::AXObjectCache::accessibilityEnabled())
        WebCore::AXObjectCache::enableAccessibility();

    if (WebCore::AXObjectCache* axObjectCache = coreElement->document().axObjectCache()) {
        if (WebCore::AccessibilityObject* axObject = axObjectCache->getOrCreate(coreElement))
            return axObject;
    }

    errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::InternalError);
    return nullptr;
}

void WebAutomationSessionProxy::ensureObserverForFrame(WebFrame& frame)
{
    // If the frame and DOMWindow have become disconnected, then frame is already being destroyed
    // and there is no way to get access to the frame from the observer's DOMWindow reference.
    if (!frame.coreFrame()->window() || !frame.coreFrame()->window()->frame())
        return;

    if (m_frameObservers.contains(frame.frameID()))
        return;

    auto frameID = frame.frameID();
    m_frameObservers.set(frameID, WebAutomationDOMWindowObserver::create(*frame.coreFrame()->window(), [this, frameID] (WebAutomationDOMWindowObserver&) {
        willDestroyGlobalObjectForFrame(frameID);
    }));
}

void WebAutomationSessionProxy::didClearWindowObjectForFrame(WebFrame& frame)
{
    willDestroyGlobalObjectForFrame(frame.frameID());
}

void WebAutomationSessionProxy::willDestroyGlobalObjectForFrame(WebCore::FrameIdentifier frameID)
{
    // The observer is no longer needed, let it become GC'd and unregister itself from DOMWindow.
    if (m_frameObservers.contains(frameID))
        m_frameObservers.remove(frameID);

    String errorMessage = "Callback was not called before the unload event."_s;
    String errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::FrameNotFound);

    auto pendingFrameCallbacks = m_webFramePendingEvaluateJavaScriptCallbacksMap.take(frameID);
    for (uint64_t callbackID : pendingFrameCallbacks)
        WebProcess::singleton().parentProcessConnection()->send(Messages::WebAutomationSession::DidEvaluateJavaScriptFunction(callbackID, errorMessage, errorType), 0);
}

void WebAutomationSessionProxy::evaluateJavaScriptFunction(WebCore::PageIdentifier pageID, std::optional<WebCore::FrameIdentifier> optionalFrameID, const String& function, Vector<String> arguments, bool expectsImplicitCallbackArgument, std::optional<double> callbackTimeout, uint64_t callbackID)
{
    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page) {
        WebProcess::singleton().parentProcessConnection()->send(Messages::WebAutomationSession::DidEvaluateJavaScriptFunction(callbackID, { },
            Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::WindowNotFound)), 0);
        return;
    }
    auto* frame = optionalFrameID ? WebProcess::singleton().webFrame(*optionalFrameID) : &page->mainWebFrame();
    if (!frame || !frame->coreFrame()->window() || !frame->coreFrame()->window()->frame()) {
        WebProcess::singleton().parentProcessConnection()->send(Messages::WebAutomationSession::DidEvaluateJavaScriptFunction(callbackID, { },
            Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::FrameNotFound)), 0);
        return;
    }

    // No need to track the main frame, this is handled by didClearWindowObjectForFrame.
    if (!frame->coreFrame()->isMainFrame())
        ensureObserverForFrame(*frame);

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
        JSValueMakeNumber(context, frameID.object().toUInt64()),
        JSValueMakeNumber(context, frameID.processIdentifier().toUInt64()),
        JSValueMakeNumber(context, callbackID),
        JSObjectMakeFunctionWithCallback(context, nullptr, evaluateJavaScriptCallback),
        JSValueMakeNumber(context, callbackTimeout.value_or(-1))
    };

    {
        WebCore::UserGestureIndicator gestureIndicator(WebCore::ProcessingUserGesture, frame->coreFrame()->document());
        callPropertyFunction(context, scriptObject, "evaluateJavaScriptFunction"_s, WTF_ARRAY_LENGTH(functionArguments), functionArguments, &exception);
    }

    if (!exception)
        return;

    String errorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::InternalError);

    String exceptionMessage;
    if (JSValueIsObject(context, exception)) {
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

void WebAutomationSessionProxy::resolveChildFrameWithOrdinal(WebCore::PageIdentifier pageID, std::optional<WebCore::FrameIdentifier> frameID, uint32_t ordinal, CompletionHandler<void(std::optional<String>, std::optional<WebCore::FrameIdentifier>)>&& completionHandler)
{
    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page) {
        String windowNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::WindowNotFound);
        completionHandler(windowNotFoundErrorType, std::nullopt);
        return;
    }

    String frameNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::FrameNotFound);

    auto* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : &page->mainWebFrame();
    if (!frame) {
        completionHandler(frameNotFoundErrorType, std::nullopt);
        return;
    }

    WebCore::Frame* coreFrame = frame->coreFrame();
    if (!coreFrame) {
        completionHandler(frameNotFoundErrorType, std::nullopt);
        return;
    }

    auto* coreChildFrame = coreFrame->tree().scopedChild(ordinal);
    if (!coreChildFrame) {
        completionHandler(frameNotFoundErrorType, std::nullopt);
        return;
    }

    WebFrame* childFrame = WebFrame::fromCoreFrame(*coreChildFrame);
    if (!childFrame) {
        completionHandler(frameNotFoundErrorType, std::nullopt);
        return;
    }

    completionHandler(std::nullopt, childFrame->frameID());
}

void WebAutomationSessionProxy::resolveChildFrameWithNodeHandle(WebCore::PageIdentifier pageID, std::optional<WebCore::FrameIdentifier> frameID, const String& nodeHandle, CompletionHandler<void(std::optional<String>, std::optional<WebCore::FrameIdentifier>)>&& completionHandler)
{
    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page) {
        String windowNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::WindowNotFound);
        completionHandler(windowNotFoundErrorType, std::nullopt);
        return;
    }

    String frameNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::FrameNotFound);

    auto* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : &page->mainWebFrame();
    if (!frame) {
        completionHandler(frameNotFoundErrorType, std::nullopt);
        return;
    }

    if (!isValidNodeHandle(nodeHandle)) {
        String invalidNodeIdentifierrrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::InvalidNodeIdentifier);
        completionHandler(invalidNodeIdentifierrrorType, std::nullopt);
        return;
    }

    WebCore::Element* coreElement = elementForNodeHandle(*frame, nodeHandle);
    if (!coreElement) {
        String nodeNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::NodeNotFound);
        completionHandler(nodeNotFoundErrorType, std::nullopt);
        return;
    }

    if (!is<WebCore::HTMLFrameElementBase>(coreElement)) {
        completionHandler(frameNotFoundErrorType, std::nullopt);
        return;
    }

    WebCore::Frame* coreFrameFromElement = downcast<WebCore::HTMLFrameElementBase>(*coreElement).contentFrame();
    if (!coreFrameFromElement) {
        completionHandler(frameNotFoundErrorType, std::nullopt);
        return;
    }

    WebFrame* frameFromElement = WebFrame::fromCoreFrame(*coreFrameFromElement);
    if (!frameFromElement) {
        completionHandler(frameNotFoundErrorType, std::nullopt);
        return;
    }

    completionHandler(std::nullopt, frameFromElement->frameID());
}

void WebAutomationSessionProxy::resolveChildFrameWithName(WebCore::PageIdentifier pageID, std::optional<WebCore::FrameIdentifier> frameID, const String& name, CompletionHandler<void(std::optional<String>, std::optional<WebCore::FrameIdentifier>)>&& completionHandler)
{
    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page) {
        String windowNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::WindowNotFound);
        completionHandler(windowNotFoundErrorType, std::nullopt);
        return;
    }

    String frameNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::FrameNotFound);

    auto* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : &page->mainWebFrame();
    if (!frame) {
        completionHandler(frameNotFoundErrorType, std::nullopt);
        return;
    }

    WebCore::Frame* coreFrame = frame->coreFrame();
    if (!coreFrame) {
        completionHandler(frameNotFoundErrorType, std::nullopt);
        return;
    }

    auto* coreChildFrame = coreFrame->tree().scopedChild(AtomString { name });
    if (!coreChildFrame) {
        completionHandler(frameNotFoundErrorType, std::nullopt);
        return;
    }

    WebFrame* childFrame = WebFrame::fromCoreFrame(*coreChildFrame);
    if (!childFrame) {
        completionHandler(frameNotFoundErrorType, std::nullopt);
        return;
    }

    completionHandler(std::nullopt, childFrame->frameID());
}

void WebAutomationSessionProxy::resolveParentFrame(WebCore::PageIdentifier pageID, std::optional<WebCore::FrameIdentifier> frameID, CompletionHandler<void(std::optional<String>, std::optional<WebCore::FrameIdentifier>)>&& completionHandler)
{
    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page) {
        String windowNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::WindowNotFound);
        completionHandler(windowNotFoundErrorType, std::nullopt);
        return;
    }

    String frameNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::FrameNotFound);

    auto* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : &page->mainWebFrame();
    if (!frame) {
        completionHandler(frameNotFoundErrorType, std::nullopt);
        return;
    }

    WebFrame* parentFrame = frame->parentFrame();
    if (!parentFrame) {
        completionHandler(frameNotFoundErrorType, std::nullopt);
        return;
    }

    completionHandler(std::nullopt, parentFrame->frameID());
}

static WebCore::Element* containerElementForElement(WebCore::Element& element)
{
    // §13. Element State.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-container.
    if (is<WebCore::HTMLOptionElement>(element)) {
#if ENABLE(DATALIST_ELEMENT)
        if (auto* parentElement = WebCore::ancestorsOfType<WebCore::HTMLDataListElement>(element).first())
            return parentElement;
#endif
        if (auto* parentElement = downcast<WebCore::HTMLOptionElement>(element).ownerSelectElement())
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
    if (!frameView->delegatesScrollingToNativeView())
        return frameView->contentsToRootView(frameView->clientToDocumentRect(clientRect));

    // If the frame delegates scrolling, contentsToRootView doesn't take into account scroll/zoom/scale.
    auto& frame = frameView->frame();
    clientRect.scale(frame.pageZoomFactor() * frame.frameScaleFactor());
    clientRect.moveBy(frameView->contentsScrollPosition());
    return clientRect;
}

static WebCore::FloatPoint convertPointFromFrameClientToRootView(WebCore::FrameView* frameView, WebCore::FloatPoint clientPoint)
{
    if (!frameView->delegatesScrollingToNativeView())
        return frameView->contentsToRootView(frameView->clientToDocumentPoint(clientPoint));

    // If the frame delegates scrolling, contentsToRootView doesn't take into account scroll/zoom/scale.
    auto& frame = frameView->frame();
    clientPoint.scale(frame.pageZoomFactor() * frame.frameScaleFactor());
    clientPoint.moveBy(frameView->contentsScrollPosition());
    return clientPoint;
}

void WebAutomationSessionProxy::computeElementLayout(WebCore::PageIdentifier pageID, std::optional<WebCore::FrameIdentifier> frameID, String nodeHandle, bool scrollIntoViewIfNeeded, CoordinateSystem coordinateSystem, CompletionHandler<void(std::optional<String>, WebCore::FloatRect, std::optional<WebCore::IntPoint>, bool)>&& completionHandler)
{
    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page) {
        String windowNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::WindowNotFound);
        completionHandler(windowNotFoundErrorType, { }, std::nullopt, false);
        return;
    }

    auto* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : &page->mainWebFrame();
    if (!frame || !frame->coreFrame() || !frame->coreFrame()->view()) {
        String frameNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::FrameNotFound);
        completionHandler(frameNotFoundErrorType, { }, std::nullopt, false);
        return;
    }

    if (!isValidNodeHandle(nodeHandle)) {
        String invalidNodeIdentifierrrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::InvalidNodeIdentifier);
        completionHandler(invalidNodeIdentifierrrorType, { }, std::nullopt, false);
        return;
    }

    WebCore::Element* coreElement = elementForNodeHandle(*frame, nodeHandle);
    if (!coreElement) {
        String nodeNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::NodeNotFound);
        completionHandler(nodeNotFoundErrorType, { }, std::nullopt, false);
        return;
    }

    auto* containerElement = containerElementForElement(*coreElement);
    if (scrollIntoViewIfNeeded && containerElement) {
        // §14.1 Element Click. Step 4. Scroll into view the element’s container.
        // https://w3c.github.io/webdriver/webdriver-spec.html#element-click
        containerElement->scrollIntoViewIfNotVisible(false);
        // FIXME: Wait in an implementation-specific way up to the session implicit wait timeout for the element to become in view.
    }

    WebCore::FrameView* frameView = frame->coreFrame()->view();
    WebCore::FrameView* mainView = frame->coreFrame()->mainFrame().view();

    WebCore::FloatRect resultElementBounds;
    std::optional<WebCore::IntPoint> resultInViewCenterPoint;
    bool isObscured = false;

    switch (coordinateSystem) {
    case CoordinateSystem::Page:
        resultElementBounds = coreElement->boundingClientRect();
        break;
    case CoordinateSystem::LayoutViewport: {
        auto elementBoundsInRootCoordinates = convertRectFromFrameClientToRootView(frameView, coreElement->boundingClientRect());
        resultElementBounds = mainView->absoluteToLayoutViewportRect(mainView->rootViewToContents(elementBoundsInRootCoordinates));
        break;
    }
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
        completionHandler(std::nullopt, resultElementBounds, resultInViewCenterPoint, isObscured);
        return;
    }

    auto elementInViewCenterPoint = visiblePortionOfElementRect.center();
    auto elementList = containerElement->treeScope().elementsFromPoint(elementInViewCenterPoint);
    auto index = elementList.findIf([containerElement] (auto& item) { return item.get() == containerElement; });
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
    isObscured = elementList[0] != containerElement && !elementList[0]->isDescendantOrShadowDescendantOf(containerElement);

    switch (coordinateSystem) {
    case CoordinateSystem::Page:
        resultInViewCenterPoint = roundedIntPoint(elementInViewCenterPoint);
        break;
    case CoordinateSystem::LayoutViewport: {
        auto inViewCenterPointInRootCoordinates = convertPointFromFrameClientToRootView(frameView, elementInViewCenterPoint);
        resultInViewCenterPoint = roundedIntPoint(mainView->absoluteToLayoutViewportPoint(mainView->rootViewToContents(inViewCenterPointInRootCoordinates)));
        break;
    }
    }

    completionHandler(std::nullopt, resultElementBounds, resultInViewCenterPoint, isObscured);
}

void WebAutomationSessionProxy::getComputedRole(WebCore::PageIdentifier pageID, std::optional<WebCore::FrameIdentifier> frameID, String nodeHandle, CompletionHandler<void(std::optional<String>, std::optional<String>)>&& completionHandler)
{
    String errorType;
    auto* axObject = getAccessibilityObjectForNode(pageID, frameID, nodeHandle, errorType);

    if (!errorType.isNull()) {
        completionHandler(errorType, std::nullopt);
        return;
    }

    completionHandler(std::nullopt, axObject->computedRoleString());
}

void WebAutomationSessionProxy::getComputedLabel(WebCore::PageIdentifier pageID, std::optional<WebCore::FrameIdentifier> frameID, String nodeHandle, CompletionHandler<void(std::optional<String>, std::optional<String>)>&& completionHandler)
{
    String errorType;
    auto* axObject = getAccessibilityObjectForNode(pageID, frameID, nodeHandle, errorType);

    if (!errorType.isNull()) {
        completionHandler(errorType, std::nullopt);
        return;
    }

    completionHandler(std::nullopt, axObject->computedLabel());
}

void WebAutomationSessionProxy::selectOptionElement(WebCore::PageIdentifier pageID, std::optional<WebCore::FrameIdentifier> frameID, String nodeHandle, CompletionHandler<void(std::optional<String>)>&& completionHandler)
{
    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page) {
        String windowNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::WindowNotFound);
        completionHandler(windowNotFoundErrorType);
        return;
    }

    auto* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : &page->mainWebFrame();
    if (!frame || !frame->coreFrame() || !frame->coreFrame()->view()) {
        String frameNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::FrameNotFound);
        completionHandler(frameNotFoundErrorType);
        return;
    }

    if (!isValidNodeHandle(nodeHandle)) {
        String invalidNodeIdentifierErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::InvalidNodeIdentifier);
        completionHandler(invalidNodeIdentifierErrorType);
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
    completionHandler(std::nullopt);
}

void WebAutomationSessionProxy::setFilesForInputFileUpload(WebCore::PageIdentifier pageID, std::optional<WebCore::FrameIdentifier> frameID, String nodeHandle, Vector<String>&& filenames, CompletionHandler<void(std::optional<String>)>&& completionHandler)
{
    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page) {
        String windowNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::WindowNotFound);
        completionHandler(windowNotFoundErrorType);
        return;
    }

    auto* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : &page->mainWebFrame();
    if (!frame || !frame->coreFrame() || !frame->coreFrame()->view()) {
        String frameNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::FrameNotFound);
        completionHandler(frameNotFoundErrorType);
        return;
    }

    WebCore::Element* coreElement = elementForNodeHandle(*frame, nodeHandle);
    if (!coreElement || !is<WebCore::HTMLInputElement>(coreElement) || !downcast<WebCore::HTMLInputElement>(*coreElement).isFileUpload()) {
        String nodeNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::NodeNotFound);
        completionHandler(nodeNotFoundErrorType);
        return;
    }

    auto& inputElement = downcast<WebCore::HTMLInputElement>(*coreElement);
    Vector<Ref<WebCore::File>> fileObjects;
    if (inputElement.multiple()) {
        if (auto* files = inputElement.files())
            fileObjects.appendVector(files->files());
    }
    fileObjects.reserveCapacity(fileObjects.size() + filenames.size());
    for (const auto& path : filenames)
        fileObjects.uncheckedAppend(WebCore::File::create(&inputElement.document(), path));
    inputElement.setFiles(WebCore::FileList::create(WTFMove(fileObjects)));

    completionHandler(std::nullopt);
}

static WebCore::IntRect snapshotElementRectForScreenshot(WebPage& page, WebCore::Element* element, bool clipToViewport)
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

void WebAutomationSessionProxy::takeScreenshot(WebCore::PageIdentifier pageID, std::optional<WebCore::FrameIdentifier> frameID, String nodeHandle, bool scrollIntoViewIfNeeded, bool clipToViewport, uint64_t callbackID)
{
    snapshotRectForScreenshot(pageID, frameID, nodeHandle, scrollIntoViewIfNeeded, clipToViewport, [pageID, frameID, callbackID] (std::optional<String> errorString, WebCore::IntRect&& rect) {
        ShareableBitmapHandle handle;
        if (errorString) {
            WebProcess::singleton().parentProcessConnection()->send(Messages::WebAutomationSession::DidTakeScreenshot(callbackID, handle, *errorString), 0);
            return;
        }

        WebPage* page = WebProcess::singleton().webPage(pageID);
        ASSERT(page);
        auto* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : &page->mainWebFrame();
        ASSERT(frame && frame->coreFrame());
        auto snapshotRect = WebCore::IntRect(frame->coreFrame()->mainFrame().view()->clientToDocumentRect(rect));
        RefPtr<WebImage> image = page->scaledSnapshotWithOptions(snapshotRect, 1, SnapshotOptionsShareable);
        if (!image) {
            String screenshotErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::ScreenshotError);
            WebProcess::singleton().parentProcessConnection()->send(Messages::WebAutomationSession::DidTakeScreenshot(callbackID, handle, screenshotErrorType), 0);
            return;
        }

        handle = image->createHandle(SharedMemory::Protection::ReadOnly);
        WebProcess::singleton().parentProcessConnection()->send(Messages::WebAutomationSession::DidTakeScreenshot(callbackID, handle, { }), 0);
    });
}

void WebAutomationSessionProxy::snapshotRectForScreenshot(WebCore::PageIdentifier pageID, std::optional<WebCore::FrameIdentifier> frameID, String nodeHandle, bool scrollIntoViewIfNeeded, bool clipToViewport, CompletionHandler<void(std::optional<String>, WebCore::IntRect&&)>&& completionHandler)
{
    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page) {
        String windowNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::WindowNotFound);
        completionHandler(windowNotFoundErrorType, { });
        return;
    }

    auto* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : &page->mainWebFrame();
    if (!frame || !frame->coreFrame()) {
        String frameNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::FrameNotFound);
        completionHandler(frameNotFoundErrorType, { });
        return;
    }

    WebCore::Element* coreElement = nullptr;
    if (!nodeHandle.isEmpty()) {
        if (!isValidNodeHandle(nodeHandle)) {
            String invalidNodeIdentifierrrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::InvalidNodeIdentifier);
            completionHandler(invalidNodeIdentifierrrorType, { });
            return;
        }

        coreElement = elementForNodeHandle(*frame, nodeHandle);
        if (!coreElement) {
            String nodeNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::NodeNotFound);
            completionHandler(nodeNotFoundErrorType, { });
            return;
        }
    }

    if (coreElement && scrollIntoViewIfNeeded)
        coreElement->scrollIntoViewIfNotVisible(false);

    String screenshotErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::ScreenshotError);
    WebCore::IntRect snapshotRect = snapshotElementRectForScreenshot(*page, coreElement, clipToViewport);
    if (snapshotRect.isEmpty()) {
        completionHandler(screenshotErrorType, { });
        return;
    }

    completionHandler(std::nullopt, WebCore::IntRect(frame->coreFrame()->mainFrame().view()->documentToClientRect(snapshotRect)));
}

void WebAutomationSessionProxy::getCookiesForFrame(WebCore::PageIdentifier pageID, std::optional<WebCore::FrameIdentifier> frameID, CompletionHandler<void(std::optional<String>, Vector<WebCore::Cookie>)>&& completionHandler)
{
    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page) {
        String windowNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::WindowNotFound);
        completionHandler(windowNotFoundErrorType, Vector<WebCore::Cookie>());
        return;
    }

    auto* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : &page->mainWebFrame();
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

    completionHandler(std::nullopt, foundCookies);
}

void WebAutomationSessionProxy::deleteCookie(WebCore::PageIdentifier pageID, std::optional<WebCore::FrameIdentifier> frameID, String cookieName, CompletionHandler<void(std::optional<String>)>&& completionHandler)
{
    WebPage* page = WebProcess::singleton().webPage(pageID);
    if (!page) {
        String windowNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::WindowNotFound);
        completionHandler(windowNotFoundErrorType);
        return;
    }

    auto* frame = frameID ? WebProcess::singleton().webFrame(*frameID) : &page->mainWebFrame();
    if (!frame || !frame->coreFrame() || !frame->coreFrame()->document()) {
        String frameNotFoundErrorType = Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::FrameNotFound);
        completionHandler(frameNotFoundErrorType);
        return;
    }

    auto& document = *frame->coreFrame()->document();
    page->corePage()->cookieJar().deleteCookie(document, document.cookieURL(), cookieName, [completionHandler = WTFMove(completionHandler)] () mutable {
        completionHandler(std::nullopt);
    });
}

} // namespace WebKit
