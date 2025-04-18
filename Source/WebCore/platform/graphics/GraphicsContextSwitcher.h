/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "DestinationColorSpace.h"
#include "FloatRect.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class Filter;
class FilterResults;
class GraphicsContext;

enum class SwitcherState : uint8_t {
    None,
    PaintingSource,
    SourcePainted,
    CycleDetected
};

class GraphicsContextSwitcher {
    WTF_MAKE_TZONE_ALLOCATED(GraphicsContextSwitcher);
public:
    static std::unique_ptr<GraphicsContextSwitcher> create(GraphicsContext& destinationContext, const DestinationColorSpace&, const FloatRect& sourceImageRect, RefPtr<Filter>&&, FilterResults* = nullptr);

    virtual ~GraphicsContextSwitcher() = default;

    SwitcherState state() const { return m_state; }
    void setState(SwitcherState state) { m_state = state; }

    virtual bool hasSourceImage() const { return false; }
    virtual GraphicsContext* drawingContext(GraphicsContext& destinationContext) const { return &destinationContext; }

    virtual SwitcherState beginDrawSourceImage(GraphicsContext& destinationContext, std::optional<FloatRect> clipRect = std::nullopt, float opacity = 1.f) = 0;
    virtual SwitcherState endDrawSourceImage(GraphicsContext& destinationContext) = 0;
    virtual void drawOutputImage(GraphicsContext& destinationContext, const DestinationColorSpace&) = 0;

protected:
    explicit GraphicsContextSwitcher(const FloatRect& sourceImageRect, RefPtr<Filter>&&);

    FloatRect m_sourceImageRect;
    RefPtr<Filter> m_filter;
    SwitcherState m_state { SwitcherState::None };
};

WTF::TextStream& operator<<(WTF::TextStream&, SwitcherState);

} // namespace WebCore
