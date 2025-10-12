/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <WebCore/IntSize.h>
#include <wtf/MathExtras.h>

#if USE(CG)
typedef struct CGSize CGSize;
#endif

namespace WebCore {

class FloatSize;

class DoubleSize {
public:
    constexpr DoubleSize() = default;
    constexpr DoubleSize(double width, double height)
        : m_width(width), m_height(height) { }
    explicit DoubleSize(const FloatSize&&);

    constexpr double width() const { return m_width; }
    constexpr double height() const { return m_height; }


#if USE(CG)
    WEBCORE_EXPORT DoubleSize(const CGSize&);
    WEBCORE_EXPORT operator CGSize() const;
    CGSize toCG() const { return static_cast<CGSize>(*this); }
#endif

    DoubleSize scaledBy(double scaleX, double scaleY) const
    {
        return DoubleSize(m_width * scaleX, m_height * scaleY);
    }

    DoubleSize scaledBy(double scale) const
    {
        return scaledBy(scale, scale);
    }

    friend bool operator==(const DoubleSize&, const DoubleSize&) = default;

private:
    double m_width { 0 };
    double m_height { 0 };
};

} // namespace WebCore

