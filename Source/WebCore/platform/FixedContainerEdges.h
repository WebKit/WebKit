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

#include <WebCore/Color.h>
#include <WebCore/RectEdges.h>

namespace WebCore {

enum class PredominantColorType : uint8_t {
    None,
    Multiple,
};

using FixedContainerEdge = Variant<PredominantColorType, Color>;

struct FixedContainerEdges {
    WTF_MAKE_TZONE_ALLOCATED(FixedContainerEdges);
public:
    RectEdges<FixedContainerEdge> colors { PredominantColorType::None, PredominantColorType::None, PredominantColorType::None, PredominantColorType::None };

    FixedContainerEdges() = default;
    FixedContainerEdges(const FixedContainerEdges&) = default;
    FixedContainerEdges(RectEdges<FixedContainerEdge>&& edgeColors)
        : colors { WTFMove(edgeColors) }
    {
    }

    FixedContainerEdges(FixedContainerEdges&& other)
        : colors { WTFMove(other.colors) }
    {
    }

    FixedContainerEdges& operator=(const FixedContainerEdges&) = default;

    WEBCORE_EXPORT bool hasFixedEdge(BoxSide) const;
    WEBCORE_EXPORT Color predominantColor(BoxSide) const;
    WEBCORE_EXPORT BoxSideSet fixedEdges() const;

    bool operator==(const FixedContainerEdges&) const = default;
};

} // namespace WebCore
