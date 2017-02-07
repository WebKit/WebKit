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
#include "WebGLAny.h"

#if ENABLE(WEBGL)

#include "JSDOMConvert.h"
#include "JSWebGLBuffer.h"
#include "JSWebGLFramebuffer.h"
#include "JSWebGLProgram.h"
#include "JSWebGLRenderbuffer.h"
#include "JSWebGLTexture.h"
#include "JSWebGLVertexArrayObjectOES.h"
#include <wtf/Variant.h>

#if ENABLE(WEBGL2)
#include "JSWebGLVertexArrayObject.h"
#endif

namespace WebCore {

using namespace JSC;

// FIXME: This should use the IDLUnion JSConverter.
JSValue convertToJSValue(ExecState& state, JSDOMGlobalObject& globalObject, const WebGLAny& any)
{
    return WTF::switchOn(any,
        [] (std::nullptr_t) {
            return jsNull();
        },
        [] (bool value) {
            return jsBoolean(value);
        },
        [] (int value) {
            return jsNumber(value);
        },
        [] (unsigned value) {
            return jsNumber(value);
        },
        [] (long long value) {
            return jsNumber(value);
        },
        [] (float value) {
            return jsNumber(value);
        },
        [&] (const String& value) {
            return jsStringWithCache(&state, value);
        },
        [&] (const Vector<bool>& values) {
            MarkedArgumentBuffer list;
            for (auto& value : values)
                list.append(jsBoolean(value));
            return constructArray(&state, 0, &globalObject, list);
        },
        [&] (const RefPtr<Float32Array>& array) {
            return toJS(&state, &globalObject, array.get());
        },
        [&] (const RefPtr<Int32Array>& array) {
            return toJS(&state, &globalObject, array.get());
        },
        [&] (const RefPtr<Uint8Array>& array) {
            return toJS(&state, &globalObject, array.get());
        },
        [&] (const RefPtr<Uint32Array>& array) {
            return toJS(&state, &globalObject, array.get());
        },
        [&] (const RefPtr<WebGLBuffer>& buffer) {
            return toJS(&state, &globalObject, buffer.get());
        },
        [&] (const RefPtr<WebGLFramebuffer>& buffer) {
            return toJS(&state, &globalObject, buffer.get());
        },
        [&] (const RefPtr<WebGLProgram>& program) {
            return toJS(&state, &globalObject, program.get());
        },
        [&] (const RefPtr<WebGLRenderbuffer>& buffer) {
            return toJS(&state, &globalObject, buffer.get());
        },
        [&] (const RefPtr<WebGLTexture>& texture) {
            return toJS(&state, &globalObject, texture.get());
        },
        [&] (const RefPtr<WebGLVertexArrayObjectOES>& array) {
            return toJS(&state, &globalObject, array.get());
        }
#if ENABLE(WEBGL2)
        ,
        [&] (const RefPtr<WebGLVertexArrayObject>& array) {
            return toJS(&state, &globalObject, array.get());
        }
#endif
    );
}

}

#endif
