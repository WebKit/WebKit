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

#import "config.h"

#if PLATFORM(IOS_FAMILY)
#import "WebAutocorrectionData.h"

#import "UIKitSPI.h"
#import <UIKit/UIKit.h>
#import <WebCore/FloatRect.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

namespace WebKit {

WebAutocorrectionData::WebAutocorrectionData(Vector<WebCore::FloatRect>&& textRects, std::optional<String>&& fontName, double pointSize, double weight)
{
    this->textRects = WTFMove(textRects);
    if (fontName.has_value())
        this->font = [UIFont fontWithName:WTFMove(*fontName) size:pointSize];
    else
        this->font = [UIFont systemFontOfSize:pointSize weight:weight];
}

WebAutocorrectionData::WebAutocorrectionData(const Vector<WebCore::FloatRect>& textRects, const RetainPtr<UIFont>& font)
{
    this->textRects = textRects;
    this->font = font;
}

std::optional<String> WebAutocorrectionData::fontName() const
{
    if ([font isSystemFont])
        return std::nullopt;
    return { { [font fontName] } };
}

double WebAutocorrectionData::fontPointSize() const
{
    return [font pointSize];
}

double WebAutocorrectionData::fontWeight() const
{
    return [[[font fontDescriptor] objectForKey:UIFontWeightTrait] doubleValue];
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
