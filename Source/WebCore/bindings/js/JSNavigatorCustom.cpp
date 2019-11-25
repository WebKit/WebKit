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
#include "JSNavigator.h"

#include "WebCoreJSClientData.h"
#include <JavaScriptCore/CatchScope.h>

namespace WebCore {

void JSNavigator::visitAdditionalChildren(JSC::SlotVisitor& visitor)
{
    visitor.addOpaqueRoot(static_cast<NavigatorBase*>(&wrapped()));
}

#if ENABLE(MEDIA_STREAM)
JSValue JSNavigator::getUserMedia(JSGlobalObject& lexicalGlobalObject, CallFrame& callFrame)
{
    auto* function = globalObject()->builtinInternalFunctions().jsDOMBindingInternals().m_getUserMediaShimFunction.get();
    ASSERT(function);

    JSC::CallData callData;
    JSC::CallType callType = JSC::getCallData(lexicalGlobalObject.vm(), function, callData);
    ASSERT(callType != JSC::CallType::None);
    JSC::MarkedArgumentBuffer arguments;
    for (size_t cptr = 0; cptr < callFrame.argumentCount(); ++cptr)
        arguments.append(callFrame.uncheckedArgument(cptr));
    ASSERT(!arguments.hasOverflowed());
    JSC::call(&lexicalGlobalObject, function, callType, callData, this, arguments);
    return jsUndefined();
}
#endif

}
