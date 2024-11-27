/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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
#include "IPCTesterReceiver.h"
#include "IPCTesterReceiverMessages.h"
#include "JSIPCBinding.h"
#include "MessageArgumentDescriptions.h"
#include "MessageObserver.h"
#include "NetworkProcessConnection.h"
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
#include <WebCore/JSDOMGlobalObject.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ScriptController.h>
#include <WebCore/SharedMemory.h>
#include <wtf/PageBlock.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebKit::IPCTestingAPI {

class JSIPC;
using WebCore::SharedMemory;

static constexpr auto processTargetNameUI = "UI"_s;
#if ENABLE(GPU_PROCESS)
static constexpr auto processTargetNameGPU = "GPU"_s;
#endif
static constexpr auto processTargetNameNetworking = "Networking"_s;

static std::optional<uint64_t> destinationIDFromArgument(JSC::JSGlobalObject*, JSValueRef, JSValueRef*);
static std::optional<IPC::MessageName> messageNameFromArgument(JSC::JSGlobalObject*, JSValueRef, JSValueRef*);
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

    void encode(IPC::Encoder& encoder) const { encoder << m_semaphore; }
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

    void encode(IPC::Encoder& encoder) { encoder << WTFMove(m_handle); }

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
    WTF_MAKE_FAST_ALLOCATED;
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(JSIPCConnection);
public:
    static Ref<JSIPCConnection> create(IPC::Connection::Identifier&& testedConnectionIdentifier)
    {
        return adoptRef(*new JSIPCConnection(IPC::Connection::createServerConnection(WTFMove(testedConnectionIdentifier))));
    }

    static Ref<JSIPCConnection> create(Ref<IPC::Connection> connection)
    {
        return adoptRef(*new JSIPCConnection(WTFMove(connection)));
    }

    JSObjectRef createJSWrapper(JSContextRef);
    static JSIPCConnection* toWrapped(JSContextRef, JSValueRef);

    Ref<IPC::Connection> connection() const { return m_testedConnection; }
private:
    JSIPCConnection(Ref<IPC::Connection> connection)
        : m_testedConnection { WTFMove(connection) }
    {
    }

    // IPC::Connection::Client overrides.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;
    void didClose(IPC::Connection&) final;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName, int32_t) final;

    static JSClassRef wrapperClass();
    static JSIPCConnection* unwrap(JSObjectRef);
    static void initialize(JSContextRef, JSObjectRef);
    static void finalize(JSObjectRef);


    static const JSStaticFunction* staticFunctions();
    static JSValueRef open(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef invalidate(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef sendMessage(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef sendWithAsyncReply(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef sendSyncMessage(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef waitForMessage(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef waitForAsyncReplyAndDispatchImmediately(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

    Ref<IPC::Connection> m_testedConnection;
};

class JSIPCStreamClientConnection : public RefCounted<JSIPCStreamClientConnection>, public CanMakeWeakPtr<JSIPCStreamClientConnection> {
public:
    static Ref<JSIPCStreamClientConnection> create(JSIPC& jsIPC, RefPtr<IPC::StreamClientConnection> connection)
    {
        return adoptRef(*new JSIPCStreamClientConnection(jsIPC, WTFMove(connection)));
    }

    JSObjectRef createJSWrapper(JSContextRef);
    static JSIPCStreamClientConnection* toWrapped(JSContextRef, JSValueRef);

    IPC::StreamClientConnection& connection() { return *m_streamConnection; }

private:
    friend class JSIPCStreamConnectionBuffer;

    JSIPCStreamClientConnection(JSIPC& jsIPC, RefPtr<IPC::StreamClientConnection> connection)
        : m_jsIPC(jsIPC)
        , m_streamConnection { WTFMove(connection) }
    {
    }

    void setSemaphores(JSIPCSemaphore& jsWakeUpSemaphore, JSIPCSemaphore& jsClientWaitSemaphore) { m_streamConnection->setSemaphores(jsWakeUpSemaphore.exchange(), jsClientWaitSemaphore.exchange()); }

    static JSClassRef wrapperClass();
    static JSIPCStreamClientConnection* unwrap(JSObjectRef);
    static void initialize(JSContextRef, JSObjectRef);
    static void finalize(JSObjectRef);

    bool prepareToSendOutOfStreamMessage(uint64_t destinationID, IPC::Timeout);

    static const JSStaticFunction* staticFunctions();
    static JSValueRef open(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef invalidate(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef streamBuffer(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef setSemaphores(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef sendMessage(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef sendWithAsyncReply(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef sendSyncMessage(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef sendIPCStreamTesterSyncCrashOnZero(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef waitForMessage(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef waitForAsyncReplyAndDispatchImmediately(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

    WeakPtr<JSIPC> m_jsIPC;
    RefPtr<IPC::StreamClientConnection> m_streamConnection;

    // Current tests expect that actions and their induced messages are waited on during same
    // run loop invocation (in JS). This means that messages of interest do not ever enter here.
    // Due to JSIPCStreamClientConnection supporting WeakPtr and IPC::MessageReceiver forcing WeakPtr, we store this as a member.
    class MessageReceiver : public IPC::Connection::Client {
        WTF_MAKE_FAST_ALLOCATED;
        WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(MessageReceiver);
    public:
        // IPC::MessageReceiver overrides.
        void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final { ASSERT_NOT_REACHED(); }
        bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final { ASSERT_NOT_REACHED(); return false; }
        void didClose(IPC::Connection&) final { }
        void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName, int32_t indexOfObjectFailingDecoding) final { ASSERT_NOT_REACHED(); }
    } m_dummyMessageReceiver;
};

class JSIPCStreamServerConnectionHandle : public RefCounted<JSIPCStreamServerConnectionHandle> {
public:
    static Ref<JSIPCStreamServerConnectionHandle> create(IPC::StreamServerConnection::Handle&& handle)
    {
        return adoptRef(*new JSIPCStreamServerConnectionHandle(WTFMove(handle)));
    }
    JSObjectRef createJSWrapper(JSContextRef);
    static JSIPCStreamServerConnectionHandle* toWrapped(JSContextRef, JSValueRef);
    void encode(IPC::Encoder& encoder) { encoder << WTFMove(m_handle); }
private:
    JSIPCStreamServerConnectionHandle(IPC::StreamServerConnection::Handle&& handle)
        : m_handle { WTFMove(handle) }
    {
    }
    static JSClassRef wrapperClass();
    static JSIPCStreamServerConnectionHandle* unwrap(JSObjectRef);
    static void initialize(JSContextRef, JSObjectRef);
    static void finalize(JSObjectRef);
    static const JSStaticFunction* staticFunctions();

    IPC::StreamServerConnection::Handle m_handle;
};

class JSIPCStreamConnectionBuffer : public RefCounted<JSIPCStreamConnectionBuffer> {
public:
    static Ref<JSIPCStreamConnectionBuffer> create(JSIPCStreamClientConnection& streamConnection)
    {
        return adoptRef(*new JSIPCStreamConnectionBuffer(streamConnection));
    }

    JSObjectRef createJSWrapper(JSContextRef);
    static JSIPCStreamConnectionBuffer* toWrapped(JSContextRef, JSValueRef);

    void encode(IPC::Encoder& encoder) const { encoder << m_streamConnection->connection().bufferForTesting().createHandle(); }

private:
    JSIPCStreamConnectionBuffer(JSIPCStreamClientConnection& streamConnection)
        : m_streamConnection(streamConnection)
    {
    }

    static JSClassRef wrapperClass();
    static JSIPCStreamConnectionBuffer* unwrap(JSObjectRef);
    static void initialize(JSContextRef, JSObjectRef);
    static void finalize(JSObjectRef);

    static const JSStaticFunction* staticFunctions();

    static JSValueRef readHeaderBytes(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef readDataBytes(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef readBytes(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception, std::span<uint8_t>);
    static JSValueRef writeHeaderBytes(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef writeDataBytes(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef writeBytes(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception, std::span<uint8_t>);

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
    std::optional<SharedMemory::Handle> createHandle(SharedMemory::Protection);

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

class JSMessageListener final : public IPC::MessageObserver, public RefCounted<JSMessageListener> {
    WTF_MAKE_TZONE_ALLOCATED(JSMessageListener);
public:
    enum class Type { Incoming, Outgoing };

    static Ref<JSMessageListener> create(JSIPC& jsIPC, Type type, JSC::JSGlobalObject* globalObject, JSObjectRef callback)
    {
        return adoptRef(*new JSMessageListener(jsIPC, type, globalObject, callback));
    }

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

private:
    JSMessageListener(JSIPC&, Type, JSC::JSGlobalObject*, JSObjectRef callback);

    void willSendMessage(const IPC::Encoder&, OptionSet<IPC::SendOption>) override;
    void didReceiveMessage(const IPC::Decoder&) override;
    JSC::JSObject* jsDescriptionFromDecoder(JSC::JSGlobalObject*, IPC::Decoder&);

    WeakPtr<JSIPC> m_jsIPC;
    Type m_type;
    JSC::Weak<WebCore::JSDOMGlobalObject> m_globalObject;
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
        , m_testerProxy(IPCTesterReceiver::create())
    { }

    static JSIPC* unwrap(JSObjectRef);
    static void initialize(JSContextRef, JSObjectRef);
    static void finalize(JSObjectRef);
    static const JSStaticFunction* staticFunctions();
    static const JSStaticValue* staticValues();

    static JSValueRef connectionForProcessTarget(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

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
    static JSValueRef addTesterReceiver(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);
    static JSValueRef removeTesterReceiver(JSContextRef, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

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

    RefPtr<JSIPCConnection> processTargetFromArgument(JSC::JSGlobalObject*, JSValueRef, JSValueRef* exception);

    WeakPtr<WebPage> m_webPage;
    WeakPtr<WebFrame> m_webFrame;
    Vector<Ref<JSMessageListener>> m_messageListeners;
    Ref<IPCTesterReceiver> m_testerProxy;
    RefPtr<JSIPCConnection> m_uiConnection;
    RefPtr<JSIPCConnection> m_networkConnection;
    RefPtr<JSIPCConnection> m_gpuConnection;
};

static JSValueRef createError(JSContextRef context, const String& message)
{
    JSC::JSLockHolder lock(toJS(context)->vm());
    return toRef(JSC::createError(toJS(context), message));
}

static JSValueRef createTypeError(JSContextRef context, const String& message)
{
    JSC::JSLockHolder lock(toJS(context)->vm());
    return toRef(JSC::createTypeError(toJS(context), message));
}

static JSValueRef createErrorFromIPCError(JSContextRef context, IPC::Error error)
{
    return createError(context, makeString("IPC error:"_s, IPC::errorAsString(error)));
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

static std::optional<double> convertToDouble(JSC::JSValue jsValue)
{
    if (jsValue.isNumber())
        return jsValue.asNumber();
    if (jsValue.isBigInt())
        return JSC::JSBigInt::toBigUInt64(jsValue);
    return std::nullopt;
}

namespace {

struct SyncIPCMessageInfo {
    uint64_t destinationID;
    IPC::MessageName messageName;
    IPC::Timeout timeout;
};

}

static std::optional<SyncIPCMessageInfo> extractSyncIPCMessageInfo(JSContextRef context, std::span<const JSValueRef> arguments, JSValueRef* exception)
{
    ASSERT(arguments.size() >= 2);
    auto* globalObject = toJS(context);
    auto destinationID = destinationIDFromArgument(globalObject, arguments[0], exception);
    if (!destinationID)
        return std::nullopt;

    auto messageName = messageNameFromArgument(globalObject, arguments[1], exception);
    if (!messageName)
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

    return { { *destinationID, *messageName, { timeoutDuration } } };
}

static JSValueRef jsSend(IPC::Connection& connection, uint64_t destinationID, IPC::MessageName messageName, JSContextRef context, const JSValueRef messageArguments, JSValueRef* exception)
{
    auto encoder = makeUniqueRef<IPC::Encoder>(messageName, destinationID);
    if (messageArguments && !encodeArgument(encoder.get(), context, messageArguments, exception))
        return JSValueMakeUndefined(context);
    connection.sendMessage(WTFMove(encoder), IPC::SendOption::IPCTestingMessage);
    return JSValueMakeUndefined(context);
}

static JSValueRef jsSendWithAsyncReply(IPC::Connection& connection, uint64_t destinationID, IPC::MessageName messageName, JSContextRef context, const JSObjectRef callback, const JSValueRef messageArguments, JSValueRef* exception)
{
    auto encoder = makeUniqueRef<IPC::Encoder>(messageName, destinationID);
    if (messageArguments && !encodeArgument(encoder.get(), context, messageArguments, exception))
        return JSValueMakeUndefined(context);
    JSGlobalContextRetain(JSContextGetGlobalContext(context));
    JSValueProtect(context, callback);
    IPC::Connection::AsyncReplyHandler handler = {
        [messageName, context, callback](IPC::Decoder* replyDecoder) {
            auto* globalObject = toJS(context);
            auto& vm = globalObject->vm();
            JSC::JSLockHolder lock(vm);
            auto scope = DECLARE_CATCH_SCOPE(vm);
            auto cleanup = makeScopeExit([context, callback] {
                JSValueUnprotect(context, callback);
                JSGlobalContextRelease(JSContextGetGlobalContext(context));
            });
            JSC::JSObject* jsResult = nullptr;
            if (replyDecoder && replyDecoder->isValid())
                jsResult = jsResultFromReplyDecoder(globalObject, messageName, *replyDecoder);
            std::array<JSValueRef, 1> arguments { nullptr };
            if (auto* exception = scope.exception()) {
                arguments[0] = toRef(globalObject, exception);
                scope.clearException();
            } else
                arguments[0] = toRef(globalObject, jsResult);
            JSObjectCallAsFunction(context, callback, callback, 1, arguments.data(), nullptr);
        },
        IPC::Connection::AsyncReplyID::generate()
    };
    auto asyncReplyID = *handler.replyID;
    auto result = connection.sendMessageWithAsyncReply(WTFMove(encoder), WTFMove(handler), IPC::SendOption::IPCTestingMessage);
    if (result != IPC::Error::NoError) {
        *exception = createErrorFromIPCError(context, result);
        return JSValueMakeUndefined(context);
    }
    return JSValueMakeNumber(context, asyncReplyID.toUInt64());
}

static JSValueRef jsSendSync(IPC::Connection& connection, uint64_t destinationID, IPC::MessageName messageName, IPC::Timeout timeout, JSContextRef context, const JSValueRef messageArguments, JSValueRef* exception)
{
    auto [encoder, syncRequestID] = connection.createSyncMessageEncoder(messageName, destinationID);
    if (messageArguments && !encodeArgument(encoder.get(), context, messageArguments, exception))
        return JSValueMakeUndefined(context);
    auto replyDecoderOrError = connection.sendSyncMessage(syncRequestID, WTFMove(encoder), timeout, { });
    if (replyDecoderOrError.has_value()) {
        auto* globalObject = toJS(context);
        JSC::JSLockHolder lock(globalObject->vm());
        auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
        auto* jsResult = jsResultFromReplyDecoder(globalObject, messageName, replyDecoderOrError.value().get());
        if (scope.exception()) {
            *exception = toRef(globalObject, scope.exception());
            scope.clearException();
            return JSValueMakeUndefined(context);
        }
        return toRef(globalObject, jsResult);
    }
    return JSValueMakeUndefined(context);
}

static JSValueRef jsWaitForMessage(IPC::Connection& connection, uint64_t destinationID, IPC::MessageName messageName, IPC::Timeout timeout, JSContextRef context, JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    auto decoderOrError = connection.waitForMessageForTesting(messageName, destinationID, timeout, { });
    if (decoderOrError.has_value()) {
        auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
        auto jsResult = jsValueForArguments(globalObject, messageName, decoderOrError.value().get());
        if (scope.exception()) {
            *exception = toRef(globalObject, scope.exception());
            scope.clearException();
            return JSValueMakeUndefined(context);
        }
        return jsResult ? toRef(globalObject, *jsResult) : JSValueMakeUndefined(context);
    }
    return JSValueMakeUndefined(context);
}

static JSValueRef jsWaitForAsyncReplyAndDispatchImmediately(IPC::Connection& connection, uint64_t destinationID, IPC::MessageName messageName, IPC::Timeout timeout, JSContextRef context, JSValueRef* exception)
{
    auto handler = connection.takeAsyncReplyHandler(IPC::Connection::AsyncReplyID { destinationID });
    if (!handler) {
        *exception = createError(context, "IPC error: no handler"_s);
        return JSValueMakeUndefined(context);
    }
    auto decoderOrError = connection.waitForMessageForTesting(messageName, destinationID, timeout, { });
    if (!decoderOrError.has_value()) {
        *exception = createErrorFromIPCError(context, decoderOrError.error());
        return JSValueMakeUndefined(context);
    }
    handler(&decoderOrError.value().get());
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

void JSIPCConnection::didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName, int32_t)
{
    ASSERT_NOT_REACHED();
}


const JSStaticFunction* JSIPCConnection::staticFunctions()
{
    static const JSStaticFunction functions[] = {
        { "open", open, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "invalidate", invalidate, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "sendMessage", sendMessage, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "sendWithAsyncReply", sendWithAsyncReply, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "sendSyncMessage", sendSyncMessage, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "waitForMessage", waitForMessage, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "waitForAsyncReplyAndDispatchImmediately", waitForAsyncReplyAndDispatchImmediately, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { 0, 0, 0 }
    };
    return functions;
}

JSValueRef JSIPCConnection::open(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef* exception)
{
    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    jsIPC->m_testedConnection->open(*jsIPC);
    return JSValueMakeUndefined(context);
}

JSValueRef JSIPCConnection::invalidate(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef* exception)
{
    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    jsIPC->m_testedConnection->invalidate();
    return JSValueMakeUndefined(context);
}

JSValueRef JSIPCConnection::sendMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t rawArgumentCount, const JSValueRef rawArguments[], JSValueRef* exception)
{
    auto arguments = unsafeMakeSpan(rawArguments, rawArgumentCount);

    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    if (arguments.size() < 2) {
        *exception = createTypeError(context, "Must specify the destination ID and message ID as the first two arguments"_s);
        return JSValueMakeUndefined(context);
    }
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    auto destinationID = destinationIDFromArgument(globalObject, arguments[0], exception);
    if (!destinationID)
        return JSValueMakeUndefined(context);
    auto messageName = messageNameFromArgument(globalObject, arguments[1], exception);
    if (!messageName)
        return JSValueMakeUndefined(context);
    JSValueRef messageArguments = arguments.size() > 2 ? arguments[2] : nullptr;
    return jsSend(jsIPC->m_testedConnection.get(), *destinationID, *messageName, context, messageArguments, exception);
}

JSValueRef JSIPCConnection::sendWithAsyncReply(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t rawArgumentCount, const JSValueRef rawArguments[], JSValueRef* exception)
{
    auto arguments = unsafeMakeSpan(rawArguments, rawArgumentCount);

    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    if (arguments.size() < 4) {
        *exception = createTypeError(context, "Must specify the destination ID, message ID, messageArguments, callback as the first four arguments"_s);
        return JSValueMakeUndefined(context);
    }
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    auto destinationID = destinationIDFromArgument(globalObject, arguments[0], exception);
    if (!destinationID)
        return JSValueMakeUndefined(context);
    auto messageName = messageNameFromArgument(globalObject, arguments[1], exception);
    if (!messageName)
        return JSValueMakeUndefined(context);
    if (!messageReplyArgumentDescriptions(*messageName)) {
        *exception = createError(context, "Message does not have a reply"_s);
        return JSValueMakeUndefined(context);
    }
    JSObjectRef callback = nullptr;
    if (JSValueIsObject(context, arguments[3])) {
        callback = JSValueToObject(context, arguments[3], exception);
        if (!JSObjectIsFunction(context, callback))
            callback = nullptr;
    }
    if (!callback) {
        *exception = createTypeError(context, "Must specify callback as the fourth argument"_s);
        return JSValueMakeUndefined(context);
    }
    return jsSendWithAsyncReply(jsIPC->m_testedConnection.get(), *destinationID, *messageName, context, callback, arguments[2], exception);
}

JSValueRef JSIPCConnection::sendSyncMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t rawArgumentCount, const JSValueRef rawArguments[], JSValueRef* exception)
{
    auto arguments = unsafeMakeSpan(rawArguments, rawArgumentCount);

    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    if (arguments.size() < 2) {
        *exception = createTypeError(context, "Must specify the destination ID and message ID as the first two arguments"_s);
        return JSValueMakeUndefined(context);
    }
    auto info = extractSyncIPCMessageInfo(context, arguments, exception);
    if (!info)
        return JSValueMakeUndefined(context);
    auto [destinationID, messageName, timeout] = *info;
    JSValueRef messageArguments = arguments.size() > 3 ? arguments[3] : nullptr;
    return jsSendSync(jsIPC->m_testedConnection.get(), destinationID, messageName, timeout, context, messageArguments, exception);
}

JSValueRef JSIPCConnection::waitForMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t rawArgumentCount, const JSValueRef rawArguments[], JSValueRef* exception)
{
    auto arguments = unsafeMakeSpan(rawArguments, rawArgumentCount);

    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    if (arguments.size() < 2) {
        *exception = createTypeError(context, "Must specify the destination ID and message ID as the first two arguments"_s);
        return JSValueMakeUndefined(context);
    }
    auto info = extractSyncIPCMessageInfo(context, arguments, exception);
    if (!info)
        return JSValueMakeUndefined(context);
    auto [destinationID, messageName, timeout] = *info;
    return jsWaitForMessage(jsIPC->m_testedConnection.get(), destinationID, messageName, timeout, context, exception);
}

JSValueRef JSIPCConnection::waitForAsyncReplyAndDispatchImmediately(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t rawArgumentCount, const JSValueRef rawArguments[], JSValueRef* exception)
{
    auto arguments = unsafeMakeSpan(rawArguments, rawArgumentCount);

    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    if (arguments.size() < 3) {
        *exception = createTypeError(context, "Must specify the message name, async reply ID and timeout as the first three arguments"_s);
        return JSValueMakeUndefined(context);
    }
    auto info = extractSyncIPCMessageInfo(context, arguments, exception);
    if (!info)
        return JSValueMakeUndefined(context);
    auto [destinationID, messageName, timeout] = *info;
    return jsWaitForAsyncReplyAndDispatchImmediately(jsIPC->m_testedConnection.get(), destinationID, messageName, timeout, context, exception);
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
        { "open", open, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "invalidate", invalidate, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "streamBuffer", streamBuffer, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "setSemaphores", setSemaphores, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "sendMessage", sendMessage, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "sendWithAsyncReply", sendWithAsyncReply, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "sendSyncMessage", sendSyncMessage, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "sendIPCStreamTesterSyncCrashOnZero", sendIPCStreamTesterSyncCrashOnZero, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "waitForMessage", waitForMessage, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "waitForAsyncReplyAndDispatchImmediately", waitForAsyncReplyAndDispatchImmediately, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { 0, 0, 0 }
    };
    return functions;
}

JSValueRef JSIPCStreamClientConnection::open(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef* exception)
{
    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    jsIPC->m_streamConnection->open(jsIPC->m_dummyMessageReceiver);
    return JSValueMakeUndefined(context);
}

JSValueRef JSIPCStreamClientConnection::invalidate(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef* exception)
{
    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    jsIPC->m_streamConnection->invalidate();
    return JSValueMakeUndefined(context);
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

JSValueRef JSIPCStreamClientConnection::setSemaphores(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t rawArgumentCount, const JSValueRef rawArguments[], JSValueRef* exception)
{
    auto arguments = unsafeMakeSpan(rawArguments, rawArgumentCount);

    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    RefPtr jsStreamConnection = toWrapped(context, thisObject);
    if (!jsStreamConnection) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }

    if (arguments.size() < 2) {
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
};

static std::optional<IPCStreamMessageInfo> extractIPCStreamMessageInfo(JSContextRef context, std::span<const JSValueRef> arguments, JSValueRef* exception)
{
    if (arguments.size() < 2) {
        *exception = createTypeError(context, "Must specify destination ID, message ID as the first two arguments"_s);
        return std::nullopt;
    }

    auto* globalObject = toJS(context);
    auto destinationID = destinationIDFromArgument(globalObject, arguments[0], exception);
    if (!destinationID)
        return std::nullopt;

    auto messageName = messageNameFromArgument(globalObject, arguments[1], exception);
    if (!messageName)
        return std::nullopt;

    return { { *destinationID, *messageName } };
}

bool JSIPCStreamClientConnection::prepareToSendOutOfStreamMessage(uint64_t destinationID, IPC::Timeout timeout)
{
    if (m_streamConnection->trySendDestinationIDIfNeeded(destinationID, timeout) != IPC::Error::NoError)
        return false;
    auto span = m_streamConnection->bufferForTesting().tryAcquire(timeout);
    if (!span)
        return false;
    m_streamConnection->sendProcessOutOfStreamMessage(WTFMove(*span));
    return true;
}

JSValueRef JSIPCStreamClientConnection::sendMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t rawArgumentCount, const JSValueRef rawArguments[], JSValueRef* exception)
{
    auto arguments = unsafeMakeSpan(rawArguments, rawArgumentCount);

    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    auto info = extractIPCStreamMessageInfo(context, arguments, exception);
    if (!info)
        return JSValueMakeUndefined(context);
    auto [destinationID, messageName] = *info;
    auto timeout = jsIPC->m_streamConnection->defaultTimeout();
    if (!jsIPC->prepareToSendOutOfStreamMessage(destinationID, timeout))
        return JSValueMakeUndefined(context);
    JSValueRef messageArguments = arguments.size() > 2 ? arguments[2] : nullptr;
    return jsSend(jsIPC->m_streamConnection->connectionForTesting(), destinationID, messageName, context, messageArguments, exception);
}

JSValueRef JSIPCStreamClientConnection::sendWithAsyncReply(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t rawArgumentCount, const JSValueRef rawArguments[], JSValueRef* exception)
{
    auto arguments = unsafeMakeSpan(rawArguments, rawArgumentCount);

    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    if (arguments.size() < 4) {
        *exception = createTypeError(context, "Must specify destination ID, message ID, messageArguments, callback as the first four arguments"_s);
        return JSValueMakeUndefined(context);
    }
    auto info = extractIPCStreamMessageInfo(context, arguments, exception);
    if (!info)
        return JSValueMakeUndefined(context);
    auto [destinationID, messageName] = *info;
    if (!messageReplyArgumentDescriptions(messageName)) {
        *exception = createError(context, "Message does not have a reply"_s);
        return JSValueMakeUndefined(context);
    }
    JSObjectRef callback = nullptr;
    if (JSValueIsObject(context, arguments[3])) {
        callback = JSValueToObject(context, arguments[3], exception);
        if (!JSObjectIsFunction(context, callback))
            callback = nullptr;
    }
    if (!callback) {
        *exception = createTypeError(context, "Must specify callback as the fifth argument"_s);
        return JSValueMakeUndefined(context);
    }
    auto timeout = jsIPC->m_streamConnection->defaultTimeout();
    if (!jsIPC->prepareToSendOutOfStreamMessage(destinationID, timeout)) {
        *exception = createError(context, "IPC error: prepare failed"_s);
        return JSValueMakeUndefined(context);
    }
    return jsSendWithAsyncReply(jsIPC->m_streamConnection->connectionForTesting(), destinationID, messageName, context, callback, arguments[2], exception);
}

JSValueRef JSIPCStreamClientConnection::sendSyncMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t rawArgumentCount, const JSValueRef rawArguments[], JSValueRef* exception)
{
    auto arguments = unsafeMakeSpan(rawArguments, rawArgumentCount);

    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    auto info = extractIPCStreamMessageInfo(context, arguments, exception);
    if (!info)
        return JSValueMakeUndefined(context);
    auto [destinationID, messageName] = *info;
    auto timeout = jsIPC->m_streamConnection->defaultTimeout();
    if (!jsIPC->prepareToSendOutOfStreamMessage(destinationID, timeout)) {
        *exception = createError(context, "IPC error: prepare failed"_s);
        return JSValueMakeUndefined(context);
    }
    JSValueRef messageArguments = arguments.size() > 2 ? arguments[2] : nullptr;
    return jsSendSync(jsIPC->m_streamConnection->connectionForTesting(), destinationID, messageName, timeout, context, messageArguments, exception);
}

// FIXME(http://webkit.org/b/237197): Cannot send arbitrary messages, so we hard-code this one to be able to send it.
JSValueRef JSIPCStreamClientConnection::sendIPCStreamTesterSyncCrashOnZero(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t rawArgumentCount, const JSValueRef rawArguments[], JSValueRef* exception)
{
    auto arguments = unsafeMakeSpan(rawArguments, rawArgumentCount);

    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());

    RefPtr jsStreamConnection = toWrapped(context, thisObject);
    if (!jsStreamConnection) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }

    if (arguments.size() < 2) {
        *exception = createTypeError(context, "Must specify destination ID, value as the first two arguments"_s);
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

    auto& streamConnection = jsStreamConnection->connection();
    enum JSIPCStreamTesterIdentifierType { };
    auto destination = ObjectIdentifier<JSIPCStreamTesterIdentifierType>(*destinationID);

    auto sendResult = streamConnection.sendSync(Messages::IPCStreamTester::SyncCrashOnZero(value), destination);
    if (!sendResult.succeeded()) {
        *exception = createTypeError(context, "sync send failed"_s);
        return JSValueMakeUndefined(context);
    }

    auto [resultValue] = sendResult.takeReply();
    return JSValueMakeNumber(context, resultValue);
}

JSValueRef JSIPCStreamClientConnection::waitForMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t rawArgumentCount, const JSValueRef rawArguments[], JSValueRef* exception)
{
    auto arguments = unsafeMakeSpan(rawArguments, rawArgumentCount);

    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    if (arguments.size() < 2) {
        *exception = createTypeError(context, "Must specify the destination ID and message ID as the first two arguments"_s);
        return JSValueMakeUndefined(context);
    }
    auto info = extractIPCStreamMessageInfo(context, arguments, exception);
    if (!info)
        return JSValueMakeUndefined(context);
    auto [destinationID, messageName] = *info;
    return jsWaitForMessage(jsIPC->m_streamConnection->connectionForTesting(), destinationID, messageName, jsIPC->m_streamConnection->defaultTimeout(), context, exception);
}

JSValueRef JSIPCStreamClientConnection::waitForAsyncReplyAndDispatchImmediately(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t rawArgumentCount, const JSValueRef rawArguments[], JSValueRef* exception)
{
    auto arguments = unsafeMakeSpan(rawArguments, rawArgumentCount);

    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    if (arguments.size() < 2) {
        *exception = createTypeError(context, "Must specify the message name, async reply ID as the first two arguments"_s);
        return JSValueMakeUndefined(context);
    }
    auto info = extractIPCStreamMessageInfo(context, arguments, exception);
    if (!info)
        return JSValueMakeUndefined(context);
    auto [destinationID, messageName] = *info;
    return jsWaitForAsyncReplyAndDispatchImmediately(jsIPC->m_streamConnection->connectionForTesting(), destinationID, messageName, jsIPC->m_streamConnection->defaultTimeout(), context, exception);
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

JSObjectRef JSIPCStreamServerConnectionHandle::createJSWrapper(JSContextRef context)
{
    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    JSObjectRef wrapperObject = JSObjectMake(toGlobalRef(globalObject), wrapperClass(), this);
    scope.clearException();
    return wrapperObject;
}

JSClassRef JSIPCStreamServerConnectionHandle::wrapperClass()
{
    static JSClassRef jsClass;
    if (!jsClass) {
        JSClassDefinition definition = kJSClassDefinitionEmpty;
        definition.className = "StreamServerConnectionHandle";
        definition.parentClass = nullptr;
        definition.staticValues = nullptr;
        definition.staticFunctions = staticFunctions();
        definition.initialize = initialize;
        definition.finalize = finalize;
        jsClass = JSClassCreate(&definition);
    }
    return jsClass;
}

inline JSIPCStreamServerConnectionHandle* JSIPCStreamServerConnectionHandle::unwrap(JSObjectRef object)
{
    return static_cast<JSIPCStreamServerConnectionHandle*>(JSObjectGetPrivate(object));
}

JSIPCStreamServerConnectionHandle* JSIPCStreamServerConnectionHandle::toWrapped(JSContextRef context, JSValueRef value)
{
    if (!context || !value || !JSValueIsObjectOfClass(context, value, wrapperClass()))
        return nullptr;
    return unwrap(JSValueToObject(context, value, nullptr));
}

void JSIPCStreamServerConnectionHandle::initialize(JSContextRef, JSObjectRef object)
{
    unwrap(object)->ref();
}

void JSIPCStreamServerConnectionHandle::finalize(JSObjectRef object)
{
    unwrap(object)->deref();
}

const JSStaticFunction* JSIPCStreamServerConnectionHandle::staticFunctions()
{
    static const JSStaticFunction functions[] = {
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

std::optional<SharedMemory::Handle> JSSharedMemory::createHandle(SharedMemory::Protection protection)
{
    return m_sharedMemory->createHandle(protection);
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

JSValueRef JSSharedMemory::readBytes(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t rawArgumentCount, const JSValueRef rawArguments[], JSValueRef* exception)
{
    auto arguments = unsafeMakeSpan(rawArguments, rawArgumentCount);

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
    if (!arguments.empty()) {
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

    if (arguments.size() > 1) {
        auto lengthValue = convertToUint64(toJS(globalObject, arguments[1]));
        if (!lengthValue) {
            *exception = createTypeError(context, "The second argument must be the number of bytes to read"_s);
            return JSValueMakeUndefined(context);
        }
        if (*lengthValue < length)
            length = *lengthValue;
    }

    auto arrayBuffer = JSC::ArrayBuffer::create(jsSharedMemory->m_sharedMemory->span().subspan(offset, length));
    JSC::JSArrayBuffer* jsArrayBuffer = nullptr;
    if (auto* structure = globalObject->arrayBufferStructure(arrayBuffer->sharingMode()))
        jsArrayBuffer = JSC::JSArrayBuffer::create(vm, structure, WTFMove(arrayBuffer));
    if (!jsArrayBuffer) {
        *exception = createTypeError(context, "Failed to create the array buffer for the read bytes"_s);
        return JSValueMakeUndefined(context);
    }

    return toRef(jsArrayBuffer);
}

static std::span<const uint8_t> arrayBufferSpanFromValueRef(JSContextRef context, JSTypedArrayType type, JSValueRef valueRef, JSValueRef* exception)
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

    return unsafeMakeSpan(static_cast<const uint8_t*>(buffer), length);
}

JSValueRef JSSharedMemory::writeBytes(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t rawArgumentCount, const JSValueRef rawArguments[], JSValueRef* exception)
{
    auto arguments = unsafeMakeSpan(rawArguments, rawArgumentCount);

    RefPtr jsSharedMemory = toWrapped(context, thisObject);
    if (!jsSharedMemory) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }

    auto type = arguments.size() > 0 ? JSValueGetTypedArrayType(context, arguments[0], exception) : kJSTypedArrayTypeNone;
    if (type == kJSTypedArrayTypeNone) {
        *exception = createTypeError(context, "The first argument must be an array buffer or a typed array"_s);
        return JSValueMakeUndefined(context);
    }

    auto span = arrayBufferSpanFromValueRef(context, type, arguments[0], exception);
    if (!span.data()) {
        *exception = createTypeError(context, "Could not read the buffer"_s);
        return JSValueMakeUndefined(context);
    }

    size_t offset = 0;
    size_t length = span.size();
    size_t sharedMemorySize = jsSharedMemory->m_sharedMemory->size();

    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);

    if (arguments.size() > 1) {
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

    if (arguments.size() > 2) {
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

    memcpySpan(jsSharedMemory->m_sharedMemory->mutableSpan().subspan(offset), span.first(length));

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

    IPC::StreamConnectionBuffer& buffer = jsStreamBuffer->m_streamConnection->m_streamConnection->bufferForTesting();
    std::span<uint8_t> span = buffer.headerForTesting();

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

    IPC::StreamConnectionBuffer& buffer = jsStreamBuffer->m_streamConnection->m_streamConnection->bufferForTesting();
    std::span<uint8_t> span = buffer.dataForTesting();

    return readBytes(context, nullptr, thisObject, argumentCount, arguments, exception, span);
}

JSValueRef JSIPCStreamConnectionBuffer::readBytes(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t rawArgumentCount, const JSValueRef rawArguments[], JSValueRef* exception, std::span<uint8_t> span)
{
    auto arguments = unsafeMakeSpan(rawArguments, rawArgumentCount);

    size_t offset = 0;
    size_t length = span.size();
    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    if (!arguments.empty()) {
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

    if (arguments.size() > 1) {
        auto lengthValue = convertToUint64(toJS(globalObject, arguments[1]));
        if (!lengthValue) {
            *exception = createTypeError(context, "The second argument must be the number of bytes to read"_s);
            return JSValueMakeUndefined(context);
        }
        if (*lengthValue < length)
            length = *lengthValue;
    }

    auto arrayBuffer = JSC::ArrayBuffer::create(span.subspan(offset, length));
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

    IPC::StreamConnectionBuffer& buffer = jsStreamBuffer->m_streamConnection->m_streamConnection->bufferForTesting();
    std::span<uint8_t> span = buffer.headerForTesting();

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

    IPC::StreamConnectionBuffer& buffer = jsStreamBuffer->m_streamConnection->m_streamConnection->bufferForTesting();
    std::span<uint8_t> span = buffer.dataForTesting();

    return writeBytes(context, nullptr, thisObject, argumentCount, arguments, exception, span);
}

JSValueRef JSIPCStreamConnectionBuffer::writeBytes(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t rawArgumentCount, const JSValueRef rawArguments[], JSValueRef* exception, std::span<uint8_t> span)
{
    auto arguments = unsafeMakeSpan(rawArguments, rawArgumentCount);

    auto type = arguments.size() > 0 ? JSValueGetTypedArrayType(context, arguments[0], exception) : kJSTypedArrayTypeNone;
    if (type == kJSTypedArrayTypeNone) {
        *exception = createTypeError(context, "The first argument must be an array buffer or a typed array"_s);
        return JSValueMakeUndefined(context);
    }

    auto data = arrayBufferSpanFromValueRef(context, type, arguments[0], exception);
    if (!data.data()) {
        *exception = createTypeError(context, "Could not read the buffer"_s);
        return JSValueMakeUndefined(context);
    }

    size_t offset = 0;
    size_t length = data.size();

    size_t sharedMemorySize = span.size();
    auto destinationData = span;

    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);

    if (arguments.size() > 1) {
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

    if (arguments.size() > 2) {
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

    memcpySpan(destinationData.subspan(offset), data.first(length));
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
        { "connectionForProcessTarget", connectionForProcessTarget, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "addIncomingMessageListener", addIncomingMessageListener, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "addOutgoingMessageListener", addOutgoingMessageListener, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "sendMessage", sendMessage, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "sendSyncMessage", sendSyncMessage, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "waitForMessage", waitForMessage, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "createConnectionPair", createConnectionPair, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "createStreamClientConnection", createStreamClientConnection, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "createSemaphore", createSemaphore, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "createSharedMemory", createSharedMemory, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "addTesterReceiver", addTesterReceiver, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
        { "removeTesterReceiver", removeTesterReceiver, kJSPropertyAttributeDontDelete | kJSPropertyAttributeReadOnly },
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

RefPtr<JSIPCConnection> JSIPC::processTargetFromArgument(JSC::JSGlobalObject* globalObject, JSValueRef valueRef, JSValueRef* exception)
{
    auto scope = DECLARE_CATCH_SCOPE(globalObject->vm());
    auto name = toJS(globalObject, valueRef).toWTFString(globalObject);
    if (scope.exception())
        return nullptr;

    if (name == processTargetNameUI) {
        RefPtr connection = WebProcess::singleton().parentProcessConnection();
        if (!m_uiConnection || m_uiConnection->connection().ptr() != connection)
            m_uiConnection = JSIPCConnection::create(connection.releaseNonNull());
        return m_uiConnection;
    }
#if ENABLE(GPU_PROCESS)
    if (name == processTargetNameGPU) {
        RefPtr connection = &WebProcess::singleton().ensureGPUProcessConnection().connection();
        if (!m_gpuConnection || m_gpuConnection->connection().ptr() != connection)
            m_gpuConnection = JSIPCConnection::create(connection.releaseNonNull());
        return m_gpuConnection;
    }
#endif
    if (name == processTargetNameNetworking) {
        RefPtr connection = &WebProcess::singleton().ensureNetworkProcessConnection().connection();
        if (!m_networkConnection || m_networkConnection->connection().ptr() != connection)
            m_networkConnection = JSIPCConnection::create(connection.releaseNonNull());
        return m_networkConnection;
    }

    *exception = toRef(JSC::createTypeError(globalObject, "Target process must be UI, GPU, or Networking"_s));
    return nullptr;
}

void JSIPC::addMessageListener(JSMessageListener::Type type, JSContextRef context, JSObjectRef thisObject, size_t rawArgumentCount, const JSValueRef rawArguments[], JSValueRef* exception)
{
    auto arguments = unsafeMakeSpan(rawArguments, rawArgumentCount);

    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return;
    }
    if (arguments.size() < 1) {
        *exception = createTypeError(context, "Must specify the target process as the first argument"_s);
        return;
    }
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    auto connection = jsIPC->processTargetFromArgument(globalObject, arguments[0], exception);
    if (!connection)
        return;

    RefPtr<JSMessageListener> listener;
    if (arguments.size() >= 2 && JSValueIsObject(context, arguments[1])) {
        auto listenerObjectRef = JSValueToObject(context, arguments[1], exception);
        if (JSObjectIsFunction(context, listenerObjectRef))
            listener = JSMessageListener::create(*jsIPC, type, globalObject, listenerObjectRef);
    }

    if (!listener) {
        *exception = createTypeError(context, "Must specify a callback function as the second argument"_s);
        return;
    }

    connection->connection()->addMessageObserver(*listener);
    jsIPC->m_messageListeners.append(listener.releaseNonNull());
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

static std::optional<IPC::MessageName> messageNameFromArgument(JSC::JSGlobalObject* globalObject, JSValueRef valueRef, JSValueRef* exception)
{
    auto result = convertToUint64(toJS(globalObject, valueRef));
    if (!result) {
        *exception = toRef(JSC::createTypeError(globalObject, "messageName must be an integer"_s));
        return std::nullopt;
    }
    return static_cast<IPC::MessageName>(result.value());

}

static bool encodeTypedArray(IPC::Encoder& encoder, JSContextRef context, JSValueRef valueRef, JSTypedArrayType type, JSValueRef* exception)
{
    ASSERT(type != kJSTypedArrayTypeNone);

    auto span = arrayBufferSpanFromValueRef(context, type, valueRef, exception);
    if (!span.data())
        return false;

    encoder.encodeSpan(span);
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
template <typename T>
std::optional<T> getObjectIdentifierFromProperty(JSC::JSGlobalObject* globalObject, JSC::JSObject* jsObject, ASCIILiteral propertyName, JSC::CatchScope& scope)
{
    auto jsPropertyValue = jsObject->get(globalObject, JSC::Identifier::fromString(globalObject->vm(), propertyName));
    if (scope.exception())
        return std::nullopt;
    auto number = convertToUint64(jsPropertyValue);
    if (!number)
        return std::nullopt;
    return std::optional<T> { *number };
}

#endif

static bool encodeSharedMemory(IPC::Encoder& encoder, JSC::JSGlobalObject* globalObject, JSC::JSObject* jsObject, JSC::CatchScope& scope)
{
    auto jsSharedMemoryValue = jsObject->get(globalObject, JSC::Identifier::fromString(globalObject->vm(), "value"_s));
    if (scope.exception())
        return false;
    RefPtr jsSharedMemory = JSSharedMemory::toWrapped(toRef(globalObject), toRef(globalObject, jsSharedMemoryValue));
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
    auto handle = jsSharedMemory->createHandle(protection);
    if (!handle)
        return false;
    encoder << WTFMove(*handle);
    return true;
}

static bool encodeFrameInfoData(IPC::Encoder& encoder, JSC::JSGlobalObject* globalObject, JSC::JSObject* jsObject, JSC::CatchScope& scope)
{
    auto jsIPCValue = jsObject->get(globalObject, JSC::Identifier::fromString(globalObject->vm(), "value"_s));
    if (scope.exception())
        return false;
    RefPtr jsIPC = JSIPC::toWrapped(toRef(globalObject), toRef(globalObject, jsIPCValue));
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
    RefPtr jsIPCStreamConnectionBuffer = JSIPCStreamConnectionBuffer::toWrapped(toRef(globalObject), toRef(globalObject, jsValue));
    if (!jsIPCStreamConnectionBuffer)
        return false;

    jsIPCStreamConnectionBuffer->encode(encoder);
    return true;
}

static bool encodeStreamServerConnectionHandle(IPC::Encoder& encoder, JSC::JSGlobalObject* globalObject, JSC::JSValue jsValue, JSC::CatchScope& scope)
{
    RefPtr JSIPCStreamServerConnectionHandle = JSIPCStreamServerConnectionHandle::toWrapped(toRef(globalObject), toRef(globalObject, jsValue));
    if (!JSIPCStreamServerConnectionHandle)
        return false;

    JSIPCStreamServerConnectionHandle->encode(encoder);
    return true;
}

static bool encodeSemaphore(IPC::Encoder& encoder, JSC::JSGlobalObject* globalObject, JSC::JSValue jsValue, JSC::CatchScope& scope)
{
    RefPtr jsIPCSemaphore = JSIPCSemaphore::toWrapped(toRef(globalObject), toRef(globalObject, jsValue));
    if (!jsIPCSemaphore)
        return false;

    jsIPCSemaphore->encode(encoder);
    return true;
}

static bool encodeConnectionHandle(IPC::Encoder& encoder, JSC::JSGlobalObject* globalObject, JSC::JSValue jsValue, JSC::CatchScope& scope)
{
    RefPtr JSIPCConnectionHandle = JSIPCConnectionHandle::toWrapped(toRef(globalObject), toRef(globalObject, jsValue));
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

} // namespace WebKit::IPCTestingAPI

namespace IPC {

template<> struct ArgumentCoder<WebKit::IPCTestingAPI::VectorEncodeHelper> {
    static void encode(Encoder& encoder, const WebKit::IPCTestingAPI::VectorEncodeHelper& helper)
    {
        helper.encode(encoder);
    }
};

} // namespace IPC

namespace WebKit::IPCTestingAPI {

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
    if (type == "StreamServerConnectionHandle"_s) {
        if (!encodeStreamServerConnectionHandle(encoder, globalObject, jsValue, scope)) {
            *exception = createTypeError(context, "Failed to convert StreamServerConnectionHandle"_s);
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

    if (type == "FrameID"_s) {
        uint64_t frameIdentifier = jsValue.get(globalObject, 0u).toBigUInt64(globalObject);
        uint64_t processIdentifier = jsValue.get(globalObject, 1u).toBigUInt64(globalObject);
        if (!ObjectIdentifier<WebCore::FrameIdentifierType>::isValidIdentifier(frameIdentifier) || !ObjectIdentifier<WebCore::ProcessIdentifierType>::isValidIdentifier(processIdentifier))
            return false;
        encoder << WebCore::FrameIdentifier {
            ObjectIdentifier<WebCore::FrameIdentifierType>(frameIdentifier),
            ObjectIdentifier<WebCore::ProcessIdentifierType>(processIdentifier)
        };
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

    if (decoder.messageName() == IPC::MessageName::CancelSyncMessageReply) {
        throwException(globalObject, scope, JSC::createTypeError(globalObject, "Receiver cancelled the reply due to invalid destination or deserialization error"_s));
        return nullptr;
    }

    auto arrayBuffer = JSC::ArrayBuffer::create(decoder.span());
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

JSValueRef JSIPC::connectionForProcessTarget(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    if (argumentCount < 1) {
        *exception = createTypeError(context, "Must specify the target process"_s);
        return JSValueMakeUndefined(context);
    }
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    RefPtr connection = jsIPC->processTargetFromArgument(globalObject, arguments[0], exception);
    if (!connection)
        return JSValueMakeUndefined(context);
    return connection->createJSWrapper(context);
}

JSValueRef JSIPC::sendMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t rawArgumentCount, const JSValueRef rawArguments[], JSValueRef* exception)
{
    auto arguments = unsafeMakeSpan(rawArguments, rawArgumentCount);

    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    if (arguments.size() < 3) {
        *exception = createTypeError(context, "Must specify the target process, destination ID, and message ID as the first three arguments"_s);
        return JSValueMakeUndefined(context);
    }
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    auto connection = jsIPC->processTargetFromArgument(globalObject, arguments[0], exception);
    if (!connection)
        return JSValueMakeUndefined(context);
    auto destinationID = destinationIDFromArgument(globalObject, arguments[1], exception);
    if (!destinationID)
        return JSValueMakeUndefined(context);
    auto messageName = messageNameFromArgument(globalObject, arguments[2], exception);
    if (!messageName)
        return JSValueMakeUndefined(context);
    JSValueRef messageArguments = arguments.size() > 3 ? arguments[3] : nullptr;
    return jsSend(connection->connection().get(), *destinationID, *messageName, context, messageArguments, exception);
}

JSValueRef JSIPC::waitForMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t rawArgumentCount, const JSValueRef rawArguments[], JSValueRef* exception)
{
    auto arguments = unsafeMakeSpan(rawArguments, rawArgumentCount);

    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    if (arguments.size() < 4) {
        *exception = createTypeError(context, "Must specify the target process, destination ID, and message ID, timeout as the first four arguments"_s);
        return JSValueMakeUndefined(context);
    }
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    auto connection = jsIPC->processTargetFromArgument(globalObject, arguments[0], exception);
    if (!connection)
        return JSValueMakeUndefined(context);
    auto info = extractSyncIPCMessageInfo(context, arguments.subspan(1), exception);
    if (!info)
        return JSValueMakeUndefined(context);
    auto [destinationID, messageName, timeout] = *info;
    return jsWaitForMessage(connection->connection().get(), destinationID, messageName, timeout, context, exception);
}

JSValueRef JSIPC::sendSyncMessage(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t rawArgumentCount, const JSValueRef rawArguments[], JSValueRef* exception)
{
    auto arguments = unsafeMakeSpan(rawArguments, rawArgumentCount);

    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }
    if (arguments.size() < 4) {
        *exception = createTypeError(context, "Must specify the target process, destination ID, and message ID, timeout as the first four arguments"_s);
        return JSValueMakeUndefined(context);
    }
    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    auto connection = jsIPC->processTargetFromArgument(globalObject, arguments[0], exception);
    if (!connection)
        return JSValueMakeUndefined(context);
    auto info = extractSyncIPCMessageInfo(context, arguments.subspan(1), exception);
    if (!info)
        return JSValueMakeUndefined(context);
    auto [destinationID, messageName, timeout] = *info;
    JSValueRef messageArguments = arguments.size() > 4 ? arguments[4] : nullptr;
    return jsSendSync(connection->connection().get(), destinationID, messageName, timeout, context, messageArguments, exception);
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

JSValueRef JSIPC::createStreamClientConnection(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t rawArgumentCount, const JSValueRef rawArguments[], JSValueRef* exception)
{
    auto arguments = unsafeMakeSpan(rawArguments, rawArgumentCount);

    auto* globalObject = toJS(context);
    JSC::JSLockHolder lock(globalObject->vm());
    RefPtr jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = createTypeError(context, "Wrong type"_s);
        return JSValueMakeUndefined(context);
    }

    if (arguments.size() < 1) {
        *exception = createTypeError(context, "Must specify the buffer size as the first argument"_s);
        return JSValueMakeUndefined(context);
    }

    auto bufferSizeLog2 = arguments.size() > 0 ? convertToUint64(toJS(globalObject, arguments[0])) : std::optional<uint64_t> { };
    if (!bufferSizeLog2) {
        *exception = createTypeError(context, "Must specify the size"_s);
        return JSValueMakeUndefined(context);
    }
    auto defaultTimeoutDuration = arguments.size() > 1 ? convertToDouble(toJS(globalObject, arguments[1])) : std::optional<double> { };
    if (!defaultTimeoutDuration) {
        *exception = createTypeError(context, "Must specify the defaultTimeoutDuration"_s);
        return JSValueMakeUndefined(context);
    }

    auto connectionPair = IPC::StreamClientConnection::create(*bufferSizeLog2, Seconds(*defaultTimeoutDuration));
    if (!connectionPair) {
        *exception = createError(context, "Failed to create the connection"_s);
        return JSValueMakeUndefined(context);
    }
    auto& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);
    JSC::JSObject* connectionPairObject = JSC::constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));
    int index = 0;
    auto jsValue = toJS(globalObject, JSIPCStreamClientConnection::create(*jsIPC, WTFMove(connectionPair->streamConnection))->createJSWrapper(context));
    connectionPairObject->putDirectIndex(globalObject, index++, jsValue);
    RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));
    jsValue = toJS(globalObject, JSIPCStreamServerConnectionHandle::create(WTFMove(connectionPair->connectionHandle))->createJSWrapper(context));
    connectionPairObject->putDirectIndex(globalObject, index++, jsValue);
    RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));
    return toRef(vm, connectionPairObject);
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

JSValueRef JSIPC::addTesterReceiver(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);

    auto* jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = toRef(JSC::createTypeError(toJS(context), "Wrong type"_s));
        return JSValueMakeUndefined(context);
    }
    // Currently supports only UI process, as there's no uniform way to add message receivers.
    WebProcess::singleton().addMessageReceiver(Messages::IPCTesterReceiver::messageReceiverName(), jsIPC->m_testerProxy.get());
    return JSValueMakeUndefined(context);
}


JSValueRef JSIPC::removeTesterReceiver(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    auto* jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
        *exception = toRef(JSC::createTypeError(toJS(context), "Wrong type"_s));
        return JSValueMakeUndefined(context);
    }

    WebProcess::singleton().removeMessageReceiver(Messages::IPCTesterReceiver::messageReceiverName());
    return JSValueMakeUndefined(context);
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
            JSC::JSObject* entry = constructEmptyObject(globalObject, globalObject->objectPrototype());
            RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));

            entry->putDirect(vm, JSC::Identifier::fromString(vm, "type"_s), JSC::jsString(vm, String(type.members[i].type)));
            RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));

            entry->putDirect(vm, JSC::Identifier::fromString(vm, "name"_s), JSC::jsString(vm, String(type.members[i].name)));
            RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));

            membersArray->putDirectIndex(globalObject, i, entry);
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

        auto validValuesArray = WTF::map(enumeration.validValues, [&](auto& validValue) -> JSValueRef {
            return JSValueMakeNumber(context, validValue);
        });
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
        array->putDirectIndex(globalObject, i, toJS(globalObject, JSValueMakeString(context, adopt(JSStringCreateWithUTF8CString(identifiers[i].characters())).get())));
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
        return webPage->visitedLinkTableID().toUInt64();
    });
}

JSValueRef JSIPC::frameID(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef* exception)
{
    auto* globalObject = toJS(context);
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSC::JSObject* array = JSC::constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, JSValueMakeUndefined(context));

    RefPtr<JSIPC> wrapped = toWrapped(context, thisObject);
    if (!wrapped) {
        *exception = toRef(JSC::createTypeError(toJS(context), "Wrong type"_s));
        return JSValueMakeUndefined(context);
    }

    auto frameID = wrapped->m_webFrame->frameID();
    array->putDirectIndex(globalObject, 0, JSC::JSBigInt::createFrom(globalObject, frameID.object().toUInt64()));
    array->putDirectIndex(globalObject, 1, JSC::JSBigInt::createFrom(globalObject, frameID.processIdentifier().toUInt64()));
    return toRef(vm, array);
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

    auto* jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
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

    auto* jsIPC = toWrapped(context, thisObject);
    if (!jsIPC) {
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

WTF_MAKE_TZONE_ALLOCATED_IMPL(JSMessageListener);

JSMessageListener::JSMessageListener(JSIPC& jsIPC, Type type, JSC::JSGlobalObject* globalObject, JSObjectRef callback)
    : m_jsIPC(jsIPC)
    , m_type(type)
    , m_globalObject(JSC::jsCast<WebCore::JSDOMGlobalObject*>(globalObject))
    , m_callback(callback)
{
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);

    auto catchScope = DECLARE_CATCH_SCOPE(vm);

    // We can't retain the global context here as that would cause a leak
    // since this object is supposed to live as long as the global object is alive.
    JSC::PrivateName uniquePrivateName(JSC::PrivateName::Description, "listener"_s);
    globalObject->putDirect(vm, uniquePrivateName, toJS(globalObject, callback));

    UNUSED_PARAM(catchScope);
}

void JSMessageListener::didReceiveMessage(const IPC::Decoder& decoder)
{
    if (m_type != Type::Incoming)
        return;

    auto* globalObject = m_globalObject.get();
    if (!globalObject)
        return;

    RELEASE_ASSERT(m_jsIPC);
    Ref protectOwnerOfThis = *m_jsIPC;
    auto context = toRef(globalObject);
    JSC::JSLockHolder lock(globalObject->vm());

    auto mutableDecoder = IPC::Decoder::create(decoder.span(), { });
    auto* description = jsDescriptionFromDecoder(globalObject, *mutableDecoder);

    JSValueRef arguments[] = { description ? toRef(globalObject, description) : JSValueMakeUndefined(context) };
    JSObjectCallAsFunction(context, m_callback, m_callback, std::size(arguments), arguments, nullptr);
}

void JSMessageListener::willSendMessage(const IPC::Encoder& encoder, OptionSet<IPC::SendOption>)
{
    if (m_type != Type::Outgoing)
        return;

    RELEASE_ASSERT(m_jsIPC);
    Ref protectOwnerOfThis = *m_jsIPC;

    auto decoder = IPC::Decoder::create(encoder.span(), { });
    RunLoop::main().dispatch([this, protectOwnerOfThis = WTFMove(protectOwnerOfThis), decoder = WTFMove(decoder)] {
        auto* globalObject = m_globalObject.get();
        if (!globalObject)
            return;

        auto context = toRef(globalObject);
        JSC::JSLockHolder lock(globalObject->vm());

        auto* description = jsDescriptionFromDecoder(globalObject, *decoder);

        JSValueRef arguments[] = { description ? toRef(globalObject, description) : JSValueMakeUndefined(context) };
        JSObjectCallAsFunction(context, m_callback, m_callback, std::size(arguments), arguments, nullptr);
    });
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
        jsResult->putDirect(vm, JSC::Identifier::fromString(vm, "syncRequestID"_s), JSC::JSValue(decoder.syncRequestID().toUInt64()));
        RETURN_IF_EXCEPTION(scope, nullptr);
    }
    auto arrayBuffer = JSC::ArrayBuffer::create(decoder.span());
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

    if (!decoder.isSyncMessage() && messageReplyArgumentDescriptions(decoder.messageName())) {
        if (auto asyncReplyID = decoder.decode<IPC::Connection::AsyncReplyID>()) {
            jsResult->putDirect(vm, JSC::Identifier::fromString(vm, "listenerID"_s), JSC::JSValue(asyncReplyID->toUInt64()));
            RETURN_IF_EXCEPTION(scope, nullptr);
        }
    }
    return jsResult;
}

void inject(WebPage& webPage, WebFrame& webFrame, WebCore::DOMWrapperWorld& world)
{
    auto* globalObject = webFrame.coreLocalFrame()->script().globalObject(world);

    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    auto wrapped = JSIPC::create(webPage, webFrame);
    JSObjectRef wrapperObject = JSObjectMake(toGlobalRef(globalObject), JSIPC::wrapperClass(), wrapped.ptr());
    globalObject->putDirect(vm, JSC::Identifier::fromString(vm, "IPC"_s), toJS(globalObject, wrapperObject));

    scope.clearException();
}

} // namespace WebKit::IPCTestingAPI

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

template<> JSC::JSValue jsValueForDecodedArgumentValue(JSC::JSGlobalObject* globalObject, WebCore::SharedMemory::Handle&& value)
{
    using SharedMemory = WebCore::SharedMemory;
    using Protection = WebCore::SharedMemory::Protection;

    auto dataSize = value.size();
    auto protection = Protection::ReadWrite;
    auto sharedMemory = SharedMemory::map(WTFMove(value), protection);
    if (!sharedMemory) {
        protection = Protection::ReadOnly;
        sharedMemory = SharedMemory::map(WTFMove(value), protection);
        if (!sharedMemory)
            return JSC::JSValue();
    }

    auto& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* object = JSC::constructEmptyObject(globalObject, globalObject->objectPrototype());
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());
    object->putDirect(vm, JSC::Identifier::fromString(vm, "type"_s), JSC::jsNontrivialString(vm, "SharedMemory"_s));
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());

    auto jsValue = toJS(globalObject, WebKit::IPCTestingAPI::JSSharedMemory::create(sharedMemory.releaseNonNull())->createJSWrapper(toRef(globalObject)));
    object->putDirect(vm, JSC::Identifier::fromString(vm, "value"_s), jsValue);
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());

    object->putDirect(vm, JSC::Identifier::fromString(vm, "dataSize"_s), JSC::JSValue(dataSize));
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());

    object->putDirect(vm, JSC::Identifier::fromString(vm, "protection"_s), JSC::jsNontrivialString(vm, protection == Protection::ReadWrite ? "ReadWrite"_s : "ReadOnly"_s));
    RETURN_IF_EXCEPTION(scope, JSC::JSValue());

    return object;
}

} // namespace IPC

#endif
