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
#include "WebCodecsImageDecoder.h"

#if ENABLE(WEB_CODECS)

#include "JSDOMConvertDictionary.h"
#include "JSDOMPromiseDeferred.h"
#include "JSWebCodecsImageDecodeResult.h"
#include "WebCodecsImageDecodeResult.h"
#include "WebCodecsVideoFrame.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebCodecsImageDecoder);

Ref<WebCodecsImageDecoder> WebCodecsImageDecoder::create(ScriptExecutionContext& context, Init&& init)
{
    Ref decoder = adoptRef(*new WebCodecsImageDecoder(context, WTF::move(init)));
    decoder->suspendIfNeeded();
    return decoder;
}

WebCodecsImageDecoder::WebCodecsImageDecoder(ScriptExecutionContext& context, Init&&)
    : WebCodecsBase(context)
    , m_completedPromise(makeUniqueRef<CompletedPromise>())
{
}

WebCodecsImageDecoder::~WebCodecsImageDecoder() = default;

Ref<WebCodecsImageTrackList> WebCodecsImageDecoder::tracks() const
{
    if (!m_tracks)
        m_tracks = WebCodecsImageTrackList::create({ WebCodecsImageTrack::create() });

    return *m_tracks;
}

ExceptionOr<void> WebCodecsImageDecoder::decode(std::optional<DecodeOptions>&&, Ref<DeferredPromise>&&)
{
    return Exception { ExceptionCode::NotSupportedError };
}

ExceptionOr<void> WebCodecsImageDecoder::reset()
{
    return Exception { ExceptionCode::NotSupportedError };
}

ExceptionOr<void> WebCodecsImageDecoder::close()
{
    return Exception { ExceptionCode::NotSupportedError };
}

void WebCodecsImageDecoder::isTypeSupported(ScriptExecutionContext&, String&&, Ref<DeferredPromise>&&)
{
}

void WebCodecsImageDecoder::suspend(ReasonForSuspension)
{
}

void WebCodecsImageDecoder::stop()
{
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
