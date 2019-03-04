/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebHitTestResultData.h"

#if PLATFORM(MAC)

#import "ArgumentCodersCF.h"
#import "ArgumentCodersCocoa.h"
#import "Decoder.h"
#import "Encoder.h"
#import "WebCoreArgumentCoders.h"
#import <WebCore/TextIndicator.h>
#import <pal/spi/cocoa/NSKeyedArchiverSPI.h>
#import <pal/spi/mac/DataDetectorsSPI.h>

namespace WebKit {

void WebHitTestResultData::platformEncode(IPC::Encoder& encoder) const
{
    bool hasActionContext = detectedDataActionContext;
    encoder << hasActionContext;
    if (!hasActionContext)
        return;

    encoder << detectedDataActionContext;

    encoder << detectedDataBoundingBox;
    encoder << detectedDataOriginatingPageOverlay;

    bool hasDetectedDataTextIndicator = detectedDataTextIndicator;
    encoder << hasDetectedDataTextIndicator;
    if (hasDetectedDataTextIndicator)
        encoder << detectedDataTextIndicator->data();
}

bool WebHitTestResultData::platformDecode(IPC::Decoder& decoder, WebHitTestResultData& hitTestResultData)
{
    bool hasActionContext;
    if (!decoder.decode(hasActionContext))
        return false;

    if (!hasActionContext)
        return true;
    ASSERT(DataDetectorsLibrary());

    auto detectedDataActionContext = IPC::decode<DDActionContext>(decoder, getDDActionContextClass());
    if (!detectedDataActionContext)
        return false;

    hitTestResultData.detectedDataActionContext = WTFMove(*detectedDataActionContext);

    if (!decoder.decode(hitTestResultData.detectedDataBoundingBox))
        return false;

    if (!decoder.decode(hitTestResultData.detectedDataOriginatingPageOverlay))
        return false;

    bool hasDetectedDataTextIndicator;
    if (!decoder.decode(hasDetectedDataTextIndicator))
        return false;

    if (hasDetectedDataTextIndicator) {
        Optional<WebCore::TextIndicatorData> indicatorData;
        decoder >> indicatorData;
        if (!indicatorData)
            return false;

        hitTestResultData.detectedDataTextIndicator = WebCore::TextIndicator::create(*indicatorData);
    }

    return true;
}

} // namespace WebKit

#endif // PLATFORM(MAC)
