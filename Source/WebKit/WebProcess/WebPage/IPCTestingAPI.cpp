/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "IPCTestingAPI.h"

#if ENABLE(IPC_TESTING_API)

#include "Encoder.h"
#include "FrameInfoData.h"
#include "GPUProcessConnection.h"
#include "MessageArgumentDescriptions.h"
#include "NetworkProcessConnection.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/BuiltinNames.h>
#include <JavaScriptCore/JSBigInt.h>
#include <JavaScriptCore/JSClassRef.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/JSValueRef.h>
#include <JavaScriptCore/JavaScript.h>
#include <JavaScriptCore/OpaqueJSString.h>
#include <WebCore/DOMWrapperWorld.h>
#include <WebCore/Frame.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ScriptController.h>

namespace WebKit {

namespace IPCTestingAPI {

class JSIPC;

class JSMessageListener final : public IPC::Connection::MessageObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Type { Incoming, Outgoing };

    JSMessageListener(JSIPC&, Type, JSContextRef, JSObjectRef callback);

private:
    void willSendMessage(const IPC::Encoder&, OptionSet<IPC::SendOption>) override;
    void didReceiveMessage(const IPC::Decoder&) override;
    JSC::JSObject* jsDescriptionFromDecoder(JSC::JSGlobalObject*, IPC::Decoder&);

    WeakPtr<JSIPC> m_jsIPC;
    Type m_type;
    JSContextRef m_context;
    JSObjectRef m_callback;
};

class JSIPC : public RefCounted<JSIPC>, public CanMakeWeakPtr<JSIPC> {
public:
    static Ref<JSIPC> create(WebPage& webPage, WebFrame& webFrame)
    {
        return adoptRef(*new JSIPC(webPage, webFrame));
    }

    static JSClassRef wrapperClass();

    WebFrame* webFrame() { return m_webFrame.get(); }

private:
    JSIPC(WebPage& webPage, WebFrame& webFrame)
        : m_webPage(makeWeakPtr(webPage))
        , m_webFrame(makeWeakPtr(webFrame))
    { }

    static JSIPC* unwrap(JSObjectRef);
    static JSIPC* toWrapped(JSContextRef, JSValueRef);
    static void initialize(JSContextRef, JSObjectRef);
    static void finalize(JSObjectRef);
    static const JSStaticFunction* staticFunctions();
    static const JSStaticValue* staticValues();

    static void addMessageListener(JSMessageListener::Type, JSContextRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef addIncomingMessageListener(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef addOutgoingMessageListener(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

    static JSValueRef sendMessage(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef sendSyncMessage(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

    static JSValueRef visitedLinkStoreID(JSContextRef, JSObjectRef, JSStringRef, JSValueRef* exception);
    static JSValueRef webPageProxyID(JSContextRef, JSObjectRef, JSStringRef, JSValueRef* exception);
    static JSValueRef sessionID(JSContextRef, JSObjectRef, JSStringRef, JSValueRef* exception);
    static JSValueRef pageID(JSContextRef, JSObjectRef, JSStringRef, JSValueRef* exception);
    static JSValueRef frameID(JSContextRef, JSObjectRef, JSStringRef, JSValueRef* exception);
    static JSValueRef retrieveID(JSContextRef, JSObjectRef thisObject, JSValueRef* exception, const WTF::Function<uint64_t(JSIPC&)>&);

    static JSValueRef messages(JSContextRef, JSObjectRef, JSStringRef, JSValueRef* exception);

    WeakPtr<WebPage> m_webPage;
    WeakPtr<WebFrame> m_webFrame;
    Vector<UniqueRef<JSMessageListener>> m_messageListeners;
};

JSClassRef JSIPC::wrapperClass()
{
    static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "IPC";
        definition.parentClass = 0;
        definition.staticValues = staticValues();
        definition.staticFunctions = staticFunctions();
        definition.initialize = initialize;
        definition.finalize = finalize;
        jsClass = JSClassCreate(&definition);
    }
    return jsClass;
}

inline JSIPC* JSIPC::unwrap(JSObjectRef object)
{
    return static_cast<JSIPC*>(JSObjectGetPrivate(object));
}

JSIPC* JSIPC::toWrapped(JSContextRef context, JSValueRef value)
{
    if (!context || !value || !JSValueIsObjectOfClass(context, value, wrapperClass()))
        return nullptr;
    return unwrap(JSValueToObject(context, value, 0));
}

void JSIPC::initialize(JSContextRef, JSObjectRef object)
{
    unwrap(object)->ref();
}

void JSIPC::finalize(JSObjectRef object)
{
    unwrap(object)->deref();
}

const JSStaticFunction* JSIPC::staticFunctions()
{
    static const JSStaticFunction functions[] = {
        { "addIncomingMessageListener", addIncomingMessageListener, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "addOutgoingMessageListener", addOutgoingMessageListener, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "sendMessage", sendMessage, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "sendSyncMessage", sendSyncMessage, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { 0, 0, 0 }
    };
    return functions;
}

const JSStaticValue* JSIPC::staticValues()
{
    static const JSStaticValue values[] = {
        { "visitedLinkStoreID", visitedLinkStoreID, 0, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "frameID", frameID, 0, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "pageID", pageID, 0, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "sessionID", sessionID, 0, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "webPageProxyID", webPageProxyID, 0, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "messages", messages, 0, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { 0, 0, 0, 0 }
    };
    return values;
}

static Optional<uint64_t> convertToUint64(JSC::JSValue jsValue)
{
    if (jsValue.isNumber()) {
        double value = jsValue.asNumber();
        if (value < 0 || trunc(value) != value)
            return WTF::nullopt;
        return value;
    }
    if (jsValue.isBigInt())
        return JSC::JSBigInt::toBigUInt64(jsValue);
    return WTF::nullopt;
}

static JSValueRef createTypeError(JSContextRef context, const String& message)
{
    JSC::JSLockHolder lock(toJS(context)->vm());
    return toRef(JSC::createTypeError(toJS(context), message));
}

static RefPtr<IPC::Connection> processTargetFromArgument(JSC::JSGlobalObject* globalObject, JSValueRef valueRef, JSValueRef* exception)
{
    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
    auto name = toJS(globalObject, valueRef).toWTFString(globalObject);
    if (scope.exception())
        return nullptr;

    if (name == "UI")
        return WebProcess::singleton().parentProcessConnection();
#if ENABLE(GPU_PROCESS)
    if (name == "GPU")
        return &WebProcess::singleton().ensureGPUProcessConnection().connection();
#endif
    if (name == "Networking")
        return &WebProcess::singleton().ensureNetworkProcessConnection().connection();

    *exception = toRef(JSC::createTypeError(globalObject, "Target process must be UI, GPU, or Networking"_s));
    return nullptr;
}

void JSIPC::addMessageListener(JSMessageListener::Type type, JSContextRef context, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    auto jsIPC = makeRefPtr(toWrapped(context, thisObject));
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return;
    }

    if (argumentCount < 1) {
        *exception = createTypeError(context, "Must specify the target process as the first argument"_s);
        return;
    }

    auto connection = processTargetFromArgument(globalObject, arguments[0], exception);
    if (!connection)
        return;

    std::unique_ptr<JSMessageListener> listener;
    if (argumentCount >= 2 && JSValueIsObject(context, arguments[1])) {
        auto listenerObjectRef = JSValueToObject(context, arguments[1], exception);
        if (JSObjectIsFunction(context, listenerObjectRef))
            listener = makeUnique<JSMessageListener>(*jsIPC, type, context, listenerObjectRef);
    }

    if (!listener) {
        *exception = createTypeError(context, "Must specify a callback function as the second argument"_s);
        return;
    }

    connection->addMessageObserver(*listener);
    jsIPC->m_messageListeners.append(makeUniqueRefFromNonNullUniquePtr(WTFMove(listener)));
}

JSValueRef JSIPC::addIncomingMessageListener(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    addMessageListener(JSMessageListener::Type::Incoming, context, thisObject, argumentCount, arguments, exception);
    return JSValueMakeUndefined(context);
}

JSValueRef JSIPC::addOutgoingMessageListener(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    addMessageListener(JSMessageListener::Type::Outgoing, context, thisObject, argumentCount, arguments, exception);
    return JSValueMakeUndefined(context);
}

static Optional<uint64_t> destinationIDFromArgument(JSC::JSGlobalObject* globalObject, JSValueRef valueRef, JSValueRef* exception)
{
    auto jsValue = toJS(globalObject, valueRef);
    auto result = convertToUint64(jsValue);
    if (!result)
        *exception = toRef(JSC::createTypeError(globalObject, "destinationID must be an integer"_s));
    return result;
}

static Optional<uint64_t> messageIDFromArgument(JSC::JSGlobalObject* globalObject, JSValueRef valueRef, JSValueRef* exception)
{
    auto jsValue = toJS(globalObject, valueRef);
    auto result = convertToUint64(jsValue);
    if (!result)
        *exception = toRef(JSC::createTypeError(globalObject, "messageID must be an integer"_s));
    return result;
}

static bool encodeTypedArray(IPC::Encoder& encoder, JSContextRef context, JSValueRef valueRef, JSTypedArrayType type, JSValueRef* exception)
{
    ASSERT(type != kJSTypedArrayTypeNone);

    auto objectRef = JSValueToObject(context, valueRef, exception);
    if (!objectRef)
        return false;

    void* buffer;
    if (type == kJSTypedArrayTypeArrayBuffer)
        buffer = JSObjectGetArrayBufferBytesPtr(context, objectRef, exception);
    else
        buffer = JSObjectGetTypedArrayBytesPtr(context, objectRef, exception);
    if (!buffer)
        return false;

    size_t bufferSize;
    if (type == kJSTypedArrayTypeArrayBuffer)
        bufferSize = JSObjectGetArrayBufferByteLength(context, objectRef, exception);
    else
        bufferSize = JSObjectGetTypedArrayByteLength(context, objectRef, exception);
    if (!bufferSize)
        return false;

    encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(buffer), bufferSize, 1);
    return true;
}

template<typename PointType> bool encodePointType(IPC::Encoder& encoder, JSC::JSGlobalObject* globalObject, JSC::JSObject* jsObject, JSC::CatchScope& scope)
{
    auto& vm = globalObject->vm();
    auto jsX = jsObject->get(globalObject, JSC::Identifier::fromString(vm, "x"_s));
    if (scope.exception() || !jsX.isNumber())
        return false;
    auto jsY = jsObject->get(globalObject, JSC::Identifier::fromString(vm, "y"_s));
    if (scope.exception() || !jsY.isNumber())
        return false;
    encoder << PointType(jsX.asNumber(), jsY.asNumber());
    return true;
}

template<typename RectType> bool encodeRectType(IPC::Encoder& encoder, JSC::JSGlobalObject* globalObject, JSC::JSObject* jsObject, JSC::CatchScope& scope)
{
    auto& vm = globalObject->vm();
    auto jsX = jsObject->get(globalObject, JSC::Identifier::fromString(vm, "x"_s));
    if (scope.exception() || !jsX.isNumber())
        return false;
    auto jsY = jsObject->get(globalObject, JSC::Identifier::fromString(vm, "y"_s));
    if (scope.exception() || !jsY.isNumber())
        return false;
    auto jsWidth = jsObject->get(globalObject, JSC::Identifier::fromString(vm, "width"_s));
    if (scope.exception() || !jsWidth.isNumber())
        return false;
    auto jsHeight = jsObject->get(globalObject, JSC::Identifier::fromString(vm, "height"_s));
    if (scope.exception() || !jsHeight.isNumber())
        return false;
    encoder << RectType(jsX.asNumber(), jsY.asNumber(), jsWidth.asNumber(), jsHeight.asNumber());
    return true;
}

template<typename IntegralType> bool encodeNumericType(IPC::Encoder& encoder, JSC::JSValue jsValue)
{
    if (jsValue.isBigInt()) {
        // FIXME: Support negative BigInt.
        uint64_t result = JSC::JSBigInt::toBigUInt64(jsValue);
        encoder << static_cast<IntegralType>(result);
        return true;
    }
    if (!jsValue.isNumber())
        return false;
    double value = jsValue.asNumber();
    encoder << static_cast<IntegralType>(value);
    return true;
}

static bool encodeArgument(IPC::Encoder&, JSIPC&, JSContextRef, JSValueRef, JSValueRef* exception);

struct VectorEncodeHelper {
    Ref<JSIPC> jsIPC;
    JSContextRef context;
    JSValueRef valueRef;
    JSValueRef* exception;
    bool& success;

    void encode(IPC::Encoder& encoder) const
    {
        if (!success)
            return;
        success = encodeArgument(encoder, jsIPC.get(), context, valueRef, exception);
    }
};

enum class ArrayMode { Tuple, Vector };
static bool encodeArrayArgument(IPC::Encoder& encoder, JSIPC& jsIPC, ArrayMode arrayMode, JSContextRef context, JSValueRef valueRef, JSValueRef* exception)
{
    auto objectRef = JSValueToObject(context, valueRef, exception);
    ASSERT(objectRef);

    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
    auto* jsObject = toJS(objectRef);

    auto jsLength = jsObject->get(globalObject, JSC::Identifier::fromString(vm, "length"_s));
    if (scope.exception())
        return false;
    if (!jsLength.isNumber()) {
        *exception = createTypeError(context, "Array length is not a number"_s);
        return false;
    }
    auto length = jsLength.asNumber();

    Vector<VectorEncodeHelper> vector;
    bool success = true;
    for (unsigned i = 0; i < length; ++i) {
        auto itemRef = JSObjectGetPropertyAtIndex(context, objectRef, i, exception);
        if (!itemRef)
            return false;
        vector.append(VectorEncodeHelper { jsIPC, context, itemRef, exception, success });
    }
    if (arrayMode == ArrayMode::Tuple) {
        for (auto& item : vector)
            item.encode(encoder);
    } else
        encoder << vector;
    return success;
}

static bool encodeArgument(IPC::Encoder& encoder, JSIPC& jsIPC, JSContextRef context, JSValueRef valueRef, JSValueRef* exception)
{
    auto objectRef = JSValueToObject(context, valueRef, exception);
    if (!objectRef)
        return false;

    if (auto type = JSValueGetTypedArrayType(context, objectRef, exception); type != kJSTypedArrayTypeNone)
        return encodeTypedArray(encoder, context, objectRef, type, exception);

    if (JSValueIsArray(context, objectRef))
        return encodeArrayArgument(encoder, jsIPC, ArrayMode::Tuple, context, objectRef, exception);

    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
    auto* jsObject = toJS(globalObject, objectRef).getObject();
    ASSERT(jsObject);
    auto jsType = jsObject->get(globalObject, JSC::Identifier::fromString(vm, "type"_s));
    if (scope.exception())
        return false;

    auto type = jsType.toWTFString(globalObject);
    if (scope.exception())
        return false;

    if (type == "IntPoint") {
        if (!encodePointType<WebCore::IntPoint>(encoder, globalObject, jsObject, scope)) {
            *exception = createTypeError(context, "Failed to convert IntPoint"_s);
            return false;
        }
        return true;
    }

    if (type == "FloatPoint") {
        if (!encodePointType<WebCore::IntPoint>(encoder, globalObject, jsObject, scope)) {
            *exception = createTypeError(context, "Failed to convert FloatPoint"_s);
            return false;
        }
        return true;
    }

    if (type == "IntRect") {
        if (!encodeRectType<WebCore::IntRect>(encoder, globalObject, jsObject, scope)) {
            *exception = createTypeError(context, "Failed to convert IntRect"_s);
            return false;
        }
        return true;
    }

    if (type == "FloatRect") {
        if (!encodeRectType<WebCore::FloatRect>(encoder, globalObject, jsObject, scope)) {
            *exception = createTypeError(context, "Failed to convert FloatRect"_s);
            return false;
        }
        return true;
    }

    if (type == "FrameInfoData") {
        auto webFrame = makeRefPtr(jsIPC.webFrame());
        if (!webFrame) {
            *exception = createTypeError(context, "Failed to get the frame"_s);
            return false;
        }
        encoder << webFrame->info();
        return true;
    }

    auto jsValue = jsObject->get(globalObject, JSC::Identifier::fromString(vm, "value"_s));
    if (scope.exception())
        return false;

    if (type == "Vector") {
        if (!jsValue.isObject() || !jsValue.inherits<JSC::JSArray>(vm)) {
            *exception = createTypeError(context, "Vector value must be an array"_s);
            return false;
        }
        return encodeArrayArgument(encoder, jsIPC, ArrayMode::Vector, context, toRef(globalObject, jsValue), exception);
    }

    if (type == "String") {
        if (jsValue.isUndefinedOrNull()) {
            encoder << String { };
            return true;
        }
        auto string = jsValue.toWTFString(globalObject);
        if (scope.exception())
            return false;
        encoder << string;
        return true;
    }

    if (type == "URL") {
        if (jsValue.isUndefinedOrNull()) {
            encoder << URL { };
            return true;
        }
        auto string = jsValue.toWTFString(globalObject);
        if (scope.exception())
            return false;
        encoder << URL { URL { }, string };
        return true;
    }

    if (type == "RegistrableDomain") {
        if (jsValue.isUndefinedOrNull()) {
            encoder << WebCore::RegistrableDomain { };
            return true;
        }
        auto string = jsValue.toWTFString(globalObject);
        if (scope.exception())
            return false;
        encoder << WebCore::RegistrableDomain { URL { URL { }, string } };
        return true;
    }

    if (type == "RGBA") {
        if (!jsValue.isNumber()) {
            *exception = createTypeError(context, "RGBA value should be a number"_s);
            return false;
        }
        uint32_t rgba = jsValue.asNumber();
        encoder << WebCore::Color { WebCore::asSRGBA(WebCore::PackedColor::RGBA(rgba)) };
        return true;
    }

    bool numericResult;
    if (type == "bool")
        numericResult = encodeNumericType<bool>(encoder, jsValue);
    else if (type == "double")
        numericResult = encodeNumericType<double>(encoder, jsValue);
    else if (type == "float")
        numericResult = encodeNumericType<float>(encoder, jsValue);
    else if (type == "int8_t")
        numericResult = encodeNumericType<int8_t>(encoder, jsValue);
    else if (type == "int16_t")
        numericResult = encodeNumericType<int16_t>(encoder, jsValue);
    else if (type == "int32_t")
        numericResult = encodeNumericType<int32_t>(encoder, jsValue);
    else if (type == "int64_t")
        numericResult = encodeNumericType<int64_t>(encoder, jsValue);
    else if (type == "uint8_t")
        numericResult = encodeNumericType<uint8_t>(encoder, jsValue);
    else if (type == "uint16_t")
        numericResult = encodeNumericType<uint16_t>(encoder, jsValue);
    else if (type == "uint32_t")
        numericResult = encodeNumericType<uint32_t>(encoder, jsValue);
    else if (type == "uint64_t")
        numericResult = encodeNumericType<uint64_t>(encoder, jsValue);
    else {
        *exception = createTypeError(context, "Bad type name"_s);
        return false;
    }
    if (!numericResult) {
        *exception = createTypeError(context, "Failed to encode a number"_s);
        return false;
    }
    return true;
}


static JSC::JSObject* jsResultFromReplyDecoder(JSC::JSGlobalObject* globalObject, IPC::MessageName messageName, IPC::Decoder& decoder)
{
    auto& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto arrayBuffer = JSC::ArrayBuffer::create(decoder.buffer(), decoder.length());
    JSC::JSArrayBuffer* jsArrayBuffer = nullptr;
    if (auto* structure = globalObject->arrayBufferStructure(arrayBuffer->sharingMode()))
        jsArrayBuffer = JSC::JSArrayBuffer::create(vm, structure, WTFMove(arrayBuffer));
    if (!jsArrayBuffer) {
        throwException(globalObject, scope, JSC::createTypeError(globalObject, "Failed to create the array buffer for the reply"_s));
        return nullptr;
    }

    auto jsReplyArguments = jsValueForReplyArguments(globalObject, messageName, decoder);
    if (!jsReplyArguments) {
        throwException(globalObject, scope, JSC::createTypeError(globalObject, "Failed to decode the reply"_s));
        return nullptr;
    }

    if (jsReplyArguments->isEmpty()) {
        throwException(globalObject, scope, JSC::createTypeError(globalObject, "Failed to convert the reply to an JS array"_s));
        return nullptr;
    }

    auto catchScope = DECLARE_CATCH_SCOPE(vm);
    JSC::JSObject* jsResult = constructEmptyObject(globalObject, globalObject->objectPrototype());
    RETURN_IF_EXCEPTION(catchScope, nullptr);

    jsResult->putDirect(vm, vm.propertyNames->arguments, *jsReplyArguments);
    RETURN_IF_EXCEPTION(catchScope, nullptr);

    jsResult->putDirect(vm, JSC::Identifier::fromString(vm, "buffer"), jsArrayBuffer);
    RETURN_IF_EXCEPTION(catchScope, nullptr);

    return jsResult;
}

JSValueRef JSIPC::sendMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    auto jsIPC = makeRefPtr(toWrapped(context, thisObject));
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }

    if (argumentCount < 3) {
        *exception = createTypeError(context, "Must specify the target process, desination ID, and message ID as the first three arguments"_s);
        return JSValueMakeUndefined(context);
    }

    auto connection = processTargetFromArgument(globalObject, arguments[0], exception);
    if (!connection)
        return JSValueMakeUndefined(context);

    auto destinationID = destinationIDFromArgument(globalObject, arguments[1], exception);
    if (!destinationID)
        return JSValueMakeUndefined(context);

    auto messageID = messageIDFromArgument(globalObject, arguments[2], exception);
    if (!messageID)
        return JSValueMakeUndefined(context);

    auto messageName = static_cast<IPC::MessageName>(*messageID);
    auto encoder = makeUnique<IPC::Encoder>(messageName, *destinationID);

    JSValueRef returnValue = JSValueMakeUndefined(context);

    bool hasReply = !!messageReplyArgumentDescriptions(messageName);
    if (hasReply) {
        uint64_t listenerID = IPC::nextAsyncReplyHandlerID();
        *encoder << listenerID;

        JSObjectRef resolve;
        JSObjectRef reject;
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
        returnValue = JSObjectMakeDeferredPromise(context, &resolve, &reject, exception);
ALLOW_NEW_API_WITHOUT_GUARDS_END
        if (!returnValue)
            return JSValueMakeUndefined(context);

        JSGlobalContextRetain(JSContextGetGlobalContext(context));
        JSValueProtect(context, resolve);
        JSValueProtect(context, reject);
        IPC::addAsyncReplyHandler(*connection, listenerID, [messageName, context, resolve, reject](IPC::Decoder* replyDecoder) {
            auto* globalObject = toJS(context);
            auto& vm = globalObject->vm();
            JSC::JSLockHolder lock(vm);

            auto scope = DECLARE_CATCH_SCOPE(vm);
            auto* jsResult = jsResultFromReplyDecoder(globalObject, messageName, *replyDecoder);
            if (auto* exception = scope.exception()) {
                scope.clearException();
                JSValueRef arguments[] = { toRef(globalObject, exception) };
                JSObjectCallAsFunction(context, reject, reject, 1, arguments, nullptr);
            } else {
                JSValueRef arguments[] = { toRef(globalObject, jsResult) };
                JSObjectCallAsFunction(context, resolve, resolve, 1, arguments, nullptr);
            }

            JSValueUnprotect(context, reject);
            JSValueUnprotect(context, resolve);
            JSGlobalContextRelease(JSContextGetGlobalContext(context));
        });
    }

    if (argumentCount > 3) {
        if (!encodeArgument(*encoder, *jsIPC, context, arguments[3], exception))
            return JSValueMakeUndefined(context);
    }

    // FIXME: Add the support for specifying IPC options.

    connection->sendMessage(WTFMove(encoder), { });

    return returnValue;
}

JSValueRef JSIPC::sendSyncMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    auto jsIPC = makeRefPtr(toWrapped(context, thisObject));
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }

    if (argumentCount < 4) {
        *exception = createTypeError(context, "Must specify the target process, desination ID, and message ID as the first three arguments"_s);
        return JSValueMakeUndefined(context);
    }

    auto connection = processTargetFromArgument(globalObject, arguments[0], exception);
    if (!connection)
        return JSValueMakeUndefined(context);

    auto destinationID = destinationIDFromArgument(globalObject, arguments[1], exception);
    if (!destinationID)
        return JSValueMakeUndefined(context);

    auto messageID = messageIDFromArgument(globalObject, arguments[2], exception);
    if (!messageID)
        return JSValueMakeUndefined(context);

    Seconds timeout;
    {
        auto jsValue = toJS(globalObject, arguments[3]);
        if (!jsValue.isNumber()) {
            *exception = createTypeError(context, "Timeout must be a number"_s);
            return JSValueMakeUndefined(context);
        }
        timeout = Seconds { jsValue.asNumber() };
    }

    // FIXME: Support the options.

    uint64_t syncRequestID = 0;
    auto messageName = static_cast<IPC::MessageName>(*messageID);
    auto encoder = connection->createSyncMessageEncoder(messageName, *destinationID, syncRequestID);

    if (argumentCount > 4) {
        if (!encodeArgument(*encoder, *jsIPC, context, arguments[4], exception))
            return JSValueMakeUndefined(context);
    }

    if (auto replyDecoder = connection->sendSyncMessage(syncRequestID, WTFMove(encoder), timeout, { })) {
        auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
        auto* jsResult = jsResultFromReplyDecoder(globalObject, messageName, *replyDecoder);
        if (scope.exception()) {
            *exception = toRef(globalObject, scope.exception());
            scope.clearException();
            return JSValueMakeUndefined(context);
        }
        return toRef(globalObject, jsResult);
    }

    return JSValueMakeUndefined(context);
}

JSValueRef JSIPC::visitedLinkStoreID(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef* exception)
{
    return retrieveID(context, thisObject, exception, [](JSIPC& wrapped) {
        auto webPage = makeRef(*wrapped.m_webPage);
        return webPage->visitedLinkTableID();
    });
}

JSValueRef JSIPC::frameID(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef* exception)
{
    return retrieveID(context, thisObject, exception, [](JSIPC& wrapped) {
        return wrapped.m_webFrame->frameID().toUInt64();
    });
}

JSValueRef JSIPC::pageID(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef* exception)
{
    return retrieveID(context, thisObject, exception, [](JSIPC& wrapped) {
        return wrapped.m_webPage->identifier().toUInt64();
    });
}

JSValueRef JSIPC::sessionID(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef* exception)
{
    return retrieveID(context, thisObject, exception, [](JSIPC& wrapped) {
        return wrapped.m_webPage->sessionID().toUInt64();
    });
}

JSValueRef JSIPC::webPageProxyID(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef* exception)
{
    return retrieveID(context, thisObject, exception, [](JSIPC& wrapped) {
        return wrapped.m_webPage->webPageProxyID();
    });
}

JSValueRef JSIPC::retrieveID(JSContextRef context, JSObjectRef thisObject, JSValueRef* exception, const WTF::Function<uint64_t(JSIPC&)>& callback)
{
    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);

    RefPtr<JSIPC> wrapped = toWrapped(context, thisObject);
    if (!wrapped) {
        *exception = toRef(JSC::createTypeError(toJS(context), "Wrong type"_s));
        return JSValueMakeUndefined(context);
    }

    auto id = callback(*wrapped);
    JSC::JSCell* jsValue = JSC::JSBigInt::createFrom(globalObject, id);
    return toRef(vm, jsValue);
}

static JSC::JSValue createJSArrayForArgumentDescriptions(JSC::JSGlobalObject* globalObject, Optional<Vector<IPC::ArgumentDescription>>&& argumentDescriptions)
{
    if (!argumentDescriptions)
        return JSC::jsNull();

    auto& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);
    JSC::JSObject* argumentsArray = JSC::constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, JSC::jsTDZValue());

    for (unsigned argumentIndex = 0; argumentIndex < argumentDescriptions->size(); ++argumentIndex) {
        auto& description = argumentDescriptions->at(argumentIndex);
        JSC::JSObject* jsDescriptions = constructEmptyObject(globalObject, globalObject->objectPrototype());
        RETURN_IF_EXCEPTION(scope, JSC::jsTDZValue());

        argumentsArray->putDirectIndex(globalObject, argumentIndex, jsDescriptions);
        RETURN_IF_EXCEPTION(scope, JSC::jsTDZValue());

        jsDescriptions->putDirect(vm, JSC::Identifier::fromString(vm, "name"), JSC::jsString(vm, String(description.name)));
        RETURN_IF_EXCEPTION(scope, JSC::jsTDZValue());

        jsDescriptions->putDirect(vm, JSC::Identifier::fromString(vm, "type"), JSC::jsString(vm, String(description.type)));
        RETURN_IF_EXCEPTION(scope, JSC::jsTDZValue());

        jsDescriptions->putDirect(vm, JSC::Identifier::fromString(vm, "optional"), JSC::jsBoolean(description.isOptional));
        RETURN_IF_EXCEPTION(scope, JSC::jsTDZValue());

        if (description.enumName) {
            jsDescriptions->putDirect(vm, JSC::Identifier::fromString(vm, "enum"), JSC::jsString(vm, String(description.enumName)));
            RETURN_IF_EXCEPTION(scope, JSC::jsTDZValue());
        }
    }
    return argumentsArray;
}

JSValueRef JSIPC::messages(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);

    auto* impl = toWrapped(context, thisObject);
    if (!impl) {
        *exception = toRef(JSC::createTypeError(toJS(context), "Wrong type"_s));
        return JSValueMakeUndefined(context);
    }

    auto scope = DECLARE_CATCH_SCOPE(vm);
    JSC::JSObject* messagesObject = constructEmptyObject(globalObject, globalObject->objectPrototype());
    RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));

    auto nameIdent = JSC::Identifier::fromString(vm, "name");
    auto replyArgumentsIdent = JSC::Identifier::fromString(vm, "replyArguments");
    auto isSyncIdent = JSC::Identifier::fromString(vm, "isSync");
    for (unsigned i = 0; i < static_cast<unsigned>(IPC::MessageName::Last); ++i) {
        auto name = static_cast<IPC::MessageName>(i);

        JSC::JSObject* dictionary = constructEmptyObject(globalObject, globalObject->objectPrototype());
        RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));

        dictionary->putDirect(vm, nameIdent, JSC::JSValue(i));
        RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));

        auto argumentDescriptions = createJSArrayForArgumentDescriptions(globalObject, IPC::messageArgumentDescriptions(name));
        if (argumentDescriptions.isEmpty())
            return JSValueMakeUndefined(context);
        dictionary->putDirect(vm, vm.propertyNames->arguments, argumentDescriptions);            
        RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));

        auto replyArgumentDescriptions = createJSArrayForArgumentDescriptions(globalObject, IPC::messageReplyArgumentDescriptions(name));
        if (replyArgumentDescriptions.isEmpty())
            return JSValueMakeUndefined(context);
        dictionary->putDirect(vm, replyArgumentsIdent, replyArgumentDescriptions);            
        RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));

        dictionary->putDirect(vm, isSyncIdent, JSC::jsBoolean(messageIsSync(name)));            
        RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));

        messagesObject->putDirect(vm, JSC::Identifier::fromString(vm, description(name)), dictionary);
        RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));
    }

    return toRef(vm, messagesObject);
}

JSMessageListener::JSMessageListener(JSIPC& jsIPC, Type type, JSContextRef context, JSObjectRef callback)
    : m_jsIPC(makeWeakPtr(jsIPC))
    , m_type(type)
    , m_context(context)
    , m_callback(callback)
{
    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);

    auto catchScope = DECLARE_CATCH_SCOPE(vm);

    // We can't retain the global context here as that would cause a leak
    // since this object is supposed to live as long as the global object is alive.
    JSC::PrivateName uniquePrivateName;
    globalObject->putDirect(vm, uniquePrivateName, toJS(globalObject, callback));

    UNUSED_PARAM(catchScope);
}

void JSMessageListener::didReceiveMessage(const IPC::Decoder& decoder)
{
    if (m_type != Type::Incoming)
        return;

    RELEASE_ASSERT(m_jsIPC);
    auto protectOwnerOfThis = makeRef(*m_jsIPC);
    auto* globalObject = toJS(m_context);
    JSC::JSLockHolder lock(globalObject->vm());

    auto mutableDecoder = IPC::Decoder::create(decoder.buffer(), decoder.length(), nullptr, { });
    auto* description = jsDescriptionFromDecoder(globalObject, *mutableDecoder);

    JSValueRef arguments[] = { description ? toRef(globalObject, description) : JSValueMakeUndefined(m_context) };
    JSObjectCallAsFunction(m_context, m_callback, m_callback, std::size(arguments), arguments, nullptr);
}

void JSMessageListener::willSendMessage(const IPC::Encoder& encoder, OptionSet<IPC::SendOption>)
{
    if (m_type != Type::Outgoing)
        return;

    RELEASE_ASSERT(m_jsIPC);
    auto protectOwnerOfThis = makeRef(*m_jsIPC);
    auto* globalObject = toJS(m_context);
    JSC::JSLockHolder lock(globalObject->vm());

    auto decoder = IPC::Decoder::create(encoder.buffer(), encoder.bufferSize(), nullptr, { });
    auto* description = jsDescriptionFromDecoder(globalObject, *decoder);

    JSValueRef arguments[] = { description ? toRef(globalObject, description) : JSValueMakeUndefined(m_context) };
    JSObjectCallAsFunction(m_context, m_callback, m_callback, WTF_ARRAY_LENGTH(arguments), arguments, nullptr);
}

JSC::JSObject* JSMessageListener::jsDescriptionFromDecoder(JSC::JSGlobalObject* globalObject, IPC::Decoder& decoder)
{
    auto& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto* jsResult = constructEmptyObject(globalObject, globalObject->objectPrototype());
    RETURN_IF_EXCEPTION(scope, nullptr);

    jsResult->putDirect(vm, JSC::Identifier::fromString(vm, "name"), JSC::JSValue(static_cast<unsigned>(decoder.messageName())));
    RETURN_IF_EXCEPTION(scope, nullptr);

    jsResult->putDirect(vm, JSC::Identifier::fromString(vm, "description"), JSC::jsString(vm, IPC::description(decoder.messageName())));
    RETURN_IF_EXCEPTION(scope, nullptr);

    jsResult->putDirect(vm, JSC::Identifier::fromString(vm, "destinationID"), JSC::JSValue(decoder.destinationID()));
    RETURN_IF_EXCEPTION(scope, nullptr);

    if (decoder.isSyncMessage()) {
        if (uint64_t syncRequestID = 0; decoder.decode(syncRequestID)) {
            jsResult->putDirect(vm, JSC::Identifier::fromString(vm, "syncRequestID"), JSC::JSValue(syncRequestID));
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
    } else if (messageReplyArgumentDescriptions(decoder.messageName())) {
        if (uint64_t listenerID = 0; decoder.decode(listenerID)) {
            jsResult->putDirect(vm, JSC::Identifier::fromString(vm, "listenerID"), JSC::JSValue(listenerID));
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
    }

    auto arrayBuffer = JSC::ArrayBuffer::create(decoder.buffer(), decoder.length());
    if (auto* structure = globalObject->arrayBufferStructure(arrayBuffer->sharingMode())) {
        if (auto* jsArrayBuffer = JSC::JSArrayBuffer::create(vm, structure, WTFMove(arrayBuffer))) {
            jsResult->putDirect(vm, JSC::Identifier::fromString(vm, "buffer"), jsArrayBuffer);
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
    }

    auto jsReplyArguments = jsValueForArguments(globalObject, decoder.messageName(), decoder);
    if (jsReplyArguments) {
        jsResult->putDirect(vm, vm.propertyNames->arguments, jsReplyArguments->isEmpty() ? JSC::jsNull() : *jsReplyArguments);
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    return jsResult;
}

void inject(WebPage& webPage, WebFrame& webFrame, WebCore::DOMWrapperWorld& world)
{
    auto* globalObject = webFrame.coreFrame()->script().globalObject(world);

    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    auto wrapped = JSIPC::create(webPage, webFrame);
    JSObjectRef wrapperObject = JSObjectMake(toGlobalRef(globalObject), JSIPC::wrapperClass(), wrapped.ptr());
    globalObject->putDirect(vm, JSC::Identifier::fromString(vm, "IPC"), toJS(globalObject, wrapperObject));

    scope.clearException();
}

} // namespace IPCTestingAPI

} // namespace WebKit

#endif
