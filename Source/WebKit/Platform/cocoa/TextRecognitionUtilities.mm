/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "TextRecognitionUtilities.h"

#if ENABLE(IMAGE_ANALYSIS)

#import <WebCore/TextRecognitionResult.h>
#import <pal/cocoa/VisionKitCoreSoftLink.h>
#import <pal/spi/cocoa/FeatureFlagsSPI.h>

namespace WebKit {
using namespace WebCore;

static bool hasVisionKitCorePrefixedClasses()
{
    static bool result = false;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        result = !!PAL::getVKCImageAnalyzerClass();
    });
    return result;
}

RetainPtr<VKImageAnalyzer> createImageAnalyzer()
{
    if (hasVisionKitCorePrefixedClasses())
        return adoptNS([static_cast<VKImageAnalyzer *>(PAL::allocVKCImageAnalyzerInstance()) init]);

    return adoptNS([PAL::allocVKImageAnalyzerInstance() init]);
}

RetainPtr<VKImageAnalyzerRequest> createImageAnalyzerRequest(CGImageRef image, VKAnalysisTypes types)
{
    if (hasVisionKitCorePrefixedClasses())
        return adoptNS([static_cast<VKImageAnalyzerRequest *>(PAL::allocVKCImageAnalyzerRequestInstance()) initWithCGImage:image orientation:VKImageOrientationUp requestType:types]);

    return adoptNS([PAL::allocVKImageAnalyzerRequestInstance() initWithCGImage:image orientation:VKImageOrientationUp requestType:types]);
}

static FloatQuad floatQuad(VKQuad *quad)
{
    return { quad.topLeft, quad.topRight, quad.bottomRight, quad.bottomLeft };
}

static Vector<FloatQuad> floatQuads(NSArray<VKQuad *> *vkQuads)
{
    Vector<FloatQuad> quads;
    quads.reserveInitialCapacity(vkQuads.count);
    for (VKQuad *vkQuad in vkQuads)
        quads.uncheckedAppend(floatQuad(vkQuad));
    return quads;
}

TextRecognitionResult makeTextRecognitionResult(VKImageAnalysis *analysis)
{
    NSArray<VKWKTextInfo *> *allLines = analysis.allLines;
    TextRecognitionResult result;
    result.lines.reserveInitialCapacity(allLines.count);

    bool isFirstLine = true;
    for (VKWKLineInfo *line in allLines) {
        Vector<TextRecognitionWordData> children;
        NSArray<VKWKTextInfo *> *vkChildren = line.children;
        children.reserveInitialCapacity(vkChildren.count);

        String lineText = line.string;
        unsigned searchLocation = 0;
        for (VKWKTextInfo *child in vkChildren) {
            if (searchLocation >= lineText.length()) {
                ASSERT_NOT_REACHED();
                continue;
            }

            String childText = child.string;
            auto matchLocation = lineText.find(childText, searchLocation);
            if (matchLocation == notFound) {
                ASSERT_NOT_REACHED();
                continue;
            }

            bool hasLeadingWhitespace = ([&] {
                if (matchLocation == searchLocation)
                    return !isFirstLine && !searchLocation;

                auto textBeforeMatch = lineText.substring(searchLocation, matchLocation - searchLocation);
                return !textBeforeMatch.isEmpty() && isSpaceOrNewline(textBeforeMatch[0]);
            })();

            searchLocation = matchLocation + childText.length();
            children.uncheckedAppend({ WTFMove(childText), floatQuad(child.quad), hasLeadingWhitespace });
        }
        result.lines.uncheckedAppend({ floatQuad(line.quad), WTFMove(children) });
        isFirstLine = false;
    }

#if ENABLE(DATA_DETECTION)
    if ([analysis respondsToSelector:@selector(textDataDetectors)]) {
        auto dataDetectors = retainPtr(analysis.textDataDetectors);
        result.dataDetectors.reserveInitialCapacity([dataDetectors count]);
        for (VKWKDataDetectorInfo *info in dataDetectors.get())
            result.dataDetectors.uncheckedAppend({ info.result, floatQuads(info.boundingQuads) });
    }
#endif // ENABLE(DATA_DETECTION)

    return result;
}

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/TextRecognitionUtilitiesAdditions.mm>
#else

static bool isLiveTextEnabled()
{
    return true;
}

#endif

bool isLiveTextAvailableAndEnabled()
{
    return PAL::isVisionKitCoreFrameworkAvailable() && isLiveTextEnabled();
}

} // namespace WebKit

#endif // ENABLE(IMAGE_ANALYSIS)
