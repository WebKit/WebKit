/*
 * Copyright (C) 2017-2023 Apple Inc.  All rights reserved.
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

#include "IntRect.h"
#include "IntSize.h"
#include <optional>
#include <variant>

namespace WebCore {

enum class DecodingMode : uint8_t {
    Auto,
    Synchronous,
    Asynchronous
};

using DecodingGeometry = std::variant<
    std::monostate,
    IntSize,
    IntRect
>;

class DecodingOptions {
public:
    DecodingOptions(DecodingMode decodingMode = DecodingMode::Synchronous, const DecodingGeometry& decodingGeometry = { })
        : m_decodingMode(decodingMode)
        , m_decodingGeometry(decodingGeometry)
    {
    }

    friend bool operator==(const DecodingOptions&, const DecodingOptions&) = default;

    DecodingMode decodingMode() const { return m_decodingMode; }
    bool isAuto() const { return m_decodingMode == DecodingMode::Auto; }
    bool isSynchronous() const { return m_decodingMode == DecodingMode::Synchronous; }
    bool isAsynchronous() const { return m_decodingMode == DecodingMode::Asynchronous; }

    DecodingGeometry decodingGeometry() const { return m_decodingGeometry; }

    std::optional<IntSize> sizeForDrawing() const
    {
        if (const auto* sizeForDrawing = std::get_if<IntSize>(&m_decodingGeometry))
            return *sizeForDrawing;
        return std::nullopt;
    }

    std::optional<IntRect> rectForDrawing() const
    {
        if (const auto* rectForDrawing = std::get_if<IntRect>(&m_decodingGeometry))
            return *rectForDrawing;
        return std::nullopt;
    }

    bool hasSizeForDrawing() const { return !!sizeForDrawing(); }
    bool hasRectForDrawing() const { return !!rectForDrawing(); }
    bool hasFullSize() const { return !hasSizeForDrawing() && !hasRectForDrawing(); }

    bool isCompatibleWith(const DecodingOptions& other) const
    {
        if (isAuto() || other.isAuto())
            return false;

        if (hasFullSize())
            return true;

        if (m_decodingGeometry.index() != other.decodingGeometry().index())
            return false;

        if (auto sizeForDrawing = this->sizeForDrawing())
            return sizeForDrawing->maxDimension() >= other.sizeForDrawing()->maxDimension();

        auto rectForDrawing = this->rectForDrawing();
        return rectForDrawing->contains(*other.rectForDrawing());
    }

private:
    DecodingMode m_decodingMode;
    DecodingGeometry m_decodingGeometry;
};

TextStream& operator<<(TextStream&, DecodingMode);

} // namespace WebCore
