/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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

#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/Filter.h>
#include <WebCore/Gradient.h>
#include <wtf/text/TextStream.h>

namespace TestWebKitAPI {
using namespace WebCore;
using DisplayList::DisplayList;
using namespace DisplayList;

static String convertToString(const Path& path)
{
    TextStream stream(TextStream::LineMode::SingleLine);
    stream << path;
    return stream.release();
}

static Ref<Gradient> createGradient()
{
    auto gradient = Gradient::create(Gradient::ConicData { { 0., 0. }, 1.25 }, { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied });
    gradient->addColorStop({ 0.1, Color::red });
    gradient->addColorStop({ 0.5, Color::green });
    gradient->addColorStop({ 0.9, Color::blue });
    return gradient;
}

static Path createComplexPath()
{
    Path path;
    path.moveTo({ 10., 10. });
    path.addLineTo({ 50., 50. });
    path.addQuadCurveTo({ 100., 100. }, { 0., 200. });
    path.addLineTo({ 10., 10. });
    return path;
}

TEST(DisplayListTests, AppendItems)
{
    DisplayList list;

    EXPECT_TRUE(list.isEmpty());

    auto gradient = createGradient();
    auto path = createComplexPath();

    for (int i = 0; i < 50; ++i) {
        list.append(SetInlineStroke(1.5));
        list.append(FillPath(path));
        list.append(FillRectWithGradient(FloatRect { 1., 1., 10., 10. }, gradient));
        list.append(SetInlineFillColor(Color::red));
#if ENABLE(INLINE_PATH_DATA)
        list.append(StrokeLine(PathDataLine { { 0., 0. }, { 10., 15. } }));
#endif
    }

    EXPECT_FALSE(list.isEmpty());

    bool observedUnexpectedItem = false;
    for (const auto& item : list.items()) {
        WTF::switchOn(item,
        [&](const SetInlineStroke& item) {
            EXPECT_EQ(item.thickness(), 1.5);
        }, [&](const FillPath& item) {
            EXPECT_EQ(convertToString(item.path()), convertToString(path));
        }, [&](const FillRectWithGradient& item) {
            EXPECT_EQ(item.rect(), FloatRect(1., 1., 10., 10.));
            EXPECT_EQ(item.gradient().ptr(), gradient.ptr());
        }, [&](const SetInlineFillColor& item) {
            EXPECT_EQ(item.color(), Color::red);
#if ENABLE(INLINE_PATH_DATA)
        }, [&](const StrokeLine& item) {
            EXPECT_EQ(item.start(), FloatPoint(0, 0));
            EXPECT_EQ(item.end(), FloatPoint(10., 15.));
#endif
        }, [&](const auto& item) {
            observedUnexpectedItem = true;
        });
    }

    EXPECT_FALSE(observedUnexpectedItem);

    list.clear();
    EXPECT_TRUE(list.isEmpty());

    list.append(FillRectWithColor(FloatRect { 0, 0, 100, 100 }, Color::black));
    EXPECT_FALSE(list.isEmpty());

    auto* item = get_if<FillRectWithColor>(&list.items()[0]);
    EXPECT_TRUE(item != nullptr);

    if (item) {
        EXPECT_EQ(item->color().tryGetAsSRGBABytes(), Color::black);
        EXPECT_EQ(item->rect(), FloatRect(0, 0, 100, 100));
    }
}

} // namespace TestWebKitAPI
