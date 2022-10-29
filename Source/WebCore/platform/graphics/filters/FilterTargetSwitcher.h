/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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
#include <functional>

namespace WebCore {

class Filter;
class GraphicsContext;
class ImageBuffer;

class FilterTargetSwitcher {
public:
    class Client {
    public:
        virtual ~Client() = default;
        virtual RefPtr<Filter> createFilter(const std::function<FloatRect()>& boundsProvider) const = 0;
        virtual IntOutsets calculateFilterOutsets(const FloatRect& bounds) const = 0;
        virtual GraphicsContext* drawingContext() const = 0;
        virtual void setTargetSwitcher(FilterTargetSwitcher*) = 0;
    protected:
        Client() = default;
    };

    FilterTargetSwitcher(Client&, const DestinationColorSpace&, const std::function<FloatRect()>& boundsProvider);
    FilterTargetSwitcher(Client&, const DestinationColorSpace&, const FloatRect& bounds);
    ~FilterTargetSwitcher();
    GraphicsContext* drawingContext() const;
    FloatBoxExtent outsets() const;

private:
    Client& m_client;
    RefPtr<Filter> m_filter;
    RefPtr<ImageBuffer> m_imageBuffer;
    FloatRect m_bounds;
};

} // namespace WebCore
