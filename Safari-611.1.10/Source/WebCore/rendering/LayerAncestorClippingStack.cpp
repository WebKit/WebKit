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
#include "LayerAncestorClippingStack.h"

#include "GraphicsLayer.h"
#include "ScrollingConstraints.h"
#include "ScrollingCoordinator.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

LayerAncestorClippingStack::LayerAncestorClippingStack(Vector<CompositedClipData>&& clipDataStack)
{
    m_stack.reserveInitialCapacity(clipDataStack.size());
    for (auto& clipDataEntry : clipDataStack)
        m_stack.uncheckedAppend({ WTFMove(clipDataEntry), 0, nullptr });
}

bool LayerAncestorClippingStack::equalToClipData(const Vector<CompositedClipData>& clipDataStack) const
{
    if (m_stack.size() != clipDataStack.size())
        return false;

    for (unsigned i = 0; i < m_stack.size(); ++i) {
        if (m_stack[i].clipData != clipDataStack[i])
            return false;
    }

    return true;
}

bool LayerAncestorClippingStack::hasAnyScrollingLayers() const
{
    for (const auto& entry : m_stack) {
        if (entry.clipData.isOverflowScroll)
            return true;
    }
    
    return false;
}

void LayerAncestorClippingStack::clear(ScrollingCoordinator* scrollingCoordinator)
{
    for (auto& entry : m_stack) {
        if (entry.overflowScrollProxyNodeID) {
            ASSERT(scrollingCoordinator);
            scrollingCoordinator->unparentChildrenAndDestroyNode(entry.overflowScrollProxyNodeID);
            entry.overflowScrollProxyNodeID = 0;
        }

        GraphicsLayer::unparentAndClear(entry.clippingLayer);
    }
}

void LayerAncestorClippingStack::detachFromScrollingCoordinator(ScrollingCoordinator& scrollingCoordinator)
{
    for (auto& entry : m_stack) {
        if (entry.overflowScrollProxyNodeID) {
            scrollingCoordinator.unparentChildrenAndDestroyNode(entry.overflowScrollProxyNodeID);
            entry.overflowScrollProxyNodeID = 0;
        }
    }
}

GraphicsLayer* LayerAncestorClippingStack::firstClippingLayer() const
{
    return m_stack.first().clippingLayer.get();
}

GraphicsLayer* LayerAncestorClippingStack::lastClippingLayer() const
{
    return m_stack.last().clippingLayer.get();
}

ScrollingNodeID LayerAncestorClippingStack::lastOverflowScrollProxyNodeID() const
{
    for (auto& entry : WTF::makeReversedRange(m_stack)) {
        if (entry.overflowScrollProxyNodeID)
            return entry.overflowScrollProxyNodeID;
    }
    
    return 0;
}

void LayerAncestorClippingStack::updateScrollingNodeLayers(ScrollingCoordinator& scrollingCoordinator)
{
    for (const auto& entry : m_stack) {
        if (!entry.clipData.isOverflowScroll)
            continue;

        scrollingCoordinator.setNodeLayers(entry.overflowScrollProxyNodeID, { entry.clippingLayer.get() });
    }
}

bool LayerAncestorClippingStack::updateWithClipData(ScrollingCoordinator* scrollingCoordinator, Vector<CompositedClipData>&& clipDataStack)
{
    bool stackChanged = false;

    int clipEntryCount = clipDataStack.size();
    int stackEntryCount = m_stack.size();
    for (int i = 0; i < clipEntryCount; ++i) {
        auto& clipDataEntry = clipDataStack[i];
        
        if (i >= stackEntryCount) {
            m_stack.append({ WTFMove(clipDataEntry), 0, nullptr });
            stackChanged = true;
            continue;
        }
        
        auto& existingEntry = m_stack[i];
        
        if (existingEntry.clipData != clipDataEntry)
            stackChanged = true;

        if (existingEntry.clipData.isOverflowScroll && !clipDataEntry.isOverflowScroll) {
            ASSERT(scrollingCoordinator);
            scrollingCoordinator->unparentChildrenAndDestroyNode(existingEntry.overflowScrollProxyNodeID);
            existingEntry.overflowScrollProxyNodeID = 0;
        }
        
        existingEntry.clipData = WTFMove(clipDataEntry);
    }
    
    if (stackEntryCount > clipEntryCount) {
        for (int i = clipEntryCount; i < stackEntryCount; ++i) {
            auto& entry = m_stack[i];
            if (entry.overflowScrollProxyNodeID) {
                ASSERT(scrollingCoordinator);
                scrollingCoordinator->unparentChildrenAndDestroyNode(entry.overflowScrollProxyNodeID);
            }
            GraphicsLayer::unparentAndClear(entry.clippingLayer);
        }

        m_stack.shrink(clipEntryCount);
        stackChanged = true;
    } else
        m_stack.shrinkToFit();

    return stackChanged;
}

Vector<CompositedClipData> LayerAncestorClippingStack::compositedClipData() const
{
    Vector<CompositedClipData> clipData;
    clipData.reserveInitialCapacity(m_stack.size());

    for (const auto& entry : m_stack)
        clipData.uncheckedAppend(entry.clipData);

    return clipData;
}

static TextStream& operator<<(TextStream& ts, const LayerAncestorClippingStack::ClippingStackEntry& entry)
{
    ts.dumpProperty("layer", entry.clipData.clippingLayer.get());
    ts.dumpProperty("clip", entry.clipData.clipRect);
    ts.dumpProperty("isOverflowScroll", entry.clipData.isOverflowScroll);
    if (entry.overflowScrollProxyNodeID)
        ts.dumpProperty("overflowScrollProxyNodeID", entry.overflowScrollProxyNodeID);
    if (entry.clippingLayer)
        ts.dumpProperty("clippingLayer", entry.clippingLayer->primaryLayerID());
    return ts;
}

TextStream& operator<<(TextStream& ts, const LayerAncestorClippingStack& clipStack)
{
    TextStream multilineStream;
    multilineStream.setIndent(ts.indent() + 2);

    {
        TextStream::GroupScope scope(ts);
        multilineStream << "LayerAncestorClippingStack";

        for (unsigned i = 0; i < clipStack.stack().size(); ++i) {
            auto& entry = clipStack.stack()[i];
            TextStream::GroupScope entryScope(multilineStream);
            multilineStream << "entry " << i;
            multilineStream << entry;
        }

        ts << multilineStream.release();
    }
    return ts;
}

} // namespace WebCore

