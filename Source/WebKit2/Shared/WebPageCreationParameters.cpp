/*
 * Copyright (C)  2010 Apple Inc. All rights reserved.
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
#include "WebPageCreationParameters.h"

#include "WebCoreArgumentCoders.h"

namespace WebKit {

void WebPageCreationParameters::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encode(viewSize);
    encoder->encode(isActive);
    encoder->encode(isFocused);
    encoder->encode(isVisible);
    encoder->encode(isInWindow);

    encoder->encode(store);
    encoder->encodeEnum(drawingAreaType);
    encoder->encode(pageGroupData);
    encoder->encode(drawsBackground);
    encoder->encode(drawsTransparentBackground);
    encoder->encode(areMemoryCacheClientCallsEnabled);
    encoder->encode(useFixedLayout);
    encoder->encode(fixedLayoutSize);
    encoder->encode(userAgent);
    encoder->encode(sessionState);
    encoder->encode(highestUsedBackForwardItemID);
    encoder->encode(canRunBeforeUnloadConfirmPanel);
    encoder->encode(canRunModal);
    encoder->encode(userSpaceScaleFactor);

#if PLATFORM(MAC)
    encoder->encode(isSmartInsertDeleteEnabled);
#endif

#if PLATFORM(WIN)
    encoder->encode(reinterpret_cast<uint64_t>(nativeWindow));
#endif
}

bool WebPageCreationParameters::decode(CoreIPC::ArgumentDecoder* decoder, WebPageCreationParameters& parameters)
{
    if (!decoder->decode(parameters.viewSize))
        return false;
    if (!decoder->decode(parameters.isActive))
        return false;
    if (!decoder->decode(parameters.isFocused))
        return false;
    if (!decoder->decode(parameters.isVisible))
        return false;
    if (!decoder->decode(parameters.isInWindow))
        return false;
    if (!decoder->decode(parameters.store))
        return false;
    if (!decoder->decodeEnum(parameters.drawingAreaType))
        return false;
    if (!decoder->decode(parameters.pageGroupData))
        return false;
    if (!decoder->decode(parameters.drawsBackground))
        return false;
    if (!decoder->decode(parameters.drawsTransparentBackground))
        return false;
    if (!decoder->decode(parameters.areMemoryCacheClientCallsEnabled))
        return false;
    if (!decoder->decode(parameters.useFixedLayout))
        return false;
    if (!decoder->decode(parameters.fixedLayoutSize))
        return false;
    if (!decoder->decode(parameters.userAgent))
        return false;
    if (!decoder->decode(parameters.sessionState))
        return false;
    if (!decoder->decode(parameters.highestUsedBackForwardItemID))
        return false;
    if (!decoder->decode(parameters.canRunBeforeUnloadConfirmPanel))
        return false;
    if (!decoder->decode(parameters.canRunModal))
        return false;
    if (!decoder->decode(parameters.userSpaceScaleFactor))
        return false;

#if PLATFORM(MAC)
    if (!decoder->decode(parameters.isSmartInsertDeleteEnabled))
        return false;
#endif

#if PLATFORM(WIN)
    uint64_t nativeWindow;
    if (!decoder->decode(nativeWindow))
        return false;
    parameters.nativeWindow = reinterpret_cast<HWND>(nativeWindow);
#endif

    return true;
}

} // namespace WebKit
