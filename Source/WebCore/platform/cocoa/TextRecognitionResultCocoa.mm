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

#import "config.h"
#import "TextRecognitionResult.h"

#import "CharacterRange.h"
#import <wtf/RuntimeApplicationChecks.h>

#if USE(APPKIT)
#import <AppKit/NSAttributedString.h>
#else
#import <UIKit/NSAttributedString.h>
#endif

#import <pal/cocoa/VisionKitCoreSoftLink.h>

namespace WebCore {

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

RetainPtr<NSData> TextRecognitionResult::encodeVKCImageAnalysis(RetainPtr<VKCImageAnalysis> analysis)
{
    return [NSKeyedArchiver archivedDataWithRootObject:analysis.get() requiringSecureCoding:YES error:nil];
}

RetainPtr<VKCImageAnalysis> TextRecognitionResult::decodeVKCImageAnalysis(RetainPtr<NSData> data)
{
    if (!PAL::isVisionKitCoreFrameworkAvailable())
        return nil;

    // FIXME: This should use _enableStrictSecureDecodingMode or extract members into custom structures,
    // but that is blocked by rdar://108673895. In the meantime, just make sure this can't
    // be reached from outside the web content process to prevent sandbox escapes.
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(isInWebProcess());

    return [NSKeyedUnarchiver unarchivedObjectOfClass:PAL::getVKCImageAnalysisClassSingleton() fromData:data.get() error:nil];
}

RetainPtr<NSAttributedString> stringForRange(const TextRecognitionResult& result, const CharacterRange& range)
{
    if (auto analysis = TextRecognitionResult::decodeVKCImageAnalysis(result.imageAnalysisData); [analysis respondsToSelector:@selector(_attributedStringForRange:)])
        return { [analysis _attributedStringForRange:static_cast<NSRange>(range)] };
    return nil;
}

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

} // namespace WebCore
