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

TEST(PlatformCAAnimation, KeyPathSinglePropertyConstructor)
{
    WebCore::PlatformCAAnimation::KeyPath keyPath { WebCore::AnimatedProperty::Transform };
    EXPECT_EQ(keyPath.animatedProperty, WebCore::AnimatedProperty::Transform);
    EXPECT_EQ(keyPath.filterOperationType, WebCore::FilterOperation::Type::None);
    EXPECT_EQ(keyPath.index, 0);
}

TEST(PlatformCAAnimation, KeyPathFullConstructor)
{
    WebCore::PlatformCAAnimation::KeyPath keyPath { WebCore::AnimatedProperty::Filter, WebCore::FilterOperation::Type::Invert, 2 };
    EXPECT_EQ(keyPath.animatedProperty, WebCore::AnimatedProperty::Filter);
    EXPECT_EQ(keyPath.filterOperationType, WebCore::FilterOperation::Type::Invert);
    EXPECT_EQ(keyPath.index, 2);
}

TEST(PlatformCAAnimation, KeyPathStringRepresentation)
{
    WebCore::PlatformCAAnimation::KeyPath translate { WebCore::AnimatedProperty::Translate };
    EXPECT_STREQ(translate.string().ascii().data(), "transform");

    WebCore::PlatformCAAnimation::KeyPath scale { WebCore::AnimatedProperty::Scale };
    EXPECT_STREQ(scale.string().ascii().data(), "transform");

    WebCore::PlatformCAAnimation::KeyPath rotate { WebCore::AnimatedProperty::Rotate };
    EXPECT_STREQ(rotate.string().ascii().data(), "transform");

    WebCore::PlatformCAAnimation::KeyPath transform { WebCore::AnimatedProperty::Transform };
    EXPECT_STREQ(transform.string().ascii().data(), "transform");

    WebCore::PlatformCAAnimation::KeyPath opacity { WebCore::AnimatedProperty::Opacity };
    EXPECT_STREQ(opacity.string().ascii().data(), "opacity");

    WebCore::PlatformCAAnimation::KeyPath backgroundColor { WebCore::AnimatedProperty::BackgroundColor };
    EXPECT_STREQ(backgroundColor.string().ascii().data(), "backgroundColor");

    WebCore::PlatformCAAnimation::KeyPath filter { WebCore::AnimatedProperty::Filter, WebCore::FilterOperation::Type::Grayscale, 2 };
    EXPECT_STREQ(filter.string().ascii().data(), "filters.filter_2.inputAmount");

#if ENABLE(FILTERS_LEVEL_2)
    WebCore::PlatformCAAnimation::KeyPath backdropFilter { WebCore::AnimatedProperty::WebkitBackdropFilter };
    EXPECT_STREQ(backdropFilter.string().ascii().data(), "backdropFilters");
#endif
}

static void testKeyPathStringRountrip(const WebCore::PlatformCAAnimation::KeyPath& src)
{
    WebCore::PlatformCAAnimation::KeyPath keyPath { src.string() };
    EXPECT_EQ(src.animatedProperty, keyPath.animatedProperty);
    EXPECT_EQ(src.filterOperationType, keyPath.filterOperationType);
    EXPECT_EQ(src.index, keyPath.index);
}

TEST(PlatformCAAnimation, KeyPathStringConstructor)
{
    testKeyPathStringRountrip({ WebCore::AnimatedProperty::Transform });
    testKeyPathStringRountrip({ WebCore::AnimatedProperty::Opacity });
    testKeyPathStringRountrip({ WebCore::AnimatedProperty::BackgroundColor });
    testKeyPathStringRountrip({ WebCore::AnimatedProperty::Filter, WebCore::FilterOperation::Type::Grayscale, 2 });
    testKeyPathStringRountrip({ WebCore::AnimatedProperty::Filter, WebCore::FilterOperation::Type::Sepia, 22 });
#if ENABLE(FILTERS_LEVEL_2)
    testKeyPathStringRountrip({ WebCore::AnimatedProperty::WebkitBackdropFilter });
#endif
}

}
