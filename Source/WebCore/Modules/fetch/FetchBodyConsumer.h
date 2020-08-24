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

#include "FetchBodySource.h"
#include "JSDOMPromiseDeferred.h"
#include "ReadableStreamSink.h"
#include "SharedBuffer.h"
#include "UserGestureIndicator.h"

namespace WebCore {

class Blob;
class FetchBodySource;
class ReadableStream;

class FetchBodyConsumer {
public:
    enum class Type { None, ArrayBuffer, Blob, JSON, Text, FormData };

    explicit FetchBodyConsumer(Type type) : m_type(type) { }

    void append(const char* data, unsigned);
    void append(const unsigned char* data, unsigned);

    bool hasData() const { return !!m_buffer; }
    const SharedBuffer* data() const { return m_buffer.get(); }
    void setData(Ref<SharedBuffer>&& data) { m_buffer = WTFMove(data); }

    RefPtr<SharedBuffer> takeData();
    RefPtr<JSC::ArrayBuffer> takeAsArrayBuffer();
    String takeAsText();

    void setContentType(const String& contentType) { m_contentType = contentType; }
    void setType(Type type) { m_type = type; }

    void clean();

    void extract(ReadableStream&, ReadableStreamToSharedBufferSink::Callback&&);
    void resolve(Ref<DeferredPromise>&&, const String& contentType, ReadableStream*);
    void resolveWithData(Ref<DeferredPromise>&&, const String& contentType, const unsigned char*, unsigned);

    void loadingFailed(const Exception&);
    void loadingSucceeded(const String& contentType);

    void setConsumePromise(Ref<DeferredPromise>&&);
    void setSource(Ref<FetchBodySource>&&);

    void setAsLoading() { m_isLoading = true; }

private:
    Ref<Blob> takeAsBlob();
    void resetConsumePromise();

    Type m_type;
    String m_contentType;
    RefPtr<SharedBuffer> m_buffer;
    RefPtr<DeferredPromise> m_consumePromise;
    RefPtr<ReadableStreamToSharedBufferSink> m_sink;
    RefPtr<FetchBodySource> m_source;
    bool m_isLoading { false };
    RefPtr<UserGestureToken> m_userGestureToken;
};

} // namespace WebCore
