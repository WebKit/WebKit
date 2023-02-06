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

#include "FilterEffectGeometry.h"
#include <wtf/Vector.h>

namespace WebCore {

struct SVGFilterExpressionNode {
    unsigned index;
    std::optional<FilterEffectGeometry> geometry;
    unsigned level;
    
    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<SVGFilterExpressionNode> decode(Decoder&);
};

using SVGFilterExpressionReference = Vector<SVGFilterExpressionNode>;

template<class Encoder>
void SVGFilterExpressionNode::encode(Encoder& encoder) const
{
    encoder << index;
    encoder << geometry;
    encoder << level;
}

template<class Decoder>
std::optional<SVGFilterExpressionNode> SVGFilterExpressionNode::decode(Decoder& decoder)
{
    std::optional<unsigned> index;
    decoder >> index;
    if (!index)
        return std::nullopt;

    std::optional<std::optional<FilterEffectGeometry>> geometry;
    decoder >> geometry;
    if (!geometry)
        return std::nullopt;

    std::optional<unsigned> level;
    decoder >> level;
    if (!level)
        return std::nullopt;

    return SVGFilterExpressionNode { *index, *geometry, *level };
}

} // namespace WebCore
