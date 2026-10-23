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
#import "FontCache.h"

#import "FontCacheCoreText.h"
#import <wtf/cocoa/TypeCastsCocoa.h>

#import <pal/ios/UIKitSoftLink.h>

namespace WebCore {

#if PLATFORM(IOS_FAMILY)
CFStringRef getUIContentSizeCategoryDidChangeNotificationName()
{
    return static_cast<CFStringRef>(PAL::get_UIKit_UIContentSizeCategoryDidChangeNotificationSingleton());
}
#endif

static String& LEGACY_NODELETE contentSizeCategoryStorage()
{
    static NeverDestroyed<String> contentSizeCategory;
    return contentSizeCategory.get();
}

CFStringRef contentSizeCategory()
{
    if (!contentSizeCategoryStorage().isNull()) {
        // The contract of this function is that it returns a +0 autoreleased object (just like [[UIApplication sharedApplication] preferredContentSizeCategory] does).
        // createNSString().autorelease() returns a +0 autoreleased object, so we do that here, and then cast it to CFStringRef to return it.
        return bridge_cast(contentSizeCategoryStorage().createNSString().autorelease());
    }
#if PLATFORM(IOS_FAMILY)
    return static_cast<CFStringRef>([[PAL::getUIApplicationClassSingleton() sharedApplication] preferredContentSizeCategory]);
#else
    return kCTFontContentSizeCategoryL;
#endif
}

#if HAVE(TEXT_SCALE_FACTOR)
// Pages that opt in with <meta name="text-scale" content="scale"> can read the text scale factor, for example
// through getComputedStyle() or rem. Exposing the exact Body size ratio would reveal the user's content size
// category, which is a fingerprinting surface. So we snap the factor to the closest of a few buckets:
// - The default category maps to exactly 1, so medium stays 16px for most users.
// - Buckets are close together near the default, where most users are, so a value says little about one person.
// - Buckets are far apart away from it, so rare categories share a value with their neighbors
//   (for example, the three largest accessibility categories all map to 2.5).
static float snappedTextScaleFactor(float textScaleFactor)
{
    static constexpr std::array textScaleFactorBuckets { 0.85f, 0.94f, 1.f, 1.1f, 1.2f, 1.75f, 2.5f };
    float closestBucket = textScaleFactorBuckets.front();
    for (float bucket : textScaleFactorBuckets) {
        if (std::abs(textScaleFactor - bucket) < std::abs(textScaleFactor - closestBucket))
            closestBucket = bucket;
    }
    return closestBucket;
}

static CGFloat bodyTextSizeForContentSizeCategory(CFStringRef contentSizeCategory)
{
#if PLATFORM(MAC)
    return CTFontDescriptorGetTextStyleSize(kCTUIFontTextStyleBody, contentSizeCategory, kCTFontTextStylePlatformDefault, nullptr, nullptr);
#else
    RetainPtr traitCollection = [PAL::getUITraitCollectionClassSingleton() traitCollectionWithPreferredContentSizeCategory:bridge_cast(contentSizeCategory)];
    return [[PAL::getUIFontClassSingleton() preferredFontForTextStyle:PAL::get_UIKit_UIFontTextStyleBodySingleton() compatibleWithTraitCollection:traitCollection.get()] pointSize];
#endif
}

float textScaleFactorForContentSizeCategory(CFStringRef contentSizeCategory)
{
    // https://drafts.csswg.org/css-fonts-5/#scale-keyword
    // "the text scale factor matches the user's preferred paragraph text size [...] divided by 16px."
    // The Body text style is the paragraph text size. Its size relative to the default category scales
    // the 16px default, so the factor is 1 at the default category. The factor is snapped to a few
    // buckets to limit how much of the user's setting is exposed.
    static const CGFloat defaultBodySize = bodyTextSizeForContentSizeCategory(kCTFontContentSizeCategoryL);
    CGFloat bodySize = bodyTextSizeForContentSizeCategory(contentSizeCategory);
    return defaultBodySize && bodySize ? snappedTextScaleFactor(bodySize / defaultBodySize) : 1;
}

// The UI process computes the text scale factor and sends it. On macOS, CoreText only returns the user's text sizes to
// apps that opt in to Dynamic Type, and the web process does not.
static std::optional<float>& textScaleFactorStorage()
{
    static std::optional<float> textScaleFactor;
    return textScaleFactor;
}

std::optional<float> textScaleFactor()
{
    return textScaleFactorStorage();
}

void setTextScaleFactor(std::optional<float> textScaleFactor)
{
    textScaleFactorStorage() = textScaleFactor;
}
#endif

void setContentSizeCategory(const String& contentSizeCategory)
{
    if (contentSizeCategory == contentSizeCategoryStorage())
        return;

    contentSizeCategoryStorage() = contentSizeCategory;
    FontCache::invalidateAllFontCaches();
}

} // namespace WebCore
