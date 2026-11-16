/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/FloatRect.h>
#include <WebCore/IntRect.h>
#include <type_traits>

namespace WebCore {

enum class ViewCoordinateSpace : uint8_t {
    View,
    Contents,
    RootView,
    MainFrameView,
};

template<ViewCoordinateSpace space, typename RectType = FloatRect>
class ViewCoordinateSpaceRect {
public:
    constexpr ViewCoordinateSpaceRect() = default;
    constexpr explicit ViewCoordinateSpaceRect(const RectType& rect)
        : m_rect(rect)
    {
    }

    constexpr const RectType& raw() const { return m_rect; }

    void intersect(const ViewCoordinateSpaceRect& other) { m_rect.intersect(other.m_rect); }

    friend bool operator==(const ViewCoordinateSpaceRect&, const ViewCoordinateSpaceRect&) = default;

private:
    RectType m_rect;
};

using ViewFloatRect = ViewCoordinateSpaceRect<ViewCoordinateSpace::View, FloatRect>;
using ViewIntRect = ViewCoordinateSpaceRect<ViewCoordinateSpace::View, IntRect>;
using ContentsFloatRect = ViewCoordinateSpaceRect<ViewCoordinateSpace::Contents, FloatRect>;
using ContentsIntRect = ViewCoordinateSpaceRect<ViewCoordinateSpace::Contents, IntRect>;
using RootViewFloatRect = ViewCoordinateSpaceRect<ViewCoordinateSpace::RootView, FloatRect>;
using RootViewIntRect = ViewCoordinateSpaceRect<ViewCoordinateSpace::RootView, IntRect>;
using MainFrameViewFloatRect = ViewCoordinateSpaceRect<ViewCoordinateSpace::MainFrameView, FloatRect>;
using MainFrameViewIntRect = ViewCoordinateSpaceRect<ViewCoordinateSpace::MainFrameView, IntRect>;

template<typename RectType> constexpr ViewCoordinateSpaceRect<ViewCoordinateSpace::View, RectType> inViewSpace(const RectType& rect) { return ViewCoordinateSpaceRect<ViewCoordinateSpace::View, RectType> { rect }; }
template<typename RectType> constexpr ViewCoordinateSpaceRect<ViewCoordinateSpace::Contents, RectType> inContentsSpace(const RectType& rect) { return ViewCoordinateSpaceRect<ViewCoordinateSpace::Contents, RectType> { rect }; }
template<typename RectType> constexpr ViewCoordinateSpaceRect<ViewCoordinateSpace::RootView, RectType> inRootViewSpace(const RectType& rect) { return ViewCoordinateSpaceRect<ViewCoordinateSpace::RootView, RectType> { rect }; }
template<typename RectType> constexpr ViewCoordinateSpaceRect<ViewCoordinateSpace::MainFrameView, RectType> inMainFrameViewSpace(const RectType& rect) { return ViewCoordinateSpaceRect<ViewCoordinateSpace::MainFrameView, RectType> { rect }; }

static_assert(sizeof(ContentsFloatRect) == sizeof(FloatRect));
static_assert(sizeof(ContentsIntRect) == sizeof(IntRect));
static_assert(std::is_trivially_copyable_v<ContentsFloatRect>);
static_assert(!std::is_convertible_v<FloatRect, ContentsFloatRect>);
static_assert(!std::is_convertible_v<ContentsFloatRect, RootViewFloatRect>);
static_assert(!std::is_convertible_v<RootViewFloatRect, MainFrameViewFloatRect>);

} // namespace WebCore
