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
#include "WebCodecsVideoDecoder.h"

#if ENABLE(WEB_CODECS)

#include "DOMException.h"
#include "WebCodecsErrorCallback.h"
#include "WebCodecsVideoFrameOutputCallback.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebCodecsVideoDecoder);

Ref<WebCodecsVideoDecoder> WebCodecsVideoDecoder::create(ScriptExecutionContext& context, Init&& init)
{
    auto decoder = adoptRef(*new WebCodecsVideoDecoder(context, WTFMove(init)));
    decoder->suspendIfNeeded();
    return decoder;
}


WebCodecsVideoDecoder::WebCodecsVideoDecoder(ScriptExecutionContext& context, Init&& init)
    : ActiveDOMObject(&context)
    , m_output(init.output.releaseNonNull())
    , m_error(init.error.releaseNonNull())
{
}

WebCodecsVideoDecoder::~WebCodecsVideoDecoder()
{
}

void WebCodecsVideoDecoder::isConfigSupported(WebCodecsVideoDecoderConfig&&, Ref<DeferredPromise>&& promise)
{
    // FIXME: Implement.
    promise->reject(Exception { NotSupportedError, "Not implemented"_s });
}

void WebCodecsVideoDecoder::close()
{
    // FIXME: Implement.
}

void WebCodecsVideoDecoder::flush(Ref<DeferredPromise>&& promise)
{
    // FIXME: Implement.
    promise->reject(Exception { NotSupportedError, "Not implemented"_s });
}

void WebCodecsVideoDecoder::reset()
{
    // FIXME: Implement.
}

void WebCodecsVideoDecoder::decode(Ref<WebCodecsEncodedVideoChunk>&&)
{
    // FIXME: Implement.
    m_error->handleEvent(DOMException::create(Exception { NotSupportedError, "Not implemented"_s }).get());
}

void WebCore::WebCodecsVideoDecoder::suspend(ReasonForSuspension)
{
    // FIXME: Implement.
}

void WebCodecsVideoDecoder::configure(WebCodecsVideoDecoderConfig&&)
{
    // FIXME: Implement.
}

void WebCodecsVideoDecoder::stop()
{
    // FIXME: Implement.
}

const char* WebCodecsVideoDecoder::activeDOMObjectName() const
{
    return "VideoDecoder";
}

bool WebCodecsVideoDecoder::virtualHasPendingActivity() const
{
    return m_state == WebCodecsCodecState::Configured && m_decodeQueueSize;
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
