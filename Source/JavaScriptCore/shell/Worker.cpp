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
#include "Worker.h"

#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "StructuredClone.h"
#include "WorkerChannel.h"
#include <wtf/StdLibExtras.h>
#include <wtf/Threading.h>

namespace JSCShell {

using namespace JSC;

bool Worker::start(ASCIILiteral threadName, WTF::Function<void()>&& body)
{
    RefPtr thread = Thread::tryCreate(threadName, WTF::move(body));
    if (!thread)
        return false;
    thread->detach();
    return true;
}

namespace {

class JSCWorker final : public JSDestructibleObject {
    using Base = JSDestructibleObject;
public:
    template<typename CellType, SubspaceAccess>
    NODELETE static CompleteSubspace* subspaceFor(VM& vm)
    {
        return &vm.destructibleObjectSpace();
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static JSCWorker* create(VM& vm, Structure* structure, Ref<WorkerChannel>&& channel)
    {
        JSCWorker* worker = new (NotNull, allocateCell<JSCWorker>(vm)) JSCWorker(vm, structure, WTF::move(channel));
        worker->finishCreation(vm);
        worker->putDirect(vm, Identifier::fromString(vm, "onmessage"_s), jsNull());
        return worker;
    }

    static void destroy(JSCell* cell)
    {
        static_cast<JSCWorker*>(cell)->JSCWorker::~JSCWorker();
    }

    WorkerChannel& channel() const { return m_channel; }

    DECLARE_INFO;

private:
    JSCWorker(VM& vm, Structure* structure, Ref<WorkerChannel>&& channel)
        : Base(vm, structure)
        , m_channel(WTF::move(channel))
    {
    }

    const Ref<WorkerChannel> m_channel;
};

const ClassInfo JSCWorker::s_info = { "Worker"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSCWorker) };

} // namespace

static JSC_DECLARE_HOST_FUNCTION(functionCallWorker);
static JSC_DECLARE_HOST_FUNCTION(functionConstructWorker);
static JSC_DECLARE_HOST_FUNCTION(functionWorkerPostMessage);
static JSC_DECLARE_HOST_FUNCTION(functionWorkerTerminate);

JSFunction* createWorkerConstructor(VM& vm, JSGlobalObject* globalObject)
{
    constexpr unsigned dontEnum = static_cast<unsigned>(PropertyAttribute::DontEnum);
    JSFunction* constructor = JSFunction::create(vm, globalObject, 1, "Worker"_s, functionCallWorker, ImplementationVisibility::Public, NoIntrinsic, functionConstructWorker);
    JSObject* prototype = constructEmptyObject(globalObject);
    prototype->putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "postMessage"_s), 1, functionWorkerPostMessage, ImplementationVisibility::Public, NoIntrinsic, dontEnum);
    prototype->putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "terminate"_s), 0, functionWorkerTerminate, ImplementationVisibility::Public, NoIntrinsic, dontEnum);
    prototype->putDirect(vm, vm.propertyNames->constructor, constructor, dontEnum);
    constructor->putDirect(vm, vm.propertyNames->prototype, prototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    return constructor;
}

JSC_DEFINE_HOST_FUNCTION(functionCallWorker, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return throwVMTypeError(globalObject, scope, "Worker constructor cannot be called without 'new'"_s);
}

JSC_DEFINE_HOST_FUNCTION(functionConstructWorker, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    WorkerScript script;
    String scriptArgument = callFrame->argument(0).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    script.callerURL = callFrame->callerSourceOrigin(vm).url();

    bool isInlineSource = false;
    JSValue options = callFrame->argument(1);
    if (!options.isUndefinedOrNull()) {
        if (!options.isObject())
            return throwVMTypeError(globalObject, scope, "Worker options must be an object."_s);

        auto getStringOption = [&](ASCIILiteral name) -> String {
            JSValue value = asObject(options)->get(globalObject, Identifier::fromString(vm, name));
            RETURN_IF_EXCEPTION(scope, { });
            if (value.isUndefined())
                return { };
            RELEASE_AND_RETURN(scope, value.toWTFString(globalObject));
        };

        String type = getStringOption("type"_s);
        RETURN_IF_EXCEPTION(scope, { });
        if (type == "module"_s)
            script.isModule = true;
        else if (!type.isNull() && type != "classic"_s)
            return throwVMTypeError(globalObject, scope, "Worker type must be 'classic' or 'module'."_s);

        String sourceKind = getStringOption("source"_s);
        RETURN_IF_EXCEPTION(scope, { });
        if (sourceKind == "string"_s)
            isInlineSource = true;
        else if (!sourceKind.isNull() && sourceKind != "file"_s)
            return throwVMTypeError(globalObject, scope, "Worker source must be 'file' or 'string'."_s);
    }

    if (isInlineSource)
        script.script = InlineSource { WTF::move(scriptArgument) };
    else
        script.script = ScriptPath { WTF::move(scriptArgument) };

    JSValue prototype = asObject(callFrame->newTarget())->get(globalObject, vm.propertyNames->prototype);
    RETURN_IF_EXCEPTION(scope, { });
    if (!prototype.isObject()) {
        // GetPrototypeFromConstructor falls back to the intrinsic Worker.prototype, not Object.prototype.
        prototype = asObject(callFrame->jsCallee())->get(globalObject, vm.propertyNames->prototype);
        RETURN_IF_EXCEPTION(scope, { });
    }

    Ref channel = WorkerChannel::create();
    JSCWorker* worker = JSCWorker::create(vm, JSCWorker::createStructure(vm, globalObject, prototype), channel.copyRef());
    channel->open(WorkerChannel::Side::Parent, vm, worker);

    startWorkerThread(globalObject, WTF::move(script), channel.copyRef());
    if (scope.exception()) [[unlikely]] {
        channel->close(WorkerChannel::Side::Parent);
        return { };
    }
    return JSValue::encode(worker);
}

// HTML gives postMessage() two overloads, told apart the way WebIDL tells a sequence from a
// dictionary: an iterable second argument is the transfer list itself, anything else is a
// StructuredSerializeOptions holding one.
static JSValue transferListFromPostMessageArgument(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (value.isUndefinedOrNull())
        return jsUndefined();

    JSObject* options = value.getObject();
    if (!options) {
        throwTypeError(globalObject, scope, "postMessage second argument must be a transfer list or an options object"_s);
        return { };
    }

    JSValue iteratorMethod = options->get(globalObject, vm.propertyNames->iteratorSymbol);
    RETURN_IF_EXCEPTION(scope, { });
    // serializeMessage() looks Symbol.iterator up a second time, so an accessor installed there
    // runs twice. WebIDL bindings reuse the method found here.
    if (!iteratorMethod.isUndefinedOrNull())
        return value;

    RELEASE_AND_RETURN(scope, options->get(globalObject, Identifier::fromString(vm, "transfer"_s)));
}

JSC_DEFINE_HOST_FUNCTION(functionWorkerPostMessage, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* worker = dynamicDowncast<JSCWorker>(callFrame->thisValue());
    if (!worker)
        return throwVMTypeError(globalObject, scope, "Worker.prototype.postMessage called on an object that is not a Worker"_s);

    JSValue transfer = transferListFromPostMessageArgument(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    RefPtr message = serializeMessage(globalObject, callFrame->argument(0), transfer, "postMessage"_s);
    RETURN_IF_EXCEPTION(scope, { });
    worker->channel().post(WorkerChannel::Side::Parent, message.releaseNonNull());
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(functionWorkerTerminate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* worker = dynamicDowncast<JSCWorker>(callFrame->thisValue());
    if (!worker)
        return throwVMTypeError(globalObject, scope, "Worker.prototype.terminate called on an object that is not a Worker"_s);

    worker->channel().terminate();
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(functionWorkerScopePostMessage, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue transfer = transferListFromPostMessageArgument(globalObject, callFrame->argument(1));
    RETURN_IF_EXCEPTION(scope, { });

    RefPtr message = serializeMessage(globalObject, callFrame->argument(0), transfer, "postMessage"_s);
    RETURN_IF_EXCEPTION(scope, { });
    protect(WorkerChannel::currentWorkerSide())->post(WorkerChannel::Side::Worker, message.releaseNonNull());
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(functionWorkerScopeClose, (JSGlobalObject*, CallFrame*))
{
    protect(WorkerChannel::currentWorkerSide())->close(WorkerChannel::Side::Worker);
    return JSValue::encode(jsUndefined());
}

} // namespace JSCShell
