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
 * THIS SOFTWARE IS PROVIDED BY CANON INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL CANON INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WritableStream.h"

#include "Exception.h"
#include "ExceptionCode.h"
#include "JSDOMConvertSequences.h"
#include "JSWritableStreamSink.h"
#include "WebCoreJSClientData.h"


namespace WebCore {
using namespace JSC;

ExceptionOr<Ref<WritableStream>> WritableStream::create(JSC::JSGlobalObject& lexicalGlobalObject, RefPtr<WritableStreamSink>&& sink)
{
    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto& clientData = *static_cast<JSVMClientData*>(vm.clientData);
    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject);

    auto* constructor = JSC::asObject(globalObject.get(&lexicalGlobalObject, clientData.builtinNames().WritableStreamPrivateName()));

    auto constructData = getConstructData(vm, constructor);
    ASSERT(constructData.type != CallData::Type::None);

    MarkedArgumentBuffer args;
    args.append(sink ? toJSNewlyCreated(&lexicalGlobalObject, &globalObject, sink.releaseNonNull()) : JSC::jsUndefined());
    ASSERT(!args.hasOverflowed());

    JSObject* object = JSC::construct(&lexicalGlobalObject, constructor, constructData, args);
    ASSERT(!!scope.exception() == !object);
    RETURN_IF_EXCEPTION(scope, Exception { ExistingExceptionError });

    return create(globalObject, *jsCast<JSWritableStream*>(object));
}

}
