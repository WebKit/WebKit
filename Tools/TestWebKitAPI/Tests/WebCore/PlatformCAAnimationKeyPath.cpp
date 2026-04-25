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

#include "config.h"

#include <WebCore/PlatformCAAnimation.h>

namespace TestWebKitAPI {

TEST(PlatformCAAnimation, makeKeyPath)
{
    auto translate = WebCore::PlatformCAAnimation::makeKeyPath(WebCore::AnimatedProperty::Translate);
    EXPECT_STREQ(translate.ascii().data(), "transform");

    auto scale = WebCore::PlatformCAAnimation::makeKeyPath(WebCore::AnimatedProperty::Scale);
    EXPECT_STREQ(scale.ascii().data(), "transform");

    auto rotate = WebCore::PlatformCAAnimation::makeKeyPath(WebCore::AnimatedProperty::Rotate);
    EXPECT_STREQ(rotate.ascii().data(), "transform");

    auto transform = WebCore::PlatformCAAnimation::makeKeyPath(WebCore::AnimatedProperty::Transform);
    EXPECT_STREQ(transform.ascii().data(), "transform");

    auto opacity = WebCore::PlatformCAAnimation::makeKeyPath(WebCore::AnimatedProperty::Opacity);
    EXPECT_STREQ(opacity.ascii().data(), "opacity");

    auto backgroundColor = WebCore::PlatformCAAnimation::makeKeyPath(WebCore::AnimatedProperty::BackgroundColor);
    EXPECT_STREQ(backgroundColor.ascii().data(), "backgroundColor");

    auto filter = WebCore::PlatformCAAnimation::makeKeyPath(WebCore::AnimatedProperty::Filter, WebCore::FilterOperation::Type::Grayscale, 2);
    EXPECT_STREQ(filter.ascii().data(), "filters.filter_2.inputAmount");

    auto backdropFilter = WebCore::PlatformCAAnimation::makeKeyPath(WebCore::AnimatedProperty::WebkitBackdropFilter);
    EXPECT_STREQ(backdropFilter.ascii().data(), "backdropFilters");
}

static void validateGeneratedKeyPath(WebCore::AnimatedProperty animatedProperty, WebCore::FilterOperation::Type filterOperationType = WebCore::FilterOperation::Type::None, int index = 0)
{
    auto keyPath = WebCore::PlatformCAAnimation::makeKeyPath(animatedProperty, filterOperationType, index);
    EXPECT_TRUE(WebCore::PlatformCAAnimation::isValidKeyPath(keyPath));
}

TEST(PlatformCAAnimation, isValidKeyPath)
{
    validateGeneratedKeyPath(WebCore::AnimatedProperty::Transform);
    validateGeneratedKeyPath(WebCore::AnimatedProperty::Opacity);
    validateGeneratedKeyPath(WebCore::AnimatedProperty::BackgroundColor);
    validateGeneratedKeyPath(WebCore::AnimatedProperty::Filter, WebCore::FilterOperation::Type::Grayscale, 2);
    validateGeneratedKeyPath(WebCore::AnimatedProperty::Filter, WebCore::FilterOperation::Type::Sepia, 22);
    validateGeneratedKeyPath(WebCore::AnimatedProperty::WebkitBackdropFilter);

    EXPECT_FALSE(WebCore::PlatformCAAnimation::isValidKeyPath("filters.filter_"_s));
    EXPECT_FALSE(WebCore::PlatformCAAnimation::isValidKeyPath("filters.filter_0"_s));
    EXPECT_FALSE(WebCore::PlatformCAAnimation::isValidKeyPath("filters.filter_10"_s));
    EXPECT_FALSE(WebCore::PlatformCAAnimation::isValidKeyPath("filters.filter_0.inputAmount."_s));
    EXPECT_FALSE(WebCore::PlatformCAAnimation::isValidKeyPath("filters.filter_0.inputAmounts"_s));
    EXPECT_FALSE(WebCore::PlatformCAAnimation::isValidKeyPath("filters.filter_-10.inputAmount"_s));

    EXPECT_TRUE(WebCore::PlatformCAAnimation::isValidKeyPath(emptyString(), WebCore::PlatformCAAnimation::AnimationType::Group));
    EXPECT_FALSE(WebCore::PlatformCAAnimation::isValidKeyPath(WebCore::PlatformCAAnimation::makeKeyPath(WebCore::AnimatedProperty::Translate), WebCore::PlatformCAAnimation::AnimationType::Group));
    EXPECT_FALSE(WebCore::PlatformCAAnimation::isValidKeyPath(emptyString()));

    EXPECT_FALSE(WebCore::PlatformCAAnimation::isValidKeyPath("dealloc"_s));
}

}
