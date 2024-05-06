/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "JSExtendableMessageEvent.h"

#include "JSDOMConstructor.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMConvertStrings.h"

namespace WebCore {

using namespace JSC;

JSC::EncodedJSValue constructJSExtendableMessageEvent(JSC::JSGlobalObject* lexicalGlobalObject, JSC::CallFrame& callFrame)
{
    VM& vm = lexicalGlobalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    UNUSED_PARAM(throwScope);

    if (UNLIKELY(callFrame.argumentCount() < 1))
        return throwVMError(lexicalGlobalObject, throwScope, createNotEnoughArgumentsError(lexicalGlobalObject));
    auto type = convert<IDLAtomStringAdaptor<IDLDOMString>>(*lexicalGlobalObject, callFrame.uncheckedArgument(0));
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());
    auto eventInitDict = convert<IDLDictionary<ExtendableMessageEvent::Init>>(*lexicalGlobalObject, callFrame.argument(1));
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    auto object = ExtendableMessageEvent::create(*lexicalGlobalObject, WTFMove(type), WTFMove(eventInitDict));
    RETURN_IF_EXCEPTION(throwScope, encodedJSValue());

    return JSValue::encode(object.strongWrapper.get());
}

template<typename Visitor>
void JSExtendableMessageEvent::visitAdditionalChildren(Visitor& visitor)
{
    wrapped().data().visit(visitor);
}

DEFINE_VISIT_ADDITIONAL_CHILDREN(JSExtendableMessageEvent);

}
