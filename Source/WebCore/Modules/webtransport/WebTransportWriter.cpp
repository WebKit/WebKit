/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "WebTransportWriter.h"

#include "InternalWritableStreamWriter.h"
#include "JSDOMGlobalObject.h"
#include "JSDOMPromiseDeferred.h"
#include "WritableStream.h"

namespace WebCore {

ExceptionOr<Ref<WebTransportWriter>> WebTransportWriter::create(JSDOMGlobalObject& globalObject, WritableStream& stream)
{
    auto result = acquireWritableStreamDefaultWriter(globalObject, stream);
    if (result.hasException())
        return result.releaseException();

    return adoptRef(*new WebTransportWriter(result.releaseReturnValue()));
}

WebTransportWriter::WebTransportWriter(Ref<InternalWritableStreamWriter>&& internalWriter)
    : WritableStreamDefaultWriter(WTF::move(internalWriter))
{
}

void WebTransportWriter::atomicWrite(JSC::JSValue, Ref<DeferredPromise>&& promise)
{
    // FIXME: Implement WebTransport atomic writes once the send-stream commit API exists (rdar://164169921).
    promise->reject(ExceptionCode::NotSupportedError);
}

void WebTransportWriter::commit()
{
    // FIXME: Implement WebTransport commit once the send-stream commit API exists (rdar://164169921).
}

} // namespace WebCore
