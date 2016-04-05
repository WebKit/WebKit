/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include <runtime/Error.h>
#include "NotImplemented.h"
#include "WebGL2RenderingContext.h"

using namespace JSC;

namespace WebCore {

static JSValue toJS(ExecState* exec, JSDOMGlobalObject* globalObject, const WebGLGetInfo& info)
{
    switch (info.getType()) {
    case WebGLGetInfo::kTypeBool:
        return jsBoolean(info.getBool());
    case WebGLGetInfo::kTypeBoolArray: {
        MarkedArgumentBuffer list;
        const auto& values = info.getBoolArray();
        for (const auto& value : values)
            list.append(jsBoolean(value));
        return constructArray(exec, 0, globalObject, list);
    }
    case WebGLGetInfo::kTypeFloat:
        return jsNumber(info.getFloat());
    case WebGLGetInfo::kTypeInt:
        return jsNumber(info.getInt());
    case WebGLGetInfo::kTypeNull:
        return jsNull();
    case WebGLGetInfo::kTypeString:
        return jsStringWithCache(exec, info.getString());
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

JSValue JSWebGL2RenderingContext::getInternalformatParameter(ExecState& exec)
{
    UNUSED_PARAM(exec);
    return jsUndefined();
}

JSValue JSWebGL2RenderingContext::getQueryParameter(ExecState& exec)
{
    UNUSED_PARAM(exec);
    return jsUndefined();
}

JSValue JSWebGL2RenderingContext::getSamplerParameter(ExecState& exec)
{
    UNUSED_PARAM(exec);
    return jsUndefined();
}

JSValue JSWebGL2RenderingContext::getSyncParameter(ExecState& exec)
{
    UNUSED_PARAM(exec);
    return jsUndefined();
}

JSValue JSWebGL2RenderingContext::getIndexedParameter(ExecState& exec)
{
    if (exec.argumentCount() != 2)
        return exec.vm().throwException(&exec, createNotEnoughArgumentsError(&exec));

    WebGL2RenderingContext& context = wrapped();
    unsigned pname = exec.uncheckedArgument(0).toInt32(&exec);
    if (exec.hadException())
        return jsUndefined();
    unsigned index = exec.uncheckedArgument(1).toInt32(&exec);
    if (exec.hadException())
        return jsUndefined();
    WebGLGetInfo info = context.getIndexedParameter(pname, index);
    return toJS(&exec, globalObject(), info);
}

JSValue JSWebGL2RenderingContext::getActiveUniformBlockParameter(ExecState& exec)
{
    UNUSED_PARAM(exec);
    return jsUndefined();
}

JSValue JSWebGL2RenderingContext::getActiveUniformBlockName(ExecState& exec)
{
    UNUSED_PARAM(exec);
    return jsUndefined();
}


} // namespace WebCore

#endif // ENABLE(WEBGL)
