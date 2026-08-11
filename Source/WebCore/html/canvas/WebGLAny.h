/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGL)

#include <JavaScriptCore/Float32Array.h>
#include <JavaScriptCore/Int32Array.h>
#include <JavaScriptCore/Uint32Array.h>
#include <JavaScriptCore/Uint8Array.h>
#include <WebCore/WebGLBuffer.h>
#include <WebCore/WebGLFramebuffer.h>
#include <WebCore/WebGLObject.h>
#include <WebCore/WebGLProgram.h>
#include <WebCore/WebGLQuery.h>
#include <WebCore/WebGLRenderbuffer.h>
#include <WebCore/WebGLSampler.h>
#include <WebCore/WebGLTexture.h>
#include <WebCore/WebGLTimerQueryEXT.h>
#include <WebCore/WebGLTransformFeedback.h>
#include <WebCore/WebGLVertexArrayObject.h>
#include <WebCore/WebGLVertexArrayObjectOES.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

using WebGLAny = Variant<
    std::nullptr_t,
    bool,
    int,
    unsigned,
    long long,
    unsigned long long,
    float,
    String,
    Vector<bool>,
    Vector<int>,
    Vector<unsigned>,
    Ref<Float32Array>,
    Ref<Int32Array>,
    Ref<Uint32Array>,
    Ref<Uint8Array>,
    Ref<WebGLBuffer>,
    Ref<WebGLFramebuffer>,
    Ref<WebGLProgram>,
    Ref<WebGLQuery>,
    Ref<WebGLRenderbuffer>,
    Ref<WebGLSampler>,
    Ref<WebGLTexture>,
    Ref<WebGLTimerQueryEXT>,
    Ref<WebGLTransformFeedback>,
    Ref<WebGLVertexArrayObject>,
    Ref<WebGLVertexArrayObjectOES>
>;

template<typename T> WebGLAny toWebGLAny(RefPtr<T> nullableValue)
{
    if (nullableValue)
        return nullableValue.releaseNonNull();
    return nullptr;
}

template<typename T> WebGLAny toWebGLAny(T* nullableValue)
{
    return toWebGLAny(RefPtr<T> { nullableValue });
}

template<typename T, unsigned target> WebGLAny toWebGLAny(const WebGLBindingPoint<T, target>& nullableValue)
{
    return toWebGLAny(static_cast<RefPtr<T>>(nullableValue));
}

} // namespace WebCore

#endif
