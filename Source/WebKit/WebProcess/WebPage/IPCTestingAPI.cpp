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
#include "IPCSemaphore.h"
#include "IPCStreamTesterMessages.h"
#include "JSIPCBinding.h"
#include "MessageArgumentDescriptions.h"
#include "MessageObserver.h"
#include "NetworkProcessConnection.h"
#include "RemoteRenderingBackendCreationParameters.h"
#include "SerializedTypeInfo.h"
#include "StreamClientConnection.h"
#include "StreamConnectionBuffer.h"
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
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSValueRef.h>
#include <JavaScriptCore/JavaScript.h>
#include <JavaScriptCore/OpaqueJSString.h>
#include <WebCore/DOMWrapperWorld.h>
#include <WebCore/Frame.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ScriptController.h>
#include <wtf/PageBlock.h>

namespace WebKit {

namespace IPCTestingAPI {

class JSIPC;

static constexpr auto processTargetNameUI = "UI"_s;
#if ENABLE(GPU_PROCESS)
static constexpr auto processTargetNameGPU = "GPU"_s;
#endif
static constexpr auto processTargetNameNetworking = "Networking"_s;

static std::optional<uint64_t> destinationIDFromArgument(JSC::JSGlobalObject*, JSValueRef, JSValueRef*);
static std::optional<uint64_t> messageIDFromArgument(JSC::JSGlobalObject*, JSValueRef, JSValueRef*);
static JSC::JSObject* jsResultFromReplyDecoder(JSC::JSGlobalObject*, IPC::MessageName, IPC::Decoder&);
static bool encodeArgument(IPC::Encoder&, JSContextRef, JSValueRef, JSValueRef* exception);

class JSIPCSemaphore : public RefCounted<JSIPCSemaphore> {
public:
    static Ref<JSIPCSemaphore> create(IPC::Semaphore&& semaphore = { })
    {
        return adoptRef(*new JSIPCSemaphore(WTFMove(semaphore)));
    }

    JSObjectRef createJSWrapper(JSContextRef);
    static JSIPCSemaphore* toWrapped(JSContextRef, JSValueRef);

    void encode(IPC::Encoder& encoder) const { m_semaphore.encode(encoder); }
    IPC::Semaphore exchange(IPC::Semaphore&& semaphore = { })
    {
        return std::exchange(m_semaphore, WTFMove(semaphore));
    }

private:
    JSIPCSemaphore(IPC::Semaphore&& semaphore)
        : m_semaphore(WTFMove(semaphore))
    { }

    static JSClassRef wrapperClass();
    static JSIPCSemaphore* unwrap(JSObjectRef);
    static void initialize(JSContextRef, JSObjectRef);
    static void finalize(JSObjectRef);

    static const JSStaticFunction* staticFunctions();

    static JSValueRef signal(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef waitFor(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

    IPC::Semaphore m_semaphore;
};

class JSIPCConnectionHandle : public RefCounted<JSIPCConnectionHandle> {
public:
    static Ref<JSIPCConnectionHandle> create(IPC::Connection::Handle&& handle)
    {
        return adoptRef(*new JSIPCConnectionHandle(WTFMove(handle)));
    }

    JSObjectRef createJSWrapper(JSContextRef);
    static JSIPCConnectionHandle* toWrapped(JSContextRef, JSValueRef);

    void encode(IPC::Encoder& encoder) const { encoder << m_handle; }

private:
    JSIPCConnectionHandle(IPC::Connection::Handle&& handle)
        : m_handle(WTFMove(handle))
    { }

    static JSClassRef wrapperClass();
    static JSIPCConnectionHandle* unwrap(JSObjectRef);
    static void initialize(JSContextRef, JSObjectRef);
    static void finalize(JSObjectRef);

    static const JSStaticFunction* staticFunctions();

    IPC::Connection::Handle m_handle;
};

class JSIPCConnection : public RefCounted<JSIPCConnection>, private IPC::Connection::Client {
public:
    static Ref<JSIPCConnection> create(IPC::Connection::Identifier&& testedConnectionIdentifier)
    {
        return adoptRef(*new JSIPCConnection(WTFMove(testedConnectionIdentifier)));
    }

    JSObjectRef createJSWrapper(JSContextRef);
    static JSIPCConnection* toWrapped(JSContextRef, JSValueRef);

private:
    JSIPCConnection(IPC::Connection::Identifier&& testedConnectionIdentifier)
        : m_testedConnection { IPC::Connection::createServerConnection(testedConnectionIdentifier) }
    {
    }

    // IPC::Connection::Client overrides.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;
    void didClose(IPC::Connection&) final;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName) final;

    static JSClassRef wrapperClass();
    static JSIPCConnection* unwrap(JSObjectRef);
    static void initialize(JSContextRef, JSObjectRef);
    static void finalize(JSObjectRef);


    static const JSStaticFunction* staticFunctions();
    static JSValueRef open(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef invalidate(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef sendMessage(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef sendSyncMessage(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef waitForMessage(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

    Ref<IPC::Connection> m_testedConnection;
};

class JSIPCStreamClientConnection : public RefCounted<JSIPCStreamClientConnection>, public CanMakeWeakPtr<JSIPCStreamClientConnection> {
public:
    static Ref<JSIPCStreamClientConnection> create(JSIPC& jsIPC, IPC::Connection& connection, size_t bufferSize)
    {
        return adoptRef(*new JSIPCStreamClientConnection(jsIPC, connection, bufferSize));
    }

    JSObjectRef createJSWrapper(JSContextRef);
    static JSIPCStreamClientConnection* toWrapped(JSContextRef, JSValueRef);

    IPC::StreamClientConnection& connection() { return m_streamConnection; }

private:
    friend class JSIPCStreamConnectionBuffer;

    JSIPCStreamClientConnection(JSIPC& jsIPC, IPC::Connection& connection, size_t bufferSize)
        : m_jsIPC(jsIPC)
        , m_streamConnection { connection, bufferSize }
    { }

    void setSemaphores(JSIPCSemaphore& jsWakeUpSemaphore, JSIPCSemaphore& jsClientWaitSemaphore) { m_streamConnection.setSemaphores(jsWakeUpSemaphore.exchange(), jsClientWaitSemaphore.exchange()); }

    static JSClassRef wrapperClass();
    static JSIPCStreamClientConnection* unwrap(JSObjectRef);
    static void initialize(JSContextRef, JSObjectRef);
    static void finalize(JSObjectRef);

    static bool prepareToSendOutOfStreamMessage(JSContextRef, size_t argumentCount, const JSValueRef arguments[], JSIPC&, IPC::StreamClientConnection&, IPC::Encoder&, uint64_t destinationID, IPC::Timeout, JSValueRef* exception);

    static const JSStaticFunction* staticFunctions();
    static JSValueRef streamBuffer(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef setSemaphores(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef sendMessage(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef sendSyncMessage(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef sendIPCStreamTesterSyncCrashOnZero(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

    WeakPtr<JSIPC> m_jsIPC;
    IPC::StreamClientConnection m_streamConnection;
};

class JSIPCStreamConnectionBuffer : public RefCounted<JSIPCStreamConnectionBuffer> {
public:
    static Ref<JSIPCStreamConnectionBuffer> create(JSIPCStreamClientConnection& streamConnection)
    {
        return adoptRef(*new JSIPCStreamConnectionBuffer(streamConnection));
    }

    JSObjectRef createJSWrapper(JSContextRef);
    static JSIPCStreamConnectionBuffer* toWrapped(JSContextRef, JSValueRef);

    void encode(IPC::Encoder& encoder) const { m_streamConnection->connection().streamBuffer().encode(encoder); }

private:
    JSIPCStreamConnectionBuffer(JSIPCStreamClientConnection& streamConnection)
        : m_streamConnection(streamConnection)
    { }

    static JSClassRef wrapperClass();
    static JSIPCStreamConnectionBuffer* unwrap(JSObjectRef);
    static void initialize(JSContextRef, JSObjectRef);
    static void finalize(JSObjectRef);

    static const JSStaticFunction* staticFunctions();

    static JSValueRef readHeaderBytes(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef readDataBytes(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef readBytes(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception, Span<uint8_t>);
    static JSValueRef writeHeaderBytes(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef writeDataBytes(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef writeBytes(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception, Span<uint8_t>);

    WeakPtr<JSIPCStreamClientConnection> m_streamConnection;
};

class JSSharedMemory : public RefCounted<JSSharedMemory> {
public:
    static Ref<JSSharedMemory> create(size_t size)
    {
        return adoptRef(*new JSSharedMemory(size));
    }

    static Ref<JSSharedMemory> create(Ref<SharedMemory>&& sharedMemory)
    {
        return adoptRef(*new JSSharedMemory(WTFMove(sharedMemory)));
    }

    size_t size() const { return m_sharedMemory->size(); }
    SharedMemory::Handle createHandle(SharedMemory::Protection);

    JSObjectRef createJSWrapper(JSContextRef);
    static JSSharedMemory* toWrapped(JSContextRef, JSValueRef);

private:
    JSSharedMemory(size_t size)
        : m_sharedMemory(*SharedMemory::allocate(size))
    { }

    JSSharedMemory(Ref<SharedMemory>&& sharedMemory)
        : m_sharedMemory(WTFMove(sharedMemory))
    {
    }

    static JSClassRef wrapperClass();
    static JSSharedMemory* unwrap(JSObjectRef);
    static void initialize(JSContextRef, JSObjectRef);
    static void finalize(JSObjectRef);

    static const JSStaticFunction* staticFunctions();

    static JSValueRef readBytes(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef writeBytes(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

    Ref<SharedMemory> m_sharedMemory;
};

class JSMessageListener final : public IPC::MessageObserver {
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
    static JSIPC* toWrapped(JSContextRef, JSValueRef);
    static JSClassRef wrapperClass();

    WebFrame* webFrame() { return m_webFrame.get(); }

private:
    JSIPC(WebPage& webPage, WebFrame& webFrame)
        : m_webPage(webPage)
        , m_webFrame(webFrame)
    { }

    static JSIPC* unwrap(JSObjectRef);
    static void initialize(JSContextRef, JSObjectRef);
    static void finalize(JSObjectRef);
    static const JSStaticFunction* staticFunctions();
    static const JSStaticValue* staticValues();

    static void addMessageListener(JSMessageListener::Type, JSContextRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef addIncomingMessageListener(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef addOutgoingMessageListener(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

    static JSValueRef sendMessage(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef sendSyncMessage(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef waitForMessage(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

    static JSValueRef createConnectionPair(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

    static JSValueRef createStreamClientConnection(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef createSemaphore(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef createSharedMemory(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

    static JSValueRef vmPageSize(JSContextRef, JSObjectRef, JSStringRef, JSValueRef* exception);
    static JSValueRef visitedLinkStoreID(JSContextRef, JSObjectRef, JSStringRef, JSValueRef* exception);
    static JSValueRef webPageProxyID(JSContextRef, JSObjectRef, JSStringRef, JSValueRef* exception);
    static JSValueRef sessionID(JSContextRef, JSObjectRef, JSStringRef, JSValueRef* exception);
    static JSValueRef pageID(JSContextRef, JSObjectRef, JSStringRef, JSValueRef* exception);
    static JSValueRef frameID(JSContextRef, JSObjectRef, JSStringRef, JSValueRef* exception);
    static JSValueRef retrieveID(JSContextRef, JSObjectRef thisObject, JSValueRef* exception, const WTF::Function<uint64_t(JSIPC&)>&);

    static JSValueRef messages(JSContextRef, JSObjectRef, JSStringRef, JSValueRef* exception);
    static JSValueRef serializedTypeInfo(JSContextRef, JSObjectRef, JSStringRef, JSValueRef* exception);
    static JSValueRef serializedEnumInfo(JSContextRef, JSObjectRef, JSStringRef, JSValueRef* exception);
    static JSValueRef objectIdentifiers(JSContextRef, JSObjectRef, JSStringRef, JSValueRef* exception);
    static JSValueRef processTargets(JSContextRef, JSObjectRef, JSStringRef, JSValueRef* exception);

    WeakPtr<WebPage> m_webPage;
    WeakPtr<WebFrame> m_webFrame;
    Vector<UniqueRef<JSMessageListener>> m_messageListeners;
};

static JSValueRef createTypeError(JSContextRef context, const String& message)
{
    JSC::JSLockHolder lock(toJS(context)->vm());
    return toRef(JSC::createTypeError(toJS(context), message));
}

static std::optional<uint64_t> convertToUint64(JSC::JSValue jsValue)
{
    if (jsValue.isNumber()) {
        double value = jsValue.asNumber();
        if (value < 0 || trunc(value) != value)
            return std::nullopt;
        return value;
    }
    if (jsValue.isBigInt())
        return JSC::JSBigInt::toBigUInt64(jsValue);
    return std::nullopt;
}


static JSValueRef sendMessageWithJSArguments(IPC::Connection& connection, JSContextRef context, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());

    auto destinationID = destinationIDFromArgument(globalObject, arguments[0], exception);
    if (!destinationID)
        return JSValueMakeUndefined(context);

    auto messageID = messageIDFromArgument(globalObject, arguments[1], exception);
    if (!messageID)
        return JSValueMakeUndefined(context);

    auto messageName = static_cast<IPC::MessageName>(*messageID);
    auto encoder = makeUniqueRef<IPC::Encoder>(messageName, *destinationID);

    JSValueRef returnValue = JSValueMakeUndefined(context);

    bool hasReply = !!messageReplyArgumentDescriptions(messageName);
    if (hasReply) {
        uint64_t listenerID = IPC::nextAsyncReplyHandlerID();
        encoder.get() << listenerID;

        JSObjectRef resolve;
        JSObjectRef reject;
ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
        returnValue = JSObjectMakeDeferredPromise(context, &resolve, &reject, exception); // NOLINT
ALLOW_NEW_API_WITHOUT_GUARDS_END
        if (!returnValue)  // NOLINT
            return JSValueMakeUndefined(context);

        JSGlobalContextRetain(JSContextGetGlobalContext(context));
        JSValueProtect(context, resolve);
        JSValueProtect(context, reject);
        IPC::addAsyncReplyHandler(connection, listenerID, [messageName, context, resolve, reject](IPC::Decoder* replyDecoder) {
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

    if (argumentCount > 2) {
        if (!encodeArgument(encoder.get(), context, arguments[2], exception))
            return JSValueMakeUndefined(context);
    }

    // FIXME: Add the support for specifying IPC options.

    connection.sendMessage(WTFMove(encoder), { });
    return returnValue;
}

namespace {

struct SyncIPCMessageInfo {
    uint64_t destinationID;
    IPC::MessageName messageName;
    IPC::Timeout timeout;
};

}

static std::optional<SyncIPCMessageInfo> extractSyncIPCMessageInfo(JSContextRef context, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    ASSERT(argumentCount >= 2);
    auto* globalObject = toJS(context);
    auto destinationID = destinationIDFromArgument(globalObject, arguments[0], exception);
    if (!destinationID)
        return std::nullopt;

    auto messageID = messageIDFromArgument(globalObject, arguments[1], exception);
    if (!messageID)
        return std::nullopt;

    Seconds timeoutDuration;
    {
        auto jsValue = toJS(globalObject, arguments[2]);
        if (!jsValue.isNumber()) {
            *exception = createTypeError(context, "Timeout must be a number"_s);
            return std::nullopt;
        }
        timeoutDuration = Seconds { jsValue.asNumber() };
    }

    return { { *destinationID, static_cast<IPC::MessageName>(*messageID), { timeoutDuration } } };
}

static JSValueRef sendSyncMessageWithJSArguments(IPC::Connection& connection, JSContextRef context, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    auto info = extractSyncIPCMessageInfo(context, argumentCount, arguments, exception);
    if (!info)
        return JSValueMakeUndefined(context);

    auto [destinationID, messageName, timeout] = *info;

    // FIXME: Support the options.

    IPC::Connection::SyncRequestID syncRequestID;
    auto encoder = connection.createSyncMessageEncoder(messageName, destinationID, syncRequestID);

    if (argumentCount > 3) {
        if (!encodeArgument(encoder.get(), context, arguments[3], exception))
            return JSValueMakeUndefined(context);
    }

    if (auto replyDecoder = connection.sendSyncMessage(syncRequestID, WTFMove(encoder), timeout, { })) {
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

static JSValueRef waitForMessageWithJSArguments(IPC::Connection& connection, JSContextRef context, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());

    auto info = extractSyncIPCMessageInfo(context, argumentCount, arguments, exception);
    if (!info)
        return JSValueMakeUndefined(context);

    auto [destinationID, messageName, timeout] = *info;
    if (auto decoder = connection.waitForMessageForTesting(messageName, destinationID, timeout, { })) {
        auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
        auto jsResult = jsValueForArguments(globalObject, messageName, *decoder);
        if (scope.exception()) {
            *exception = toRef(globalObject, scope.exception());
            scope.clearException();
            return JSValueMakeUndefined(context);
        }
        return jsResult ? toRef(globalObject, *jsResult) : JSValueMakeUndefined(context);
    }
    return JSValueMakeUndefined(context);
}

JSObjectRef JSIPCSemaphore::createJSWrapper(JSContextRef context)
{
    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    JSObjectRef wrapperObject = JSObjectMake(toGlobalRef(globalObject), wrapperClass(), this);
    scope.clearException();
    return wrapperObject;
}

JSClassRef JSIPCSemaphore::wrapperClass()
{
    static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "Semaphore";
        definition.parentClass = nullptr;
        definition.staticValues = nullptr;
        definition.staticFunctions = staticFunctions();
        definition.initialize = initialize;
        definition.finalize = finalize;
        jsClass = JSClassCreate(&definition);
    }
    return jsClass;
}

inline JSIPCSemaphore* JSIPCSemaphore::unwrap(JSObjectRef object)
{
    return static_cast<JSIPCSemaphore*>(JSObjectGetPrivate(object));
}

JSIPCSemaphore* JSIPCSemaphore::toWrapped(JSContextRef context, JSValueRef value)
{
    if (!context || !value || !JSValueIsObjectOfClass(context, value, wrapperClass()))
        return nullptr;
    return unwrap(JSValueToObject(context, value, nullptr));
}

void JSIPCSemaphore::initialize(JSContextRef, JSObjectRef object)
{
    unwrap(object)->ref();
}

void JSIPCSemaphore::finalize(JSObjectRef object)
{
    unwrap(object)->deref();
}

const JSStaticFunction* JSIPCSemaphore::staticFunctions()
{
    static const JSStaticFunction functions[] = {
        { "signal", signal, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "waitFor", waitFor, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { 0, 0, 0 }
    };
    return functions;
}


JSObjectRef JSIPCConnectionHandle::createJSWrapper(JSContextRef context)
{
    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    JSObjectRef wrapperObject = JSObjectMake(toGlobalRef(globalObject), wrapperClass(), this);
    scope.clearException();
    return wrapperObject;
}

JSClassRef JSIPCConnectionHandle::wrapperClass()
{
    static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "ConnectionHandle";
        definition.parentClass = nullptr;
        definition.staticValues = nullptr;
        definition.staticFunctions = staticFunctions();
        definition.initialize = initialize;
        definition.finalize = finalize;
        jsClass = JSClassCreate(&definition);
    }
    return jsClass;
}

inline JSIPCConnectionHandle* JSIPCConnectionHandle::unwrap(JSObjectRef object)
{
    return static_cast<JSIPCConnectionHandle*>(JSObjectGetPrivate(object));
}

JSIPCConnectionHandle* JSIPCConnectionHandle::toWrapped(JSContextRef context, JSValueRef value)
{
    if (!context || !value || !JSValueIsObjectOfClass(context, value, wrapperClass()))
        return nullptr;
    return unwrap(JSValueToObject(context, value, nullptr));
}

void JSIPCConnectionHandle::initialize(JSContextRef, JSObjectRef object)
{
    unwrap(object)->ref();
}

void JSIPCConnectionHandle::finalize(JSObjectRef object)
{
    unwrap(object)->deref();
}

const JSStaticFunction* JSIPCConnectionHandle::staticFunctions()
{
    static const JSStaticFunction functions[] = {
        { 0, 0, 0 }
    };
    return functions;
}

JSObjectRef JSIPCConnection::createJSWrapper(JSContextRef context)
{
    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    JSObjectRef wrapperObject = JSObjectMake(toGlobalRef(globalObject), wrapperClass(), this);
    scope.clearException();
    return wrapperObject;
}

JSClassRef JSIPCConnection::wrapperClass()
{
    static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "Connection";
        definition.parentClass = nullptr;
        definition.staticValues = nullptr;
        definition.staticFunctions = staticFunctions();
        definition.initialize = initialize;
        definition.finalize = finalize;
        jsClass = JSClassCreate(&definition);
    }
    return jsClass;
}

inline JSIPCConnection* JSIPCConnection::unwrap(JSObjectRef object)
{
    return static_cast<JSIPCConnection*>(JSObjectGetPrivate(object));
}

JSIPCConnection* JSIPCConnection::toWrapped(JSContextRef context, JSValueRef value)
{
    if (!context || !value || !JSValueIsObjectOfClass(context, value, wrapperClass()))
        return nullptr;
    return unwrap(JSValueToObject(context, value, nullptr));
}

void JSIPCConnection::initialize(JSContextRef, JSObjectRef object)
{
    unwrap(object)->ref();
}

void JSIPCConnection::finalize(JSObjectRef object)
{
    unwrap(object)->deref();
}

void JSIPCConnection::didReceiveMessage(IPC::Connection&, IPC::Decoder&)
{
    ASSERT_NOT_REACHED();
}

bool JSIPCConnection::didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&)
{
    ASSERT_NOT_REACHED();
    return false;
}

void JSIPCConnection::didClose(IPC::Connection&)
{
}

void JSIPCConnection::didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName)
{
    ASSERT_NOT_REACHED();
}


const JSStaticFunction* JSIPCConnection::staticFunctions()
{
    static const JSStaticFunction functions[] = {
        { "open", open, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "invalidate", invalidate, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "sendMessage", sendMessage, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "sendSyncMessage", sendSyncMessage, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "waitForMessage", waitForMessage, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { 0, 0, 0 }
    };
    return functions;
}

JSValueRef JSIPCConnection::open(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef* exception)
{
    RefPtr self = toWrapped(context, thisObject);
    if (!self) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    self->m_testedConnection->open(*self);
    return JSValueMakeUndefined(context);
}

JSValueRef JSIPCConnection::invalidate(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef* exception)
{
    RefPtr self = toWrapped(context, thisObject);
    if (!self) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    self->m_testedConnection->invalidate();
    return JSValueMakeUndefined(context);
}

JSValueRef JSIPCConnection::sendMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    RefPtr self = toWrapped(context, thisObject);
    if (!self) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    if (argumentCount < 2) {
        *exception = createTypeError(context, "Must specify the destination ID and message ID as the first two arguments"_s);
        return JSValueMakeUndefined(context);
    }
    return sendMessageWithJSArguments(self->m_testedConnection, context, argumentCount, arguments, exception);
}

JSValueRef JSIPCConnection::sendSyncMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    RefPtr self = toWrapped(context, thisObject);
    if (!self) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    if (argumentCount < 2) {
        *exception = createTypeError(context, "Must specify the destination ID and message ID as the first two arguments"_s);
        return JSValueMakeUndefined(context);
    }
    return sendSyncMessageWithJSArguments(self->m_testedConnection, context, argumentCount, arguments, exception);
}

JSValueRef JSIPCConnection::waitForMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    RefPtr self = toWrapped(context, thisObject);
    if (!self) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    if (argumentCount < 2) {
        *exception = createTypeError(context, "Must specify the destination ID and message ID as the first two arguments"_s);
        return JSValueMakeUndefined(context);
    }
    return waitForMessageWithJSArguments(self->m_testedConnection, context, argumentCount, arguments, exception);
}

JSObjectRef JSIPCStreamClientConnection::createJSWrapper(JSContextRef context)
{
    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    JSObjectRef wrapperObject = JSObjectMake(toGlobalRef(globalObject), wrapperClass(), this);
    scope.clearException();
    return wrapperObject;
}

JSClassRef JSIPCStreamClientConnection::wrapperClass()
{
    static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "StreamClientConnection";
        definition.parentClass = nullptr;
        definition.staticValues = nullptr;
        definition.staticFunctions = staticFunctions();
        definition.initialize = initialize;
        definition.finalize = finalize;
        jsClass = JSClassCreate(&definition);
    }
    return jsClass;
}

inline JSIPCStreamClientConnection* JSIPCStreamClientConnection::unwrap(JSObjectRef object)
{
    return static_cast<JSIPCStreamClientConnection*>(JSObjectGetPrivate(object));
}

JSIPCStreamClientConnection* JSIPCStreamClientConnection::toWrapped(JSContextRef context, JSValueRef value)
{
    if (!context || !value || !JSValueIsObjectOfClass(context, value, wrapperClass()))
        return nullptr;
    return unwrap(JSValueToObject(context, value, nullptr));
}

void JSIPCStreamClientConnection::initialize(JSContextRef, JSObjectRef object)
{
    unwrap(object)->ref();
}

void JSIPCStreamClientConnection::finalize(JSObjectRef object)
{
    unwrap(object)->deref();
}

const JSStaticFunction* JSIPCStreamClientConnection::staticFunctions()
{
    static const JSStaticFunction functions[] = {
        { "streamBuffer", streamBuffer, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "setSemaphores", setSemaphores, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "sendMessage", sendMessage, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "sendSyncMessage", sendSyncMessage, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "sendIPCStreamTesterSyncCrashOnZero", sendIPCStreamTesterSyncCrashOnZero, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { 0, 0, 0 }
    };
    return functions;
}

JSValueRef JSIPCStreamClientConnection::streamBuffer(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    RefPtr jsStreamConnection = toWrapped(context, thisObject);
    if (!jsStreamConnection) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }

    return JSIPCStreamConnectionBuffer::create(*jsStreamConnection)->createJSWrapper(context);
}

JSValueRef JSIPCStreamClientConnection::setSemaphores(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    RefPtr jsStreamConnection = toWrapped(context, thisObject);
    if (!jsStreamConnection) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }

    if (argumentCount < 2) {
        *exception = createTypeError(context, "Must specify an IPC semaphore as the first and second argument"_s);
        return JSValueMakeUndefined(context);
    }

    RefPtr jsWakeUpSemaphore = JSIPCSemaphore::toWrapped(context, arguments[0]);
    if (!jsWakeUpSemaphore) {
        *exception = createTypeError(context, "Wrong type (expected Semaphore)"_s);
        return JSValueMakeUndefined(context);
    }

    RefPtr jsClientWaitSemaphore = JSIPCSemaphore::toWrapped(context, arguments[1]);
    if (!jsClientWaitSemaphore) {
        *exception = createTypeError(context, "Wrong type (expected Semaphore)"_s);
        return JSValueMakeUndefined(context);
    }

    jsStreamConnection->setSemaphores(*jsWakeUpSemaphore, *jsClientWaitSemaphore);
    return JSValueMakeUndefined(context);
}

struct IPCStreamMessageInfo {
    uint64_t destinationID;
    IPC::MessageName messageName;
    IPC::Timeout timeout;
};

static std::optional<IPCStreamMessageInfo> extractIPCStreamMessageInfo(JSContextRef context, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 3) {
        *exception = createTypeError(context, "Must specify destination ID, message ID, and timeout as the first three arguments"_s);
        return std::nullopt;
    }

    auto* globalObject = toJS(context);
    auto destinationID = destinationIDFromArgument(globalObject, arguments[0], exception);
    if (!destinationID)
        return std::nullopt;

    auto messageID = messageIDFromArgument(globalObject, arguments[1], exception);
    if (!messageID)
        return std::nullopt;

    Seconds timeoutDuration;
    {
        auto jsValue = toJS(globalObject, arguments[2]);
        if (!jsValue.isNumber()) {
            *exception = createTypeError(context, "Timeout must be a number"_s);
            return std::nullopt;
        }
        timeoutDuration = Seconds { jsValue.asNumber() };
    }

    return { { *destinationID, static_cast<IPC::MessageName>(*messageID), { timeoutDuration } } };
}

bool JSIPCStreamClientConnection::prepareToSendOutOfStreamMessage(JSContextRef context, size_t argumentCount, const JSValueRef arguments[], JSIPC& jsIPC, IPC::StreamClientConnection& streamConnection, IPC::Encoder& encoder, uint64_t destinationID, IPC::Timeout timeout, JSValueRef* exception)
{
    // FIXME: Add support for sending in-stream IPC messages when appropriate.
    if (argumentCount > 3) {
        if (!encodeArgument(encoder, context, arguments[3], exception))
            return false;
    }

    if (!streamConnection.trySendDestinationIDIfNeeded(destinationID, timeout))
        return false;

    auto span = streamConnection.tryAcquire(timeout);
    if (!span)
        return false;

    streamConnection.sendProcessOutOfStreamMessage(WTFMove(*span));
    return true;
}

JSValueRef JSIPCStreamClientConnection::sendMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());

    auto returnValue = JSValueMakeUndefined(context);
    RefPtr jsStreamConnection = toWrapped(context, thisObject);
    if (!jsStreamConnection) {
        *exception = createTypeError(context, "Wrong type"_s);
        return returnValue;
    }

    auto info = extractIPCStreamMessageInfo(context, argumentCount, arguments, exception);
    if (!info)
        return returnValue;

    auto [destinationID, messageName, timeout] = *info;
    auto& streamConnection = jsStreamConnection->connection();
    auto& connection = streamConnection.connectionForTesting();

    auto encoder = makeUniqueRef<IPC::Encoder>(messageName, destinationID);
    if (prepareToSendOutOfStreamMessage(context, argumentCount, arguments, *jsStreamConnection->m_jsIPC, streamConnection, encoder.get(), destinationID, timeout, exception))
        connection.sendMessage(WTFMove(encoder), { });

    return returnValue;
}

JSValueRef JSIPCStreamClientConnection::sendSyncMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());

    RefPtr jsStreamConnection = toWrapped(context, thisObject);
    if (!jsStreamConnection) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }

    auto info = extractIPCStreamMessageInfo(context, argumentCount, arguments, exception);
    if (!info)
        return JSValueMakeUndefined(context);

    auto [destinationID, messageName, timeout] = *info;
    auto& streamConnection = jsStreamConnection->connection();
    auto& connection = streamConnection.connectionForTesting();

    IPC::Connection::SyncRequestID syncRequestID;
    auto encoder = connection.createSyncMessageEncoder(messageName, destinationID, syncRequestID);
    if (!prepareToSendOutOfStreamMessage(context, argumentCount, arguments, *jsStreamConnection->m_jsIPC, streamConnection, encoder.get(), destinationID, timeout, exception))
        return JSValueMakeUndefined(context);

    if (auto replyDecoder = connection.sendSyncMessage(syncRequestID, WTFMove(encoder), timeout, { })) {
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

// FIXME(http://webkit.org/b/237197): Cannot send arbitrary messages, so we hard-code this one to be able to send it.
JSValueRef JSIPCStreamClientConnection::sendIPCStreamTesterSyncCrashOnZero(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());

    RefPtr jsStreamConnection = toWrapped(context, thisObject);
    if (!jsStreamConnection) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }

    if (argumentCount < 3) {
        *exception = createTypeError(context, "Must specify destination ID, value, and timeout as the first three arguments"_s);
        return JSValueMakeUndefined(context);
    }

    auto destinationID = destinationIDFromArgument(globalObject, arguments[0], exception);
    if (!destinationID)
        return JSValueMakeUndefined(context);

    int32_t value;
    {
        auto jsValue = toJS(globalObject, arguments[1]);
        if (!jsValue.isNumber()) {
            *exception = createTypeError(context, "value must be a number"_s);
            return JSValueMakeUndefined(context);
        }
        value = static_cast<int32_t>(jsValue.asNumber());
    }

    Seconds timeoutDuration;
    {
        auto jsValue = toJS(globalObject, arguments[2]);
        if (!jsValue.isNumber()) {
            *exception = createTypeError(context, "timeout must be a number"_s);
            return JSValueMakeUndefined(context);
        }
        timeoutDuration = Seconds { jsValue.asNumber() };
    }

    auto& streamConnection = jsStreamConnection->connection();
    enum JSIPCStreamTesterIdentifierType { };
    auto destination = makeObjectIdentifier<JSIPCStreamTesterIdentifierType>(*destinationID);

    auto result = streamConnection.sendSync(Messages::IPCStreamTester::SyncCrashOnZero(value), destination, timeoutDuration);
    if (!result) {
        *exception = createTypeError(context, "sync send failed"_s);
        return JSValueMakeUndefined(context);
    }

    auto [resultValue] = result.takeReply();
    return JSValueMakeNumber(context, resultValue);
}

JSObjectRef JSIPCStreamConnectionBuffer::createJSWrapper(JSContextRef context)
{
    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    JSObjectRef wrapperObject = JSObjectMake(toGlobalRef(globalObject), wrapperClass(), this);
    scope.clearException();
    return wrapperObject;
}

JSClassRef JSIPCStreamConnectionBuffer::wrapperClass()
{
    static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "StreamConnectionBuffer";
        definition.parentClass = nullptr;
        definition.staticValues = nullptr;
        definition.staticFunctions = staticFunctions();
        definition.initialize = initialize;
        definition.finalize = finalize;
        jsClass = JSClassCreate(&definition);
    }
    return jsClass;
}

inline JSIPCStreamConnectionBuffer* JSIPCStreamConnectionBuffer::unwrap(JSObjectRef object)
{
    return static_cast<JSIPCStreamConnectionBuffer*>(JSObjectGetPrivate(object));
}

JSIPCStreamConnectionBuffer* JSIPCStreamConnectionBuffer::toWrapped(JSContextRef context, JSValueRef value)
{
    if (!context || !value || !JSValueIsObjectOfClass(context, value, wrapperClass()))
        return nullptr;
    return unwrap(JSValueToObject(context, value, nullptr));
}

void JSIPCStreamConnectionBuffer::initialize(JSContextRef, JSObjectRef object)
{
    unwrap(object)->ref();
}

void JSIPCStreamConnectionBuffer::finalize(JSObjectRef object)
{
    unwrap(object)->deref();
}

const JSStaticFunction* JSIPCStreamConnectionBuffer::staticFunctions()
{
    static const JSStaticFunction functions[] = {
        { "readHeaderBytes", readHeaderBytes, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "writeHeaderBytes", writeHeaderBytes, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "readDataBytes", readDataBytes, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "writeDataBytes", writeDataBytes, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { 0, 0, 0 }
    };
    return functions;
}

JSValueRef JSIPCSemaphore::signal(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    RefPtr jsIPCSemaphore = toWrapped(context, thisObject);
    if (!jsIPCSemaphore) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }

    jsIPCSemaphore->m_semaphore.signal();

    return JSValueMakeUndefined(context);
}

JSValueRef JSIPCSemaphore::waitFor(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    RefPtr jsIPCSemaphore = toWrapped(context, thisObject);
    if (!jsIPCSemaphore) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }

    if (argumentCount < 1) {
        *exception = createTypeError(context, "Must specify the timeout in milliseconds as the first argument"_s);
        return JSValueMakeUndefined(context);
    }

    auto jsValue = toJS(globalObject, arguments[0]);
    Seconds timeout;
    if (jsValue.isNumber()) {
        double milliseconds = jsValue.asNumber();
        if (std::isfinite(milliseconds) && milliseconds > 0)
            timeout = Seconds::fromMilliseconds(milliseconds);
    }
    if (!timeout) {
        *exception = createTypeError(context, "Timeout must be a positive number"_s);
        return JSValueMakeUndefined(context);
    }

    auto result = jsIPCSemaphore->m_semaphore.waitFor(timeout);

    return JSValueMakeBoolean(context, result);
}

SharedMemory::Handle JSSharedMemory::createHandle(SharedMemory::Protection protection)
{
    SharedMemory::Handle handle;
    m_sharedMemory->createHandle(handle, protection);
    return handle;
}

JSObjectRef JSSharedMemory::createJSWrapper(JSContextRef context)
{
    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    JSObjectRef wrapperObject = JSObjectMake(toGlobalRef(globalObject), wrapperClass(), this);
    scope.clearException();
    return wrapperObject;
}

JSClassRef JSSharedMemory::wrapperClass()
{
    static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "SharedMemory";
        definition.parentClass = nullptr;
        definition.staticValues = nullptr;
        definition.staticFunctions = staticFunctions();
        definition.initialize = initialize;
        definition.finalize = finalize;
        jsClass = JSClassCreate(&definition);
    }
    return jsClass;
}

inline JSSharedMemory* JSSharedMemory::unwrap(JSObjectRef object)
{
    return static_cast<JSSharedMemory*>(JSObjectGetPrivate(object));
}

JSSharedMemory* JSSharedMemory::toWrapped(JSContextRef context, JSValueRef value)
{
    if (!context || !value || !JSValueIsObjectOfClass(context, value, wrapperClass()))
        return nullptr;
    return unwrap(JSValueToObject(context, value, 0));
}

void JSSharedMemory::initialize(JSContextRef, JSObjectRef object)
{
    unwrap(object)->ref();
}

void JSSharedMemory::finalize(JSObjectRef object)
{
    unwrap(object)->deref();
}

const JSStaticFunction* JSSharedMemory::staticFunctions()
{
    static const JSStaticFunction functions[] = {
        { "readBytes", readBytes, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "writeBytes", writeBytes, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { 0, 0, 0 }
    };
    return functions;
}

JSValueRef JSSharedMemory::readBytes(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    RefPtr jsSharedMemory = toWrapped(context, thisObject);
    if (!jsSharedMemory) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }

    size_t offset = 0;
    size_t length = jsSharedMemory->m_sharedMemory->size();

    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    if (argumentCount) {
        auto offsetValue = convertToUint64(toJS(globalObject, arguments[0]));
        if (!offsetValue) {
            *exception = createTypeError(context, "The first argument must be a byte offset to read"_s);
            return JSValueMakeUndefined(context);
        }
        if (*offsetValue > length)
            offset = length;
        else
            offset = *offsetValue;
        length -= offset;
    }

    if (argumentCount > 1) {
        auto lengthValue = convertToUint64(toJS(globalObject, arguments[1]));
        if (!lengthValue) {
            *exception = createTypeError(context, "The second argument must be the number of bytes to read"_s);
            return JSValueMakeUndefined(context);
        }
        if (*lengthValue < length)
            length = *lengthValue;
    }

    auto arrayBuffer = JSC::ArrayBuffer::create(static_cast<uint8_t*>(jsSharedMemory->m_sharedMemory->data()) + offset, length);
    JSC::JSArrayBuffer* jsArrayBuffer = nullptr;
    if (auto* structure = globalObject->arrayBufferStructure(arrayBuffer->sharingMode()))
        jsArrayBuffer = JSC::JSArrayBuffer::create(vm, structure, WTFMove(arrayBuffer));
    if (!jsArrayBuffer) {
        *exception = createTypeError(context, "Failed to create the array buffer for the read bytes"_s);
        return JSValueMakeUndefined(context);
    }

    return toRef(jsArrayBuffer);
}

struct ArrayBufferData {
    void* buffer { nullptr };
    size_t length { 0 };
};

static ArrayBufferData arrayBufferDataFromValueRef(JSContextRef context, JSTypedArrayType type, JSValueRef valueRef, JSValueRef* exception)
{
    auto objectRef = JSValueToObject(context, valueRef, exception);
    if (!objectRef)
        return { };

    void* buffer = nullptr;
    if (type == kJSTypedArrayTypeArrayBuffer)
        buffer = JSObjectGetArrayBufferBytesPtr(context, objectRef, exception);
    else
        buffer = JSObjectGetTypedArrayBytesPtr(context, objectRef, exception);

    if (!buffer)
        return { };

    size_t length;
    if (type == kJSTypedArrayTypeArrayBuffer)
        length = JSObjectGetArrayBufferByteLength(context, objectRef, exception);
    else
        length = JSObjectGetTypedArrayByteLength(context, objectRef, exception);

    return { buffer, length };
}

JSValueRef JSSharedMemory::writeBytes(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    RefPtr jsSharedMemory = toWrapped(context, thisObject);
    if (!jsSharedMemory) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }

    auto type = argumentCount ? JSValueGetTypedArrayType(context, arguments[0], exception) : kJSTypedArrayTypeNone;
    if (type == kJSTypedArrayTypeNone) {
        *exception = createTypeError(context, "The first argument must be an array buffer or a typed array"_s);
        return JSValueMakeUndefined(context);
    }

    auto data = arrayBufferDataFromValueRef(context, type, arguments[0], exception);
    if (!data.buffer) {
        *exception = createTypeError(context, "Could not read the buffer"_s);
        return JSValueMakeUndefined(context);
    }

    size_t offset = 0;
    size_t length = data.length;
    size_t sharedMemorySize = jsSharedMemory->m_sharedMemory->size();

    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);

    if (argumentCount > 1) {
        auto offsetValue = convertToUint64(toJS(globalObject, arguments[1]));
        if (!offsetValue) {
            *exception = createTypeError(context, "The second argument must be a byte offset to write"_s);
            return JSValueMakeUndefined(context);
        }
        if (*offsetValue >= sharedMemorySize) {
            *exception = createTypeError(context, "Offset is too big"_s);
            return JSValueMakeUndefined(context);
        }
        offset = *offsetValue;
    }

    if (argumentCount > 2) {
        auto lengthValue = convertToUint64(toJS(globalObject, arguments[2]));
        if (!lengthValue) {
            *exception = createTypeError(context, "The third argument must be the number of bytes to write"_s);
            return JSValueMakeUndefined(context);
        }
        if (*lengthValue >= sharedMemorySize - offset || *lengthValue > length) {
            *exception = createTypeError(context, "The number of bytes to write is too big"_s);
            return JSValueMakeUndefined(context);
        }
        length = *lengthValue;
    }

    memcpy(static_cast<uint8_t*>(jsSharedMemory->m_sharedMemory->data()) + offset, data.buffer, length);

    return JSValueMakeUndefined(context);
}

JSValueRef JSIPCStreamConnectionBuffer::readHeaderBytes(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    RefPtr jsStreamBuffer = toWrapped(context, thisObject);
    if (!jsStreamBuffer) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }

    if (!jsStreamBuffer->m_streamConnection) {
        *exception = createTypeError(context, "streamConnection not valid"_s);
        return JSValueMakeUndefined(context);
    }

    IPC::StreamConnectionBuffer& buffer = jsStreamBuffer->m_streamConnection->m_streamConnection.bufferForTesting();
    Span<uint8_t> span = buffer.headerForTesting();

    return readBytes(context, nullptr, thisObject, argumentCount, arguments, exception, span);
}

JSValueRef JSIPCStreamConnectionBuffer::readDataBytes(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    RefPtr jsStreamBuffer = toWrapped(context, thisObject);
    if (!jsStreamBuffer) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }

    if (!jsStreamBuffer->m_streamConnection) {
        *exception = createTypeError(context, "streamConnection not valid"_s);
        return JSValueMakeUndefined(context);
    }

    IPC::StreamConnectionBuffer& buffer = jsStreamBuffer->m_streamConnection->m_streamConnection.bufferForTesting();
    Span<uint8_t> span = buffer.dataForTesting();

    return readBytes(context, nullptr, thisObject, argumentCount, arguments, exception, span);
}

JSValueRef JSIPCStreamConnectionBuffer::readBytes(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception, Span<uint8_t> span)
{
    size_t offset = 0;
    size_t length = span.size();
    uint8_t* data = span.data();
    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    if (argumentCount) {
        auto offsetValue = convertToUint64(toJS(globalObject, arguments[0]));
        if (!offsetValue) {
            *exception = createTypeError(context, "The first argument must be a byte offset to read"_s);
            return JSValueMakeUndefined(context);
        }
        if (*offsetValue > length)
            offset = length;
        else
            offset = *offsetValue;
        length -= offset;
    }

    if (argumentCount > 1) {
        auto lengthValue = convertToUint64(toJS(globalObject, arguments[1]));
        if (!lengthValue) {
            *exception = createTypeError(context, "The second argument must be the number of bytes to read"_s);
            return JSValueMakeUndefined(context);
        }
        if (*lengthValue < length)
            length = *lengthValue;
    }

    auto arrayBuffer = JSC::ArrayBuffer::create(data + offset, length);
    JSC::JSArrayBuffer* jsArrayBuffer = nullptr;
    if (auto* structure = globalObject->arrayBufferStructure(arrayBuffer->sharingMode()))
        jsArrayBuffer = JSC::JSArrayBuffer::create(vm, structure, WTFMove(arrayBuffer));
    if (!jsArrayBuffer) {
        *exception = createTypeError(context, "Failed to create the array buffer for the read bytes"_s);
        return JSValueMakeUndefined(context);
    }

    return toRef(jsArrayBuffer);
}

JSValueRef JSIPCStreamConnectionBuffer::writeHeaderBytes(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    RefPtr jsStreamBuffer = toWrapped(context, thisObject);
    if (!jsStreamBuffer) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }

    if (!jsStreamBuffer->m_streamConnection) {
        *exception = createTypeError(context, "streamConnection not valid"_s);
        return JSValueMakeUndefined(context);
    }

    IPC::StreamConnectionBuffer& buffer = jsStreamBuffer->m_streamConnection->m_streamConnection.bufferForTesting();
    Span<uint8_t> span = buffer.headerForTesting();

    return writeBytes(context, nullptr, thisObject, argumentCount, arguments, exception, span);
}

JSValueRef JSIPCStreamConnectionBuffer::writeDataBytes(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    RefPtr jsStreamBuffer = toWrapped(context, thisObject);
    if (!jsStreamBuffer) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }

    if (!jsStreamBuffer->m_streamConnection) {
        *exception = createTypeError(context, "streamConnection not valid"_s);
        return JSValueMakeUndefined(context);
    }

    IPC::StreamConnectionBuffer& buffer = jsStreamBuffer->m_streamConnection->m_streamConnection.bufferForTesting();
    Span<uint8_t> span = buffer.dataForTesting();

    return writeBytes(context, nullptr, thisObject, argumentCount, arguments, exception, span);
}

JSValueRef JSIPCStreamConnectionBuffer::writeBytes(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception, Span<uint8_t> span)
{
    auto type = argumentCount ? JSValueGetTypedArrayType(context, arguments[0], exception) : kJSTypedArrayTypeNone;
    if (type == kJSTypedArrayTypeNone) {
        *exception = createTypeError(context, "The first argument must be an array buffer or a typed array"_s);
        return JSValueMakeUndefined(context);
    }

    auto data = arrayBufferDataFromValueRef(context, type, arguments[0], exception);
    if (!data.buffer) {
        *exception = createTypeError(context, "Could not read the buffer"_s);
        return JSValueMakeUndefined(context);
    }

    size_t offset = 0;
    size_t length = data.length;

    size_t sharedMemorySize = span.size();
    uint8_t* destinationData = span.data();

    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);

    if (argumentCount > 1) {
        auto offsetValue = convertToUint64(toJS(globalObject, arguments[1]));
        if (!offsetValue) {
            *exception = createTypeError(context, "The second argument must be a byte offset to write"_s);
            return JSValueMakeUndefined(context);
        }
        if (*offsetValue >= sharedMemorySize) {
            *exception = createTypeError(context, "Offset is too big"_s);
            return JSValueMakeUndefined(context);
        }
        offset = *offsetValue;
    }

    if (argumentCount > 2) {
        auto lengthValue = convertToUint64(toJS(globalObject, arguments[2]));
        if (!lengthValue) {
            *exception = createTypeError(context, "The third argument must be the number of bytes to write"_s);
            return JSValueMakeUndefined(context);
        }
        if (*lengthValue > sharedMemorySize - offset || *lengthValue > length) {
            *exception = createTypeError(context, "The number of bytes to write is too big"_s);
            return JSValueMakeUndefined(context);
        }
        length = *lengthValue;
    }

    memcpy(destinationData + offset, data.buffer, length);
    return JSValueMakeUndefined(context);
}

JSClassRef JSIPC::wrapperClass()
{
    static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "IPC";
        definition.parentClass = nullptr;
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
        { "waitForMessage", waitForMessage, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "createConnectionPair", createConnectionPair, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "createStreamClientConnection", createStreamClientConnection, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "createSemaphore", createSemaphore, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "createSharedMemory", createSharedMemory, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { 0, 0, 0 }
    };
    return functions;
}

const JSStaticValue* JSIPC::staticValues()
{
    static const JSStaticValue values[] = {
        { "vmPageSize", vmPageSize, 0, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "visitedLinkStoreID", visitedLinkStoreID, 0, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "frameID", frameID, 0, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "pageID", pageID, 0, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "sessionID", sessionID, 0, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "webPageProxyID", webPageProxyID, 0, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "messages", messages, 0, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "serializedTypeInfo", serializedTypeInfo, 0, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "serializedEnumInfo", serializedEnumInfo, 0, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "objectIdentifiers", objectIdentifiers, 0, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "processTargets", processTargets, 0, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { 0, 0, 0, 0 }
    };
    return values;
}

static RefPtr<IPC::Connection> processTargetFromArgument(JSC::JSGlobalObject* globalObject, JSValueRef valueRef, JSValueRef* exception)
{
    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
    auto name = toJS(globalObject, valueRef).toWTFString(globalObject);
    if (scope.exception())
        return nullptr;

    if (name == processTargetNameUI)
        return WebProcess::singleton().parentProcessConnection();
#if ENABLE(GPU_PROCESS)
    if (name == processTargetNameGPU)
        return &WebProcess::singleton().ensureGPUProcessConnection().connection();
#endif
    if (name == processTargetNameNetworking)
        return &WebProcess::singleton().ensureNetworkProcessConnection().connection();

    *exception = toRef(JSC::createTypeError(globalObject, "Target process must be UI, GPU, or Networking"_s));
    return nullptr;
}

void JSIPC::addMessageListener(JSMessageListener::Type type, JSContextRef context, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    RefPtr jsIPC = toWrapped(context, thisObject);
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

static std::optional<uint64_t> destinationIDFromArgument(JSC::JSGlobalObject* globalObject, JSValueRef valueRef, JSValueRef* exception)
{
    auto jsValue = toJS(globalObject, valueRef);
    auto result = convertToUint64(jsValue);
    if (!result)
        *exception = toRef(JSC::createTypeError(globalObject, "destinationID must be an integer"_s));
    return result;
}

static std::optional<uint64_t> messageIDFromArgument(JSC::JSGlobalObject* globalObject, JSValueRef valueRef, JSValueRef* exception)
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

    auto data = arrayBufferDataFromValueRef(context, type, valueRef, exception);
    if (!data.buffer)
        return false;

    encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(data.buffer), data.length, 1);
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

#if ENABLE(GPU_PROCESS)
template <typename ObjectIdentifierType>
std::optional<ObjectIdentifier<ObjectIdentifierType>> getObjectIdentifierFromProperty(JSC::JSGlobalObject* globalObject, JSC::JSObject* jsObject, ASCIILiteral propertyName, JSC::CatchScope& scope)
{
    auto jsPropertyValue = jsObject->get(globalObject, JSC::Identifier::fromString(globalObject->vm(), propertyName));
    if (scope.exception())
        return std::nullopt;
    auto number = convertToUint64(jsPropertyValue);
    if (!number)
        return std::nullopt;
    return makeObjectIdentifier<ObjectIdentifierType>(*number);
}

static bool encodeRemoteRenderingBackendCreationParameters(IPC::Encoder& encoder, JSC::JSGlobalObject* globalObject, JSC::JSObject* jsObject, JSC::CatchScope& scope)
{
    auto identifier = getObjectIdentifierFromProperty<RenderingBackendIdentifierType>(globalObject, jsObject, "identifier"_s, scope);
    if (!identifier)
        return false;

    auto pageProxyID = getObjectIdentifierFromProperty<WebPageProxyIdentifierType>(globalObject, jsObject, "pageProxyID"_s, scope);
    if (!pageProxyID)
        return false;

    auto pageID = getObjectIdentifierFromProperty<WebCore::PageIdentifierType>(globalObject, jsObject, "pageID"_s, scope);
    if (!pageID)
        return false;

    RemoteRenderingBackendCreationParameters parameters { *identifier, *pageProxyID, *pageID };
    encoder << parameters;
    return true;
}
#endif

static bool encodeSharedMemory(IPC::Encoder& encoder, JSC::JSGlobalObject* globalObject, JSC::JSObject* jsObject, JSC::CatchScope& scope)
{
    auto jsSharedMemoryValue = jsObject->get(globalObject, JSC::Identifier::fromString(globalObject->vm(), "value"_s));
    if (scope.exception())
        return false;
    RefPtr jsSharedMemory = JSSharedMemory::toWrapped(toRef(globalObject), toRef(jsSharedMemoryValue));
    if (!jsSharedMemory)
        return false;

    auto jsProtectionValue = jsObject->get(globalObject, JSC::Identifier::fromString(globalObject->vm(), "protection"_s));
    if (scope.exception())
        return false;
    if (jsProtectionValue.isUndefined())
        return false;
    auto protectionValue = jsProtectionValue.toWTFString(globalObject);
    if (scope.exception())
        return false;
    auto protection = SharedMemory::Protection::ReadWrite;
    if (equalLettersIgnoringASCIICase(protectionValue, "readonly"_s))
        protection = SharedMemory::Protection::ReadOnly;
    else if (!equalLettersIgnoringASCIICase(protectionValue, "readwrite"_s))
        return false;

    encoder << jsSharedMemory->createHandle(protection);
    return true;
}

static bool encodeFrameInfoData(IPC::Encoder& encoder, JSC::JSGlobalObject* globalObject, JSC::JSObject* jsObject, JSC::CatchScope& scope)
{
    auto jsIPCValue = jsObject->get(globalObject, JSC::Identifier::fromString(globalObject->vm(), "value"_s));
    if (scope.exception())
        return false;
    RefPtr jsIPC = JSIPC::toWrapped(toRef(globalObject), toRef(jsIPCValue));
    if (!jsIPC)
        return false;
    RefPtr webFrame = jsIPC->webFrame();
    if (!webFrame)
        return false;
    encoder << webFrame->info();
    return true;
}

static bool encodeStreamConnectionBuffer(IPC::Encoder& encoder, JSC::JSGlobalObject* globalObject, JSC::JSValue jsValue, JSC::CatchScope& scope)
{
    RefPtr jsIPCStreamConnectionBuffer = JSIPCStreamConnectionBuffer::toWrapped(toRef(globalObject), toRef(jsValue));
    if (!jsIPCStreamConnectionBuffer)
        return false;

    jsIPCStreamConnectionBuffer->encode(encoder);
    return true;
}

static bool encodeSemaphore(IPC::Encoder& encoder, JSC::JSGlobalObject* globalObject, JSC::JSValue jsValue, JSC::CatchScope& scope)
{
    RefPtr jsIPCSemaphore = JSIPCSemaphore::toWrapped(toRef(globalObject), toRef(jsValue));
    if (!jsIPCSemaphore)
        return false;

    jsIPCSemaphore->encode(encoder);
    return true;
}

static bool encodeConnectionHandle(IPC::Encoder& encoder, JSC::JSGlobalObject* globalObject, JSC::JSValue jsValue, JSC::CatchScope& scope)
{
    RefPtr JSIPCConnectionHandle = JSIPCConnectionHandle::toWrapped(toRef(globalObject), toRef(jsValue));
    if (!JSIPCConnectionHandle)
        return false;

    JSIPCConnectionHandle->encode(encoder);
    return true;
}

struct VectorEncodeHelper {
    JSContextRef context;
    JSValueRef valueRef;
    JSValueRef* exception;
    bool& success;

    void encode(IPC::Encoder& encoder) const
    {
        if (!success)
            return;
        success = encodeArgument(encoder, context, valueRef, exception);
    }
};

enum class ArrayMode { Tuple, Vector };
static bool encodeArrayArgument(IPC::Encoder& encoder, ArrayMode arrayMode, JSContextRef context, JSValueRef valueRef, JSValueRef* exception)
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
        vector.append(VectorEncodeHelper { context, itemRef, exception, success });
    }
    if (arrayMode == ArrayMode::Tuple) {
        for (auto& item : vector)
            item.encode(encoder);
    } else
        encoder << vector;
    return success;
}

static bool encodeArgument(IPC::Encoder& encoder, JSContextRef context, JSValueRef valueRef, JSValueRef* exception)
{
    auto objectRef = JSValueToObject(context, valueRef, exception);
    if (!objectRef)
        return false;

    if (auto type = JSValueGetTypedArrayType(context, objectRef, exception); type != kJSTypedArrayTypeNone)
        return encodeTypedArray(encoder, context, objectRef, type, exception);

    if (JSValueIsArray(context, objectRef))
        return encodeArrayArgument(encoder, ArrayMode::Tuple, context, objectRef, exception);

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

    if (type == "IntPoint"_s) {
        if (!encodePointType<WebCore::IntPoint>(encoder, globalObject, jsObject, scope)) {
            *exception = createTypeError(context, "Failed to convert IntPoint"_s);
            return false;
        }
        return true;
    }

    if (type == "FloatPoint"_s) {
        if (!encodePointType<WebCore::IntPoint>(encoder, globalObject, jsObject, scope)) {
            *exception = createTypeError(context, "Failed to convert FloatPoint"_s);
            return false;
        }
        return true;
    }

    if (type == "IntRect"_s) {
        if (!encodeRectType<WebCore::IntRect>(encoder, globalObject, jsObject, scope)) {
            *exception = createTypeError(context, "Failed to convert IntRect"_s);
            return false;
        }
        return true;
    }

    if (type == "FloatRect"_s) {
        if (!encodeRectType<WebCore::FloatRect>(encoder, globalObject, jsObject, scope)) {
            *exception = createTypeError(context, "Failed to convert FloatRect"_s);
            return false;
        }
        return true;
    }

#if ENABLE(GPU_PROCESS)
    if (type == "RemoteRenderingBackendCreationParameters"_s) {
        if (!encodeRemoteRenderingBackendCreationParameters(encoder, globalObject, jsObject, scope)) {
            *exception = createTypeError(context, "Failed to convert RemoteRenderingBackendCreationParameters"_s);
            return false;
        }
        return true;
    }
#endif

    if (type == "SharedMemory"_s) {
        if (!encodeSharedMemory(encoder, globalObject, jsObject, scope)) {
            *exception = createTypeError(context, "Failed to convert SharedMemory"_s);
            return false;
        }
        return true;
    }

    if (type == "FrameInfoData"_s) {
        if (!encodeFrameInfoData(encoder, globalObject, jsObject, scope)) {
            *exception = createTypeError(context, "Failed to get the frame"_s);
            return false;
        }
        return true;
    }

    auto jsValue = jsObject->get(globalObject, JSC::Identifier::fromString(vm, "value"_s));
    if (scope.exception())
        return false;

    if (type == "StreamConnectionBuffer"_s) {
        if (!encodeStreamConnectionBuffer(encoder, globalObject, jsValue, scope)) {
            *exception = createTypeError(context, "Failed to convert StreamConnectionBuffer"_s);
            return false;
        }
        return true;
    }

    if (type == "Semaphore"_s) {
        if (!encodeSemaphore(encoder, globalObject, jsValue, scope)) {
            *exception = createTypeError(context, "Failed to convert Semaphore"_s);
            return false;
        }
        return true;
    }

    if (type == "ConnectionHandle"_s) {
        if (!encodeConnectionHandle(encoder, globalObject, jsValue, scope)) {
            *exception = createTypeError(context, "Failed to convert ConnectionHandle"_s);
            return false;
        }
        return true;
    }

    if (type == "Vector"_s) {
        if (!jsValue.inherits<JSC::JSArray>()) {
            *exception = createTypeError(context, "Vector value must be an array"_s);
            return false;
        }
        return encodeArrayArgument(encoder, ArrayMode::Vector, context, toRef(globalObject, jsValue), exception);
    }

    if (type == "String"_s) {
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

    if (type == "URL"_s) {
        if (jsValue.isUndefinedOrNull()) {
            encoder << URL { };
            return true;
        }
        auto string = jsValue.toWTFString(globalObject);
        if (scope.exception())
            return false;
        encoder << URL { string };
        return true;
    }

    if (type == "RegistrableDomain"_s) {
        if (jsValue.isUndefinedOrNull()) {
            encoder << WebCore::RegistrableDomain { };
            return true;
        }
        auto string = jsValue.toWTFString(globalObject);
        if (scope.exception())
            return false;
        encoder << WebCore::RegistrableDomain { URL { string } };
        return true;
    }

    if (type == "RGBA"_s) {
        if (!jsValue.isNumber()) {
            *exception = createTypeError(context, "RGBA value should be a number"_s);
            return false;
        }
        uint32_t rgba = jsValue.asNumber();
        encoder << WebCore::Color { WebCore::asSRGBA(WebCore::PackedColor::RGBA(rgba)) };
        return true;
    }

    bool numericResult;
    if (type == "bool"_s)
        numericResult = encodeNumericType<bool>(encoder, jsValue);
    else if (type == "double"_s)
        numericResult = encodeNumericType<double>(encoder, jsValue);
    else if (type == "float"_s)
        numericResult = encodeNumericType<float>(encoder, jsValue);
    else if (type == "int8_t"_s)
        numericResult = encodeNumericType<int8_t>(encoder, jsValue);
    else if (type == "int16_t"_s)
        numericResult = encodeNumericType<int16_t>(encoder, jsValue);
    else if (type == "int32_t"_s)
        numericResult = encodeNumericType<int32_t>(encoder, jsValue);
    else if (type == "int64_t"_s)
        numericResult = encodeNumericType<int64_t>(encoder, jsValue);
    else if (type == "uint8_t"_s)
        numericResult = encodeNumericType<uint8_t>(encoder, jsValue);
    else if (type == "uint16_t"_s)
        numericResult = encodeNumericType<uint16_t>(encoder, jsValue);
    else if (type == "uint32_t"_s)
        numericResult = encodeNumericType<uint32_t>(encoder, jsValue);
    else if (type == "uint64_t"_s)
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

    jsResult->putDirect(vm, JSC::Identifier::fromString(vm, "buffer"_s), jsArrayBuffer);
    RETURN_IF_EXCEPTION(catchScope, nullptr);

    return jsResult;
}

JSValueRef JSIPC::sendMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    if (argumentCount < 3) {
        *exception = createTypeError(context, "Must specify the target process, destination ID, and message ID as the first three arguments"_s);
        return JSValueMakeUndefined(context);
    }
    auto connection = processTargetFromArgument(globalObject, arguments[0], exception);
    if (!connection)
        return JSValueMakeUndefined(context);
    return sendMessageWithJSArguments(*connection, context, argumentCount - 1, arguments + 1, exception);
}

JSValueRef JSIPC::waitForMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    if (argumentCount < 3) {
        *exception = createTypeError(context, "Must specify the target process, destination ID, and message ID as the first three arguments"_s);
        return JSValueMakeUndefined(context);
    }
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    auto connection = processTargetFromArgument(globalObject, arguments[0], exception);
    if (!connection)
        return JSValueMakeUndefined(context);
    return waitForMessageWithJSArguments(*connection, context, argumentCount - 1, arguments + 1, exception);
}

JSValueRef JSIPC::sendSyncMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    if (argumentCount < 3) {
        *exception = createTypeError(context, "Must specify the target process, destination ID, and message ID as the first three arguments"_s);
        return JSValueMakeUndefined(context);
    }
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    auto connection = processTargetFromArgument(globalObject, arguments[0], exception);
    if (!connection)
        return JSValueMakeUndefined(context);
    return sendSyncMessageWithJSArguments(*connection, context, argumentCount - 1, arguments + 1, exception);
}

JSValueRef JSIPC::createConnectionPair(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    auto connectionIdentifiers = IPC::Connection::createConnectionIdentifierPair();
    if (!connectionIdentifiers)
        return JSValueMakeUndefined(context);
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    auto& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);
    JSC::JSObject* connectionPairObject = JSC::constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));
    int index = 0;
    auto jsValue = toJS(globalObject, JSIPCConnection::create(WTFMove(connectionIdentifiers->server))->createJSWrapper(context));
    connectionPairObject->putDirectIndex(globalObject, index++, jsValue);
    RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));
    jsValue = toJS(globalObject, JSIPCConnectionHandle::create(WTFMove(connectionIdentifiers->client))->createJSWrapper(context));
    connectionPairObject->putDirectIndex(globalObject, index++, jsValue);
    RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));
    return toRef(vm, connectionPairObject);
}

JSValueRef JSIPC::createStreamClientConnection(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }

    if (argumentCount < 2) {
        *exception = createTypeError(context, "Must specify the target process and buffer size as the first two arguments"_s);
        return JSValueMakeUndefined(context);
    }

    auto connection = processTargetFromArgument(globalObject, arguments[0], exception);
    if (!connection)
        return JSValueMakeUndefined(context);

    auto bufferSize = argumentCount ? convertToUint64(toJS(globalObject, arguments[1])) : std::optional<uint64_t> { };
    if (!bufferSize) {
        *exception = createTypeError(context, "Must specify the size"_s);
        return JSValueMakeUndefined(context);
    }

    return JSIPCStreamClientConnection::create(*jsIPC, *connection, *bufferSize)->createJSWrapper(context);
}

JSValueRef JSIPC::createSemaphore(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return JSIPCSemaphore::create()->createJSWrapper(context);
}

JSValueRef JSIPC::createSharedMemory(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (!toWrapped(context, thisObject)) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }

    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);

    auto size = argumentCount ? convertToUint64(toJS(globalObject, arguments[0])) : std::optional<uint64_t> { };
    if (!size) {
        *exception = createTypeError(context, "Must specify the size"_s);
        return JSValueMakeUndefined(context);
    }

    return JSSharedMemory::create(*size)->createJSWrapper(context);
}

JSValueRef JSIPC::serializedTypeInfo(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSC::JSObject* object = constructEmptyObject(globalObject, globalObject->objectPrototype());
    RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));

    for (auto& type : allSerializedTypes()) {
        JSC::JSObject* membersArray = JSC::constructEmptyArray(globalObject, nullptr);
        RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));

        for (size_t i = 0; i < type.members.size(); i++) {
            membersArray->putDirectIndex(globalObject, i, JSC::jsNontrivialString(vm, type.members[i]));
            RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));
        }

        object->putDirect(vm, JSC::Identifier::fromString(vm, String(type.name)), membersArray);
        RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));
    }

    return toRef(vm, object);
}

JSValueRef JSIPC::serializedEnumInfo(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSC::JSObject* object = constructEmptyObject(globalObject, globalObject->objectPrototype());
    RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));

    for (auto& enumeration : allSerializedEnums()) {
        JSC::JSObject* enumObject = constructEmptyObject(globalObject, globalObject->objectPrototype());
        RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));
        
        JSObjectRef jsEnumObject = JSValueToObject(context, toRef(vm, enumObject), exception);
        if (*exception)
            return JSValueMakeUndefined(context);

        Vector<JSValueRef> validValuesArray;
        validValuesArray.reserveInitialCapacity(enumeration.validValues.size());
        for (auto& validValue : enumeration.validValues)
            validValuesArray.uncheckedAppend(JSValueMakeNumber(context, validValue));
        JSObjectRef jsValidValues = JSObjectMakeArray(context, enumeration.validValues.size(), validValuesArray.data(), exception);
        if (*exception)
            return JSValueMakeUndefined(context);

        JSObjectSetProperty(context, jsEnumObject, adopt(JSStringCreateWithUTF8CString("validValues")).get(), jsValidValues, kJSPropertyAttributeNone, exception);
        if (*exception)
            return JSValueMakeUndefined(context);

        JSObjectSetProperty(context, jsEnumObject, adopt(JSStringCreateWithUTF8CString("isOptionSet")).get(), JSValueMakeNumber(context, enumeration.isOptionSet), kJSPropertyAttributeNone, exception);
        if (*exception)
            return JSValueMakeUndefined(context);

        JSObjectSetProperty(context, jsEnumObject, adopt(JSStringCreateWithUTF8CString("size")).get(), JSValueMakeNumber(context, enumeration.size), kJSPropertyAttributeNone, exception);
        if (*exception)
            return JSValueMakeUndefined(context);

        object->putDirect(vm, JSC::Identifier::fromString(vm, String(enumeration.name)), enumObject);
        RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));
    }

    return toRef(vm, object);
}

JSValueRef JSIPC::objectIdentifiers(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSC::JSObject* array = JSC::constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));

    auto identifiers = IPC::serializedIdentifiers();
    for (size_t i = 0; i < identifiers.size(); i++) {
        array->putDirectIndex(globalObject, i, toJS(JSValueMakeString(context, adopt(JSStringCreateWithUTF8CString(identifiers[i].characters())).get())));
        RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));
    }

    return toRef(vm, array);
}

JSValueRef JSIPC::vmPageSize(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef* exception)
{
    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    return JSValueMakeNumber(context, pageSize());
}

JSValueRef JSIPC::visitedLinkStoreID(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef* exception)
{
    return retrieveID(context, thisObject, exception, [](JSIPC& wrapped) {
        Ref webPage = *wrapped.m_webPage;
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

static JSC::JSValue createJSArrayForArgumentDescriptions(JSC::JSGlobalObject* globalObject, std::optional<Vector<IPC::ArgumentDescription>>&& argumentDescriptions)
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

        jsDescriptions->putDirect(vm, JSC::Identifier::fromString(vm, "name"_s), JSC::jsString(vm, String::fromLatin1(description.name)));
        RETURN_IF_EXCEPTION(scope, JSC::jsTDZValue());

        jsDescriptions->putDirect(vm, JSC::Identifier::fromString(vm, "type"_s), JSC::jsString(vm, String::fromLatin1(description.type)));
        RETURN_IF_EXCEPTION(scope, JSC::jsTDZValue());

        jsDescriptions->putDirect(vm, JSC::Identifier::fromString(vm, "optional"_s), JSC::jsBoolean(description.isOptional));
        RETURN_IF_EXCEPTION(scope, JSC::jsTDZValue());

        if (description.enumName) {
            jsDescriptions->putDirect(vm, JSC::Identifier::fromString(vm, "enum"_s), JSC::jsString(vm, String::fromLatin1(description.enumName)));
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

    auto nameIdent = JSC::Identifier::fromString(vm, "name"_s);
    auto replyArgumentsIdent = JSC::Identifier::fromString(vm, "replyArguments"_s);
    auto isSyncIdent = JSC::Identifier::fromString(vm, "isSync"_s);
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

        messagesObject->putDirect(vm, JSC::Identifier::fromLatin1(vm, description(name)), dictionary);
        RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));
    }

    return toRef(vm, messagesObject);
}

JSValueRef JSIPC::processTargets(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef* exception)
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
    JSC::JSObject* processTargetsObject = JSC::constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));
    int index = 0;
    processTargetsObject->putDirectIndex(globalObject, index++, JSC::jsNontrivialString(vm, processTargetNameUI));
    RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));
#if ENABLE(GPU_PROCESS)
    processTargetsObject->putDirectIndex(globalObject, index++, JSC::jsNontrivialString(vm, processTargetNameGPU));
    RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));
#endif
    processTargetsObject->putDirectIndex(globalObject, index++, JSC::jsNontrivialString(vm, processTargetNameNetworking));
    RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));
    return toRef(vm, processTargetsObject);
}

JSMessageListener::JSMessageListener(JSIPC& jsIPC, Type type, JSContextRef context, JSObjectRef callback)
    : m_jsIPC(jsIPC)
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
    Ref protectOwnerOfThis = *m_jsIPC;
    auto* globalObject = toJS(m_context);
    JSC::JSLockHolder lock(globalObject->vm());

    auto mutableDecoder = IPC::Decoder::create(decoder.buffer(), decoder.length(), { });
    auto* description = jsDescriptionFromDecoder(globalObject, *mutableDecoder);

    JSValueRef arguments[] = { description ? toRef(globalObject, description) : JSValueMakeUndefined(m_context) };
    JSObjectCallAsFunction(m_context, m_callback, m_callback, std::size(arguments), arguments, nullptr);
}

void JSMessageListener::willSendMessage(const IPC::Encoder& encoder, OptionSet<IPC::SendOption>)
{
    if (m_type != Type::Outgoing)
        return;

    RELEASE_ASSERT(m_jsIPC);
    Ref protectOwnerOfThis = *m_jsIPC;
    auto* globalObject = toJS(m_context);
    JSC::JSLockHolder lock(globalObject->vm());

    auto decoder = IPC::Decoder::create(encoder.buffer(), encoder.bufferSize(), { });
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

    jsResult->putDirect(vm, JSC::Identifier::fromString(vm, "name"_s), JSC::JSValue(static_cast<unsigned>(decoder.messageName())));
    RETURN_IF_EXCEPTION(scope, nullptr);

    jsResult->putDirect(vm, JSC::Identifier::fromString(vm, "description"_s), JSC::jsString(vm, String::fromLatin1(IPC::description(decoder.messageName()))));
    RETURN_IF_EXCEPTION(scope, nullptr);

    jsResult->putDirect(vm, JSC::Identifier::fromString(vm, "destinationID"_s), JSC::JSValue(decoder.destinationID()));
    RETURN_IF_EXCEPTION(scope, nullptr);

    if (decoder.isSyncMessage()) {
        IPC::Connection::SyncRequestID syncRequestID;
        if (decoder.decode(syncRequestID)) {
            jsResult->putDirect(vm, JSC::Identifier::fromString(vm, "syncRequestID"_s), JSC::JSValue(syncRequestID.toUInt64()));
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
    } else if (messageReplyArgumentDescriptions(decoder.messageName())) {
        if (uint64_t listenerID = 0; decoder.decode(listenerID)) {
            jsResult->putDirect(vm, JSC::Identifier::fromString(vm, "listenerID"_s), JSC::JSValue(listenerID));
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
    }

    auto arrayBuffer = JSC::ArrayBuffer::create(decoder.buffer(), decoder.length());
    if (auto* structure = globalObject->arrayBufferStructure(arrayBuffer->sharingMode())) {
        if (auto* jsArrayBuffer = JSC::JSArrayBuffer::create(vm, structure, WTFMove(arrayBuffer))) {
            jsResult->putDirect(vm, JSC::Identifier::fromString(vm, "buffer"_s), jsArrayBuffer);
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
    globalObject->putDirect(vm, JSC::Identifier::fromString(vm, "IPC"_s), toJS(globalObject, wrapperObject));

    scope.clearException();
}

} // namespace IPCTestingAPI

} // namespace WebKit

namespace IPC {

template<>
JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, IPC::Semaphore&& value)
{
    auto& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* object = JSC::constructEmptyObject(globalObject, globalObject->objectPrototype());
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    object->putDirect(vm, JSC::Identifier::fromString(vm, "type"_s), JSC::jsNontrivialString(vm, "Semaphore"_s));
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    auto jsValue = toJS(globalObject, WebKit::IPCTestingAPI::JSIPCSemaphore::create(WTFMove(value))->createJSWrapper(toRef(globalObject)));
    object->putDirect(vm, JSC::Identifier::fromString(vm, "value"_s), jsValue);
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    return object;
}

template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, WebKit::SharedMemory::Handle&& value)
{
    using SharedMemory = WebKit::SharedMemory;
    using Protection = WebKit::SharedMemory::Protection;

    auto& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* object = JSC::constructEmptyObject(globalObject, globalObject->objectPrototype());
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    object->putDirect(vm, JSC::Identifier::fromString(vm, "type"_s), JSC::jsNontrivialString(vm, "SharedMemory"_s));
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());

    auto protection = Protection::ReadWrite;
    auto sharedMemory = SharedMemory::map(value, protection);
    if (!sharedMemory) {
        protection = Protection::ReadOnly;
        sharedMemory = SharedMemory::map(value, protection);
        if (!sharedMemory)
            return JSC::JSValue();
    }

    auto jsValue = toJS(globalObject, WebKit::IPCTestingAPI::JSSharedMemory::create(sharedMemory.releaseNonNull())->createJSWrapper(toRef(globalObject)));
    object->putDirect(vm, JSC::Identifier::fromString(vm, "value"_s), jsValue);
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());

    object->putDirect(vm, JSC::Identifier::fromString(vm, "dataSize"_s), JSC::JSValue(value.size()));
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());

    object->putDirect(vm, JSC::Identifier::fromString(vm, "protection"_s), JSC::jsNontrivialString(vm, protection == Protection::ReadWrite ? "ReadWrite"_s : "ReadOnly"_s));
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());

    return object;
}

} // namespace IPC

#endif
