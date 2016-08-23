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

#pragma once

#include "CanvasRenderingContext2D.h"
#include "JSCanvasRenderingContext2D.h"

#if ENABLE(WEBGL)
#include "JSWebGL2RenderingContext.h"
#include "JSWebGLRenderingContext.h"
#include "WebGL2RenderingContext.h"
#include "WebGLRenderingContext.h"
#endif

namespace WebCore {

inline JSC::JSValue toJS(JSC::ExecState* state, JSDOMGlobalObject* globalObject, CanvasRenderingContext& object)
{
#if ENABLE(WEBGL)
    if (is<WebGLRenderingContext>(object))
        return wrap(state, globalObject, downcast<WebGLRenderingContext>(object));
#if ENABLE(WEBGL2)
    if (is<WebGL2RenderingContext>(object))
        return wrap(state, globalObject, downcast<WebGL2RenderingContext>(object));
#endif
#endif
    return wrap(state, globalObject, downcast<CanvasRenderingContext2D>(object));
}

inline JSC::JSValue toJS(JSC::ExecState* state, JSDOMGlobalObject* globalObject, CanvasRenderingContext* object)
{
    return object ? toJS(state, globalObject, *object) : JSC::jsNull();
}

} // namespace WebCore
