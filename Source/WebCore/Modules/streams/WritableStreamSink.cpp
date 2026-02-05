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
#include "WritableStreamSink.h"

#include "JSWritableStreamDefaultController.h"
#include "JSWritableStreamSink.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/IdentifierInlines.h>
#include <JavaScriptCore/JSObjectInlines.h>
#include <JavaScriptCore/TopExceptionScope.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

class WritableStreamDefaultController {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(WritableStreamDefaultController);
public:
    explicit WritableStreamDefaultController(JSWritableStreamDefaultController* controller)
        : m_jsController(controller)
    {
    }

    void errorIfNeeded(JSC::JSGlobalObject&, JSC::JSValue);
    JSWritableStreamDefaultController* jsController() { return m_jsController; }

private:
    // The owner of WritableStreamDefaultController is responsible to keep uncollected the JSWritableStreamDefaultController.
    JSWritableStreamDefaultController* m_jsController { nullptr };
};

static bool invokeWritableStreamDefaultControllerFunction(JSC::JSGlobalObject& lexicalGlobalObject, const JSC::Identifier& identifier, const JSC::MarkedArgumentBuffer& arguments)
{
    JSC::VM& vm = lexicalGlobalObject.vm();
    JSC::JSLockHolder lock(vm);

    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
    auto function = lexicalGlobalObject.get(&lexicalGlobalObject, identifier);

    EXCEPTION_ASSERT(!scope.exception() || vm.hasPendingTerminationException());
    RETURN_IF_EXCEPTION(scope, false);

    ASSERT(function.isCallable());

    auto callData = JSC::getCallData(function);
    call(&lexicalGlobalObject, function, callData, JSC::jsUndefined(), arguments);
    EXCEPTION_ASSERT(!scope.exception() || vm.hasPendingTerminationException());
    return !scope.exception();
}

WritableStreamSink::WritableStreamSink() = default;
WritableStreamSink::~WritableStreamSink() = default;

void WritableStreamSink::start(std::unique_ptr<WritableStreamDefaultController>&& controller)
{
    m_controller = WTF::move(controller);
}

void WritableStreamSink::errorIfNeeded(JSC::JSGlobalObject& globalObject, JSC::JSValue error)
{
    Ref vm = globalObject.vm();
    JSC::JSLockHolder lock(vm.get());

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(m_controller->jsController());
    arguments.append(error);
    ASSERT(!arguments.hasOverflowed());

    auto* clientData = downcast<JSVMClientData>(vm->clientData);
    auto& privateName = clientData->builtinFunctions().writableStreamInternalsBuiltins().writableStreamDefaultControllerErrorIfNeededPrivateName();

    invokeWritableStreamDefaultControllerFunction(globalObject, privateName, arguments);
}

JSC::JSValue JSWritableStreamSink::start(JSC::JSGlobalObject& globalObject, JSC::CallFrame& callFrame)
{
    Ref vm = globalObject.vm();

    ASSERT(callFrame.argumentCount());
    JSWritableStreamDefaultController* controller = jsDynamicCast<JSWritableStreamDefaultController*>(callFrame.uncheckedArgument(0));
    ASSERT(controller);

    m_controller.set(vm, this, controller);

    Ref { wrapped() }->start(makeUnique<WritableStreamDefaultController>(controller));

    return JSC::jsUndefined();
}

JSC::JSValue JSWritableStreamSink::controller(JSC::JSGlobalObject&) const
{
    ASSERT_NOT_REACHED();
    return JSC::jsUndefined();
}


} // namespace WebCore
