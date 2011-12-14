/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef WebArrayBuffer_h
#define WebArrayBuffer_h

#include "platform/WebCommon.h"
#include "platform/WebPrivatePtr.h"

#if WEBKIT_USING_V8
namespace v8 {
class Value;
template <class T> class Handle;
}
#endif

namespace WTF { class ArrayBuffer; }

namespace WebKit {

class WebArrayBuffer {
public:
    ~WebArrayBuffer() { reset(); }

    WebArrayBuffer() { }
    WebArrayBuffer(const WebArrayBuffer& b) { assign(b); }
    WebArrayBuffer& operator=(const WebArrayBuffer& b)
    {
        assign(b);
        return *this;
    }

    WEBKIT_EXPORT static WebArrayBuffer create(unsigned numElements, unsigned elementByteSize);

    WEBKIT_EXPORT void reset();
    WEBKIT_EXPORT void assign(const WebArrayBuffer&);

    bool isNull() const { return m_private.isNull(); }
    WEBKIT_EXPORT void* data() const;
    WEBKIT_EXPORT unsigned byteLength() const;

#if WEBKIT_USING_V8
    WEBKIT_EXPORT v8::Handle<v8::Value> toV8Value();
#endif

#if WEBKIT_IMPLEMENTATION
    WebArrayBuffer(const WTF::PassRefPtr<WTF::ArrayBuffer>&);
    WebArrayBuffer& operator=(const PassRefPtr<WTF::ArrayBuffer>&);
    operator WTF::PassRefPtr<WTF::ArrayBuffer>() const;
#endif

protected:
    WebPrivatePtr<WTF::ArrayBuffer> m_private;
};

} // namespace WebKit

#endif // WebArrayBuffer_h
