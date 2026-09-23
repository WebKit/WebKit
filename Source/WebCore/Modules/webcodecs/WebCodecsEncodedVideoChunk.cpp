/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WebCodecsEncodedVideoChunk.h"

#if ENABLE(WEB_CODECS)

#include "ExceptionOr.h"
#include "WebCodecsBufferTransfer.h"
#include <JavaScriptCore/JSGlobalObject.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

ExceptionOr<Ref<WebCodecsEncodedVideoChunk>> WebCodecsEncodedVideoChunk::create(JSC::JSGlobalObject& globalObject, Init&& init)
{
    WebCodecsTransferList transferList { WTF::move(init.transfer) };
    if (auto result = transferList.validate(); result.hasException())
        return result.releaseException();

    Ref buffer = transferList.isEmpty() ? SharedBuffer::create(init.data.span()) : transferList.takeData(globalObject.vm(), init.data);
    // FIXME: timestamp is required by the spec; make it non-optional in the IDL.
    return create(init.type, init.timestamp.value_or(0), init.duration, WTF::move(buffer));
}

ExceptionOr<void> WebCodecsEncodedVideoChunk::copyTo(BufferSource&& source)
{
    if (source.byteLength() < byteLength())
        return Exception { ExceptionCode::TypeError, "buffer is too small"_s };

    memcpySpan(source.mutableSpan(), buffer()->span());
    return { };
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
