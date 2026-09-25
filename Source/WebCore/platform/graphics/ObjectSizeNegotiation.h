/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/FloatSize.h>
#include <WebCore/ImageOrientation.h>
#include <WebCore/NaturalDimensions.h>
#include <WebCore/PlatformExportMacros.h>
#include <optional>

namespace WebCore::ObjectSizeNegotiation {

inline constexpr FloatSize defaultObjectSize { 300, 150 };

struct SpecifiedSize {
    std::optional<float> definiteWidth;
    std::optional<float> definiteHeight;

    static SpecifiedSize none() { return { }; }
};

struct Inputs {
    FloatSize defaultObjectSize;

    float density { 1 };
};

class ConcreteObjectSize;

WEBCORE_EXPORT ConcreteObjectSize defaultSizingAlgorithm(NaturalDimensions, SpecifiedSize, const Inputs&);

WEBCORE_EXPORT ConcreteObjectSize resolveContainConstraint(NaturalDimensions, FloatSize constraintRectangle, const Inputs&);

WEBCORE_EXPORT ConcreteObjectSize resolveCoverConstraint(NaturalDimensions, FloatSize constraintRectangle, const Inputs&);

class ConcreteObjectSize {
public:
    static ConcreteObjectSize fixed(FloatSize size, float zoom = 1) { return { size, zoom }; }

    static ConcreteObjectSize zeroSize() { return { { }, 1 }; }

    static ConcreteObjectSize fromIPCData(FloatSize size, float zoom) { return { size, zoom }; }

    FloatSize size() const { return m_size; }

    float zoom() const { return m_zoom; }

    bool operator==(const ConcreteObjectSize&) const = default;

private:
    friend ConcreteObjectSize defaultSizingAlgorithm(NaturalDimensions, SpecifiedSize, const Inputs&);
    friend ConcreteObjectSize resolveContainConstraint(NaturalDimensions, FloatSize, const Inputs&);
    friend ConcreteObjectSize resolveCoverConstraint(NaturalDimensions, FloatSize, const Inputs&);

    constexpr ConcreteObjectSize(FloatSize size, float zoom)
        : m_size(size)
        , m_zoom(zoom)
    {
    }

    FloatSize m_size;
    float m_zoom { 1 };
};

} // namespace WebCore::ObjectSizeNegotiation

namespace WebCore {

using ObjectSizeNegotiation::ConcreteObjectSize;

} // namespace WebCore
