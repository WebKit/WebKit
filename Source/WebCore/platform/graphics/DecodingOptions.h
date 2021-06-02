/*
 * Copyright (C) 2017 Apple Inc.  All rights reserved.
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

#include "IntSize.h"
#include <optional>
#include <wtf/EnumTraits.h>
#include <wtf/Variant.h>

namespace WebCore {

enum class DecodingMode : uint8_t {
    Auto,
    Synchronous,
    Asynchronous
};

class DecodingOptions {
public:
    explicit DecodingOptions(DecodingMode decodingMode = DecodingMode::Auto)
        : m_decodingModeOrSize(decodingMode)
    {
    }

    DecodingOptions(const std::optional<IntSize>& sizeForDrawing)
        : m_decodingModeOrSize(sizeForDrawing)
    {
    }

    bool operator==(const DecodingOptions& other) const
    {
        return m_decodingModeOrSize == other.m_decodingModeOrSize;
    }

    bool isAuto() const
    {
        return hasDecodingMode() && WTF::get<DecodingMode>(m_decodingModeOrSize) == DecodingMode::Auto;
    }
    
    bool isSynchronous() const
    {
        return hasDecodingMode() && WTF::get<DecodingMode>(m_decodingModeOrSize) == DecodingMode::Synchronous;
    }

    bool isAsynchronous() const
    {
        return hasDecodingMode() && WTF::get<DecodingMode>(m_decodingModeOrSize) == DecodingMode::Asynchronous;
    }

    bool isAsynchronousCompatibleWith(const DecodingOptions& decodingOptions) const
    {
        if (isAuto() || decodingOptions.isAuto())
            return false;

        // Comparing DecodingOptions with isAsynchronous() should not happen.
        ASSERT(!isAsynchronous());
        if (isAsynchronous() || decodingOptions.isSynchronous())
            return false;
        
        // If the image was synchronously decoded, then it should fit any size.
        // If we want an image regardless of its size, then the current decoded
        // image should be fine.
        if (isSynchronous() || decodingOptions.isAsynchronous())
            return true;

        ASSERT(decodingOptions.hasSize());
        if (decodingOptions.hasFullSize())
            return hasFullSize();

        ASSERT(decodingOptions.hasSizeForDrawing());
        if (hasFullSize())
            return true;

        ASSERT(hasSizeForDrawing());
        return maxDimension(*sizeForDrawing()) >= maxDimension(*decodingOptions.sizeForDrawing());
    }

    bool hasFullSize() const
    {
        return hasSize() && !sizeForDrawing();
    }

    bool hasSizeForDrawing() const
    {
        return hasSize() && sizeForDrawing();
    }

    std::optional<IntSize> sizeForDrawing() const
    {
        ASSERT(hasSize());
        return WTF::get<std::optional<IntSize>>(m_decodingModeOrSize);
    }

    static int maxDimension(const IntSize& size)
    {
        return std::max(size.width(), size.height());
    }

private:
    template<typename T>
    bool has() const
    {
        return WTF::holds_alternative<T>(m_decodingModeOrSize);
    }

    bool hasDecodingMode() const
    {
        return has<DecodingMode>();
    }

    bool hasSize() const
    {
        return has<std::optional<IntSize>>();
    }

    // Four states of the decoding:
    // - Synchronous: DecodingMode::Synchronous
    // - Asynchronous + anySize: DecodingMode::Asynchronous
    // - Asynchronous + intrinsicSize: an empty std::optional<IntSize>>
    // - Asynchronous + sizeForDrawing: a none empty std::optional<IntSize>>
    using DecodingModeOrSize = Variant<DecodingMode, std::optional<IntSize>>;
    DecodingModeOrSize m_decodingModeOrSize;
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::DecodingMode> {
    using values = EnumValues<
    WebCore::DecodingMode,
    WebCore::DecodingMode::Auto,
    WebCore::DecodingMode::Synchronous,
    WebCore::DecodingMode::Asynchronous
    >;
};

} // namespace WTF
