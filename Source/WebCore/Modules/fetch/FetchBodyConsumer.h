/*
 * Copyright (C) 2016 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "JSDOMPromiseDeferred.h"
#include "SharedBuffer.h"

namespace WebCore {

class Blob;

class FetchBodyConsumer {
public:
    // Type is used in FetchResponse.js and should be kept synchronized with it.
    enum class Type { None, ArrayBuffer, Blob, JSON, Text };

    FetchBodyConsumer(Type type) : m_type(type) { }

    void append(const char* data, unsigned);
    void append(const unsigned char* data, unsigned);

    RefPtr<SharedBuffer> takeData();
    RefPtr<JSC::ArrayBuffer> takeAsArrayBuffer();
    Ref<Blob> takeAsBlob();
    String takeAsText();

    void setContentType(const String& contentType) { m_contentType = contentType; }
    void setType(Type type) { m_type = type; }

    void clean() { m_buffer = nullptr; }

    void resolve(Ref<DeferredPromise>&&);
    void resolveWithData(Ref<DeferredPromise>&&, const unsigned char*, unsigned);

    bool hasData() const { return !!m_buffer; }

private:
    Type m_type;
    String m_contentType;
    RefPtr<SharedBuffer> m_buffer;
};

} // namespace WebCore
