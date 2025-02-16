/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include "Test.h"
#include <WebCore/StyleGradient.h>

namespace TestWebKitAPI {

using namespace WebCore::CSS::Literals;

static WebCore::Style::GradientLinearColorStopList cacheableStops()
{
    return {
        {
            WebCore::Style::Color { WebCore::Style::ResolvedColor { WebCore::Color::red } },
            50_css_percentage
        },
        {
            WebCore::Style::Color { WebCore::Style::ResolvedColor { WebCore::Color::blue } },
            100_css_percentage
        }
    };
}

static WebCore::Style::GradientLinearColorStopList someUncacheableStops()
{
    return {
        {
            WebCore::Style::Color { WebCore::Style::CurrentColor { } },
            50_css_percentage
        },
        {
            WebCore::Style::Color { WebCore::Style::ResolvedColor { WebCore::Color::blue } },
            100_css_percentage
        }
    };
}

static WebCore::Style::GradientLinearColorStopList allUncacheableStops()
{
    return {
        {
            WebCore::Style::Color { WebCore::Style::CurrentColor { } },
            50_css_percentage
        },
        {
            WebCore::Style::Color { WebCore::Style::CurrentColor { } },
            100_css_percentage
        }
    };
}

static WebCore::Style::Gradient gradientWithStops(WebCore::Style::GradientLinearColorStopList stops)
{
    return WebCore::FunctionNotation<WebCore::CSSValueLinearGradient, WebCore::Style::LinearGradient> {
        .parameters = {
            .colorInterpolationMethod = WebCore::CSS::GradientColorInterpolationMethod {
                .method = { WebCore::ColorInterpolationMethod::SRGB { }, WebCore::AlphaPremultiplication::Premultiplied },
                .defaultMethod = WebCore::CSS::GradientColorInterpolationMethod::Default::SRGB,
            },
            .gradientLine = WebCore::CSS::Horizontal { WebCore::CSS::Keyword::Left { } },
            .stops = stops
        }
    };
}

TEST(WebCore, StyleGradientStopsAreCacheable)
{
    auto cacheableGradient = gradientWithStops(cacheableStops());
    ASSERT_TRUE(WebCore::Style::stopsAreCacheable(cacheableGradient));

    auto uncacheableGradient1 = gradientWithStops(someUncacheableStops());
    ASSERT_FALSE(WebCore::Style::stopsAreCacheable(uncacheableGradient1));

    auto uncacheableGradient2 = gradientWithStops(allUncacheableStops());
    ASSERT_FALSE(WebCore::Style::stopsAreCacheable(uncacheableGradient2));
}

} // namespace TestWebKitAPI
