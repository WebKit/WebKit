/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "WebAutocorrectionData.h"

#if PLATFORM(IOS_FAMILY)

#import "ArgumentCodersCocoa.h"
#import "Decoder.h"
#import "Encoder.h"
#import "WebCoreArgumentCoders.h"
#import <UIKit/UIKit.h>
#import <WebCore/FloatRect.h>

namespace WebKit {
using namespace WebCore;

void WebAutocorrectionData::encode(IPC::Encoder& encoder) const
{
    encoder << textRects;
    IPC::encode(encoder, font.get());
}

std::optional<WebAutocorrectionData> WebAutocorrectionData::decode(IPC::Decoder& decoder)
{
    std::optional<Vector<FloatRect>> textRects;
    decoder >> textRects;
    if (!textRects)
        return std::nullopt;

    RetainPtr<UIFont> font;
    if (!IPC::decode(decoder, font, @[ UIFont.class ]))
        return std::nullopt;

    return {{ *textRects, font }};
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
