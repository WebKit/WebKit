/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "config.h"
#include "LayerOverlapMap.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

struct RectList {
    Vector<LayoutRect> rects;
    LayoutRect boundingRect;
    
    void append(const LayoutRect& rect)
    {
        rects.append(rect);
        boundingRect.unite(rect);
    }

    void append(const RectList& rectList)
    {
        rects.appendVector(rectList.rects);
        boundingRect.unite(rectList.boundingRect);
    }
    
    bool intersects(const LayoutRect& rect) const
    {
        if (!rects.size() || !rect.intersects(boundingRect))
            return false;

        for (const auto& currentRect : rects) {
            if (currentRect.intersects(rect))
                return true;
        }
        return false;
    }
};

class OverlapMapContainer {
public:
    void add(const LayoutRect& bounds)
    {
        m_rectList.append(bounds);
    }

    bool overlapsLayers(const LayoutRect& bounds) const
    {
        return m_rectList.intersects(bounds);
    }

    void unite(const OverlapMapContainer& otherContainer)
    {
        m_rectList.append(otherContainer.m_rectList);
    }
    
    const RectList& rectList() const { return m_rectList; }

private:
    RectList m_rectList;
};

LayerOverlapMap::LayerOverlapMap()
    : m_geometryMap(UseTransforms)
{
    // Begin assuming the root layer will be composited so that there is
    // something on the stack. The root layer should also never get an
    // popCompositingContainer call.
    pushCompositingContainer();
}

LayerOverlapMap::~LayerOverlapMap() = default;

void LayerOverlapMap::add(const LayoutRect& bounds)
{
    // Layers do not contribute to overlap immediately--instead, they will
    // contribute to overlap as soon as their composited ancestor has been
    // recursively processed and popped off the stack.
    ASSERT(m_overlapStack.size() >= 2);
    m_overlapStack[m_overlapStack.size() - 2]->add(bounds);
    m_isEmpty = false;
}

bool LayerOverlapMap::overlapsLayers(const LayoutRect& bounds) const
{
    return m_overlapStack.last()->overlapsLayers(bounds);
}

void LayerOverlapMap::pushCompositingContainer()
{
    m_overlapStack.append(std::make_unique<OverlapMapContainer>());
}

void LayerOverlapMap::popCompositingContainer()
{
    m_overlapStack[m_overlapStack.size() - 2]->unite(*m_overlapStack.last());
    m_overlapStack.removeLast();
}

static TextStream& operator<<(TextStream& ts, const RectList& rectList)
{
    ts << "bounds " << rectList.boundingRect << " (" << rectList.rects << " rects)";
    return ts;
}

static TextStream& operator<<(TextStream& ts, const OverlapMapContainer& container)
{
    ts << container.rectList();
    return ts;
}

TextStream& operator<<(TextStream& ts, const LayerOverlapMap& overlapMap)
{
    TextStream multilineStream;

    TextStream::GroupScope scope(ts);
    multilineStream << indent << "LayerOverlapMap\n";

    for (auto& container : overlapMap.overlapStack())
        multilineStream << "  " << *container << "\n";

    ts << multilineStream.release();
    return ts;
}

} // namespace WebCore
