/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCLayerIterator_h
#define CCLayerIterator_h

#include "cc/CCLayerIteratorPosition.h"

#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

// These classes provide means to iterate over the RenderSurface-Layer tree.

// Example code follows, for a tree of LayerChromium/RenderSurfaceChromium objects. See below for details.
//
// void doStuffOnLayers(const Vector<RefPtr<LayerChromium> >& renderSurfaceLayerList)
// {
//     typedef CCLayerIterator<LayerChromium, RenderSurfaceChromium, CCLayerIteratorActions::FrontToBack> CCLayerIteratorType;
//
//     CCLayerIteratorType end = CCLayerIteratorType::end(&renderSurfaceLayerList);
//     for (CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList); it != end; ++it) {
//         // Only one of these will be true
//         if (it.representsTargetRenderSurface())
//             foo(*it); // *it is a layer representing a target RenderSurface
//         if (it.representsContributingRenderSurface())
//             bar(*it); // *it is a layer representing a RenderSurface that contributes to the layer's target RenderSurface
//         if (it.representsItself())
//             baz(*it); // *it is a layer representing itself, as it contributes to its own target RenderSurface
//     }
// }

// A RenderSurface R may be referred to in one of two different contexts. One RenderSurface is "current" at any time, for
// whatever operation is being performed. This current surface is referred to as a target surface. For example, when R is
// being painted it would be the target surface. Once R has been painted, its contents may be included into another
// surface S. While S is considered the target surface when it is being painted, R is called a contributing surface
// in this context as it contributes to the content of the target surface S.
//
// The iterator's current position in the tree always points to some layer. The state of the iterator indicates the role of the
// layer, and will be one of the following three states. A single layer L will appear in the iteration process in at least one,
// and possibly all, of these states.
// 1. Representing the target surface: The iterator in this state, pointing at layer L, indicates that the target RenderSurface
// is now the surface owned by L. This will occur exactly once for each RenderSurface in the tree.
// 2. Representing a contributing surface: The iterator in this state, pointing at layer L, refers to the RenderSurface owned
// by L as a contributing surface, without changing the current target RenderSurface.
// 3. Representing itself: The iterator in this state, pointing at layer L, refers to the layer itself, as a child of the
// current target RenderSurface.
//
// The BackToFront iterator will return a layer representing the target surface before returning layers representing themselves
// as children of the current target surface. Whereas the FrontToBack ordering will iterate over children layers of a surface
// before the layer representing the surface as a target surface.
//
// To use the iterators:
//
// Create a stepping iterator and end iterator by calling CCLayerIterator::begin() and CCLayerIterator::end() and passing in the
// list of layers owning target RenderSurfaces. Step through the tree by incrementing the stepping iterator while it is != to
// the end iterator. At each step the iterator knows what the layer is representing, and you can query the iterator to decide
// what actions to perform with the layer given what it represents.

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// An iterator class for walking over layers in the RenderSurface-Layer tree.
template <typename LayerType, typename RenderSurfaceType, typename IteratorActionType>
class CCLayerIterator {
    typedef CCLayerIterator<LayerType, RenderSurfaceType, IteratorActionType> CCLayerIteratorType;

public:
    CCLayerIterator() { }

    static CCLayerIteratorType begin(const Vector<RefPtr<LayerType> >* renderSurfaceLayerList) { return CCLayerIteratorType(renderSurfaceLayerList, true); }
    static CCLayerIteratorType end(const Vector<RefPtr<LayerType> >* renderSurfaceLayerList) { return CCLayerIteratorType(renderSurfaceLayerList, false); }

    CCLayerIteratorType& operator++() { ASSERT(m_actions); m_actions->next(m_position); return *this; }
    bool operator==(const CCLayerIteratorType& other) const { return m_position == other.m_position; }
    bool operator!=(const CCLayerIteratorType& other) const { return !(*this == other); }

    LayerType* operator->() const { return m_position.currentLayer(); }
    LayerType* operator*() const { return m_position.currentLayer(); }

    bool representsTargetRenderSurface() const { return m_position.currentLayerRepresentsTargetRenderSurface(); }
    bool representsContributingRenderSurface() const { return !representsTargetRenderSurface() && m_position.currentLayerRepresentsContributingRenderSurface(); }
    bool representsItself() const { return !representsTargetRenderSurface() && !representsContributingRenderSurface(); }

    LayerType* targetRenderSurfaceLayer() const { return m_position.targetRenderSurfaceLayer(); }

private:
    CCLayerIterator(const Vector<RefPtr<LayerType> >* renderSurfaceLayerList, bool start)
        : m_position(renderSurfaceLayerList)
        , m_actions(adoptPtr(new IteratorActionType()))
    {
        if (start && !renderSurfaceLayerList->isEmpty())
            m_actions->begin(m_position);
        else
            m_actions->end(m_position);
    }

    CCLayerIteratorPosition<LayerType, RenderSurfaceType> m_position;
    OwnPtr<IteratorActionType> m_actions;
};

// Orderings for iterating over the RenderSurface-Layer tree.
struct CCLayerIteratorActions {
    // Walks layers sorted by z-order from back to front.
    class BackToFront {
    public:
        template <typename LayerType, typename RenderSurfaceType>
        void begin(CCLayerIteratorPosition<LayerType, RenderSurfaceType>&);

        template <typename LayerType, typename RenderSurfaceType>
        void end(CCLayerIteratorPosition<LayerType, RenderSurfaceType>&);

        template <typename LayerType, typename RenderSurfaceType>
        void next(CCLayerIteratorPosition<LayerType, RenderSurfaceType>&);

    private:
        int m_highestTargetRenderSurfaceLayer;
    };

    // Walks layers sorted by z-order from front to back
    class FrontToBack {
    public:
        template <typename LayerType, typename RenderSurfaceType>
        void begin(CCLayerIteratorPosition<LayerType, RenderSurfaceType>&);

        template <typename LayerType, typename RenderSurfaceType>
        void end(CCLayerIteratorPosition<LayerType, RenderSurfaceType>&);

        template <typename LayerType, typename RenderSurfaceType>
        void next(CCLayerIteratorPosition<LayerType, RenderSurfaceType>&);

    private:
        template <typename LayerType, typename RenderSurfaceType>
        void goToHighestInSubtree(CCLayerIteratorPosition<LayerType, RenderSurfaceType>&);
    };
};

} // namespace WebCore

#endif
