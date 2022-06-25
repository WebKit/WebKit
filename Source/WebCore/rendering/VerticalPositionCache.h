/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "FontBaseline.h"
#include <wtf/HashMap.h>

namespace WebCore {

class RenderObject;

class VerticalPositionCache {
    WTF_MAKE_NONCOPYABLE(VerticalPositionCache);
public:
    VerticalPositionCache()
    { }
    
    bool get(RenderObject* renderer, FontBaseline baselineType, LayoutUnit& result) const
    {
        const HashMap<RenderObject*, LayoutUnit>& mapToCheck = baselineType == AlphabeticBaseline ? m_alphabeticPositions : m_ideographicPositions;
        const HashMap<RenderObject*, LayoutUnit>::const_iterator it = mapToCheck.find(renderer);
        if (it == mapToCheck.end())
            return false;

        result = it->value;
        return true;
    }
    
    void set(RenderObject* renderer, FontBaseline baselineType, LayoutUnit position)
    {
        if (baselineType == AlphabeticBaseline)
            m_alphabeticPositions.set(renderer, position);
        else
            m_ideographicPositions.set(renderer, position);
    }

private:
    HashMap<RenderObject*, LayoutUnit> m_alphabeticPositions;
    HashMap<RenderObject*, LayoutUnit> m_ideographicPositions;
};

} // namespace WebCore
