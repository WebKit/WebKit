/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEBGL) && ENABLE(WEBGL2)

#include "JSWebGL2RenderingContext.h"
#include "NotImplemented.h"
#include <heap/HeapInlines.h>
#include <runtime/Error.h>

using namespace JSC;

namespace WebCore {

// FIXME: There is a duplicate version of this function in JSWebGLRenderingContextBaseCustom.cpp,
// but it is not exactly the same! We should merge these.
static JSValue toJS(ExecState* state, JSDOMGlobalObject* globalObject, const WebGLGetInfo& info)
{
    switch (info.getType()) {
    case WebGLGetInfo::kTypeBool:
        return jsBoolean(info.getBool());
    case WebGLGetInfo::kTypeBoolArray: {
        MarkedArgumentBuffer list;
        for (auto& value : info.getBoolArray())
            list.append(jsBoolean(value));
        return constructArray(state, 0, globalObject, list);
    }
    case WebGLGetInfo::kTypeFloat:
        return jsNumber(info.getFloat());
    case WebGLGetInfo::kTypeInt:
        return jsNumber(info.getInt());
    case WebGLGetInfo::kTypeNull:
        return jsNull();
    case WebGLGetInfo::kTypeString:
        return jsStringWithCache(state, info.getString());
    case WebGLGetInfo::kTypeUnsignedInt:
        return jsNumber(info.getUnsignedInt());
    case WebGLGetInfo::kTypeInt64:
        return jsNumber(info.getInt64());
    default:
        notImplemented();
        return jsUndefined();
    }
}

void JSWebGL2RenderingContext::visitAdditionalChildren(SlotVisitor& visitor)
{
    visitor.addOpaqueRoot(&wrapped());
}

JSValue JSWebGL2RenderingContext::getInternalformatParameter(ExecState&)
{
    return jsUndefined();
}

JSValue JSWebGL2RenderingContext::getQueryParameter(ExecState&)
{
    return jsUndefined();
}

JSValue JSWebGL2RenderingContext::getSamplerParameter(ExecState&)
{
    return jsUndefined();
}

JSValue JSWebGL2RenderingContext::getSyncParameter(ExecState&)
{
    return jsUndefined();
}

JSValue JSWebGL2RenderingContext::getIndexedParameter(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 2)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    unsigned pname = state.uncheckedArgument(0).toInt32(&state);
    RETURN_IF_EXCEPTION(scope, JSValue());
    unsigned index = state.uncheckedArgument(1).toInt32(&state);
    RETURN_IF_EXCEPTION(scope, JSValue());
    return toJS(&state, globalObject(), wrapped().getIndexedParameter(pname, index));
}

JSValue JSWebGL2RenderingContext::getActiveUniformBlockParameter(ExecState&)
{
    return jsUndefined();
}

JSValue JSWebGL2RenderingContext::getActiveUniformBlockName(ExecState&)
{
    return jsUndefined();
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
