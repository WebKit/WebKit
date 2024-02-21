/*
 * Copyright (C) 2022-2023 Apple Inc.  All rights reserved.
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

namespace WebCore {

class Filter;
class FilterResults;
class GraphicsContext;

class FilterTargetSwitcher {
    WTF_MAKE_FAST_ALLOCATED;

public:
    static std::unique_ptr<FilterTargetSwitcher> create(GraphicsContext& destinationContext, Filter&, const FloatRect &sourceImageRect, const DestinationColorSpace&, FilterResults* = nullptr);

    virtual ~FilterTargetSwitcher() = default;

    virtual GraphicsContext* drawingContext(GraphicsContext& destinationContext) const { return &destinationContext; }

    virtual bool hasSourceImage() const { return false; }

    virtual void beginClipAndDrawSourceImage(GraphicsContext& destinationContext, const FloatRect& repaintRect, const FloatRect& clipRect) = 0;
    virtual void endClipAndDrawSourceImage(GraphicsContext& destinationContext, const DestinationColorSpace&) = 0;

    virtual void beginDrawSourceImage(GraphicsContext& destinationContext) = 0;
    virtual void endDrawSourceImage(GraphicsContext& destinationContext, const DestinationColorSpace&) = 0;

protected:
    FilterTargetSwitcher(Filter&);

    RefPtr<Filter> m_filter;
};

} // namespace WebCore
