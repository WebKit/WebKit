/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "WebProcessCreationParameters.h"

#include "ArgumentCoders.h"

namespace WebKit {

WebProcessCreationParameters::WebProcessCreationParameters()
    : shouldTrackVisitedLinks(false)
#if PLATFORM(WIN)
    , shouldPaintNativeControls(false)
#endif
{
}

void WebProcessCreationParameters::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encode(injectedBundlePath);
#if ENABLE(WEB_PROCESS_SANDBOX)
    encoder->encode(injectedBundlePathToken);
#endif

    encoder->encode(applicationCacheDirectory);
    encoder->encode(urlSchemesRegistererdAsEmptyDocument);
    encoder->encode(static_cast<uint32_t>(cacheModel));
    encoder->encode(shouldTrackVisitedLinks);

#if PLATFORM(MAC)
    encoder->encode(acceleratedCompositingPort);
#elif PLATFORM(WIN)
    encoder->encode(shouldPaintNativeControls);
#endif
}

bool WebProcessCreationParameters::decode(CoreIPC::ArgumentDecoder* decoder, WebProcessCreationParameters& parameters)
{
    if (!decoder->decode(parameters.injectedBundlePath))
        return false;
#if ENABLE(WEB_PROCESS_SANDBOX)
    if (!decoder->decode(parameters.injectedBundlePathToken))
        return false;
#endif

    if (!decoder->decode(parameters.applicationCacheDirectory))
        return false;
    if (!decoder->decode(parameters.urlSchemesRegistererdAsEmptyDocument))
        return false;

    uint32_t cacheModel;
    if (!decoder->decode(cacheModel))
        return false;
    parameters.cacheModel = static_cast<CacheModel>(cacheModel);

    if (!decoder->decode(parameters.shouldTrackVisitedLinks))
        return false;

#if PLATFORM(MAC)
    if (!decoder->decode(parameters.acceleratedCompositingPort))
        return false;
#elif PLATFORM(WIN)
    if (!decoder->decode(parameters.shouldPaintNativeControls))
        return false;
#endif

    return true;
}

} // namespace WebKit
