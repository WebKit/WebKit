/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(3D_CANVAS)
#include "V8WebGLArray.h"

#include "V8WebGLByteArray.h"
#include "V8WebGLFloatArray.h"
#include "V8WebGLIntArray.h"
#include "V8WebGLShortArray.h"
#include "V8WebGLUnsignedByteArray.h"
#include "V8WebGLUnsignedIntArray.h"
#include "V8WebGLUnsignedShortArray.h"

namespace WebCore {

v8::Handle<v8::Value> toV8(WebGLArray* impl)
{
    if (!impl)
        return v8::Null();
    if (impl->isByteArray())
        return toV8(static_cast<WebGLByteArray*>(impl));
    if (impl->isFloatArray())
        return toV8(static_cast<WebGLFloatArray*>(impl));
    if (impl->isIntArray())
        return toV8(static_cast<WebGLIntArray*>(impl));
    if (impl->isShortArray())
        return toV8(static_cast<WebGLShortArray*>(impl));
    if (impl->isUnsignedByteArray())
        return toV8(static_cast<WebGLUnsignedByteArray*>(impl));
    if (impl->isUnsignedIntArray())
        return toV8(static_cast<WebGLUnsignedIntArray*>(impl));
    if (impl->isUnsignedShortArray())
        return toV8(static_cast<WebGLUnsignedShortArray*>(impl));
    return v8::Handle<v8::Value>();
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
