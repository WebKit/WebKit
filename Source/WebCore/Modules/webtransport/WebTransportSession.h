/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#pragma once

#include <span>
#include <wtf/AbstractRefCounted.h>
#include <wtf/NativePromise.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace WebCore {

class Exception;
class ReadableStreamSource;
class ScriptExecutionContext;
class WebTransportBidirectionalStream;
class WebTransportSendStream;
class WebTransportSessionClient;
class WritableStreamSink;

struct WebTransportBidirectionalStreamConstructionParameters;

using WritableStreamPromise = NativePromise<Ref<WritableStreamSink>, void>;
using BidirectionalStreamPromise = NativePromise<WebTransportBidirectionalStreamConstructionParameters, void>;
using WebTransportSendPromise = NativePromise<std::optional<Exception>, void>;

struct WebTransportStreamIdentifierType;
using WebTransportStreamIdentifier = ObjectIdentifier<WebTransportStreamIdentifierType>;

using WebTransportSessionErrorCode = uint32_t;
using WebTransportStreamErrorCode = uint64_t;

class WEBCORE_EXPORT WebTransportSession : public AbstractRefCounted {
public:
    virtual ~WebTransportSession();

    virtual Ref<WebTransportSendPromise> sendDatagram(std::span<const uint8_t>) = 0;
    virtual Ref<WritableStreamPromise> createOutgoingUnidirectionalStream() = 0;
    virtual Ref<BidirectionalStreamPromise> createBidirectionalStream() = 0;
    virtual void cancelReceiveStream(WebTransportStreamIdentifier, std::optional<WebTransportStreamErrorCode>) = 0;
    virtual void cancelSendStream(WebTransportStreamIdentifier, std::optional<WebTransportStreamErrorCode>) = 0;
    virtual void destroyStream(WebTransportStreamIdentifier, std::optional<WebTransportStreamErrorCode>) = 0;
    virtual void terminate(WebTransportSessionErrorCode, CString&&) = 0;
};

}
