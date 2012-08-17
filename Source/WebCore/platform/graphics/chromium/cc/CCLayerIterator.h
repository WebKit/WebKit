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

#include "CCLayerTreeHostCommon.h"

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

// Non-templated constants
struct CCLayerIteratorValue {
    static const int InvalidTargetRenderSurfaceLayerIndex = -1;
    // This must be -1 since the iterator action code assumes that this value can be
    // reached by subtracting one from the position of the first layer in the current
    // target surface's child layer list, which is 0.
    static const int LayerIndexRepresentingTargetRenderSurface = -1;
};

// The position of a layer iterator that is independent of its many template types.
template <typename LayerType>
struct CCLayerIteratorPosition {
    bool representsTargetRenderSurface;
    bool representsContributingRenderSurface;
    bool representsItself;
    LayerType* targetRenderSurfaceLayer;
    LayerType* currentLayer;
};

// An iterator class for walking over layers in the RenderSurface-Layer tree.
template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename IteratorActionType>
class CCLayerIterator {
    typedef CCLayerIterator<LayerType, LayerList, RenderSurfaceType, IteratorActionType> CCLayerIteratorType;

public:
    CCLayerIterator() : m_renderSurfaceLayerList(0) { }

    static CCLayerIteratorType begin(const LayerList* renderSurfaceLayerList) { return CCLayerIteratorType(renderSurfaceLayerList, true); }
    static CCLayerIteratorType end(const LayerList* renderSurfaceLayerList) { return CCLayerIteratorType(renderSurfaceLayerList, false); }

    CCLayerIteratorType& operator++() { m_actions.next(*this); return *this; }
    bool operator==(const CCLayerIterator& other) const
    {
        return m_targetRenderSurfaceLayerIndex == other.m_targetRenderSurfaceLayerIndex
            && m_currentLayerIndex == other.m_currentLayerIndex;
    }
    bool operator!=(const CCLayerIteratorType& other) const { return !(*this == other); }

    LayerType* operator->() const { return currentLayer(); }
    LayerType* operator*() const { return currentLayer(); }

    bool representsTargetRenderSurface() const { return currentLayerRepresentsTargetRenderSurface(); }
    bool representsContributingRenderSurface() const { return !representsTargetRenderSurface() && currentLayerRepresentsContributingRenderSurface(); }
    bool representsItself() const { return !representsTargetRenderSurface() && !representsContributingRenderSurface(); }

    LayerType* targetRenderSurfaceLayer() const { return getRawPtr((*m_renderSurfaceLayerList)[m_targetRenderSurfaceLayerIndex]); }

    operator const CCLayerIteratorPosition<LayerType>() const
    {
        CCLayerIteratorPosition<LayerType> position;
        position.representsTargetRenderSurface = representsTargetRenderSurface();
        position.representsContributingRenderSurface = representsContributingRenderSurface();
        position.representsItself = representsItself();
        position.targetRenderSurfaceLayer = targetRenderSurfaceLayer();
        position.currentLayer = currentLayer();
        return position;
    }

private:
    CCLayerIterator(const LayerList* renderSurfaceLayerList, bool start)
        : m_renderSurfaceLayerList(renderSurfaceLayerList)
        , m_targetRenderSurfaceLayerIndex(0)
    {
        for (size_t i = 0; i < renderSurfaceLayerList->size(); ++i) {
            if (!(*renderSurfaceLayerList)[i]->renderSurface()) {
                ASSERT_NOT_REACHED();
                m_actions.end(*this);
                return;
            }
        }

        if (start && !renderSurfaceLayerList->isEmpty())
            m_actions.begin(*this);
        else
            m_actions.end(*this);
    }

    inline static LayerChromium* getRawPtr(const RefPtr<LayerChromium>& ptr) { return ptr.get(); }
    inline static CCLayerImpl* getRawPtr(CCLayerImpl* ptr) { return ptr; }

    inline LayerType* currentLayer() const { return currentLayerRepresentsTargetRenderSurface() ? targetRenderSurfaceLayer() : getRawPtr(targetRenderSurfaceChildren()[m_currentLayerIndex]); }

    inline bool currentLayerRepresentsContributingRenderSurface() const { return CCLayerTreeHostCommon::renderSurfaceContributesToTarget<LayerType>(currentLayer(), targetRenderSurfaceLayer()->id()); }
    inline bool currentLayerRepresentsTargetRenderSurface() const { return m_currentLayerIndex == CCLayerIteratorValue::LayerIndexRepresentingTargetRenderSurface; }

    inline RenderSurfaceType* targetRenderSurface() const { return targetRenderSurfaceLayer()->renderSurface(); }
    inline const LayerList& targetRenderSurfaceChildren() const { return targetRenderSurface()->layerList(); }

    IteratorActionType m_actions;
    const LayerList* m_renderSurfaceLayerList;

    // The iterator's current position.

    // A position in the renderSurfaceLayerList. This points to a layer which owns the current target surface.
    // This is a value from 0 to n-1 (n = size of renderSurfaceLayerList = number of surfaces). A value outside of
    // this range (for example, CCLayerIteratorValue::InvalidTargetRenderSurfaceLayerIndex) is used to
    // indicate a position outside the bounds of the tree.
    int m_targetRenderSurfaceLayerIndex;
    // A position in the list of layers that are children of the current target surface. When pointing to one of
    // these layers, this is a value from 0 to n-1 (n = number of children). Since the iterator must also stop at
    // the layers representing the target surface, this is done by setting the currentLayerIndex to a value of
    // CCLayerIteratorValue::LayerRepresentingTargetRenderSurface.
    int m_currentLayerIndex;

    friend struct CCLayerIteratorActions;
};

// Orderings for iterating over the RenderSurface-Layer tree.
struct CCLayerIteratorActions {
    // Walks layers sorted by z-order from back to front.
    class BackToFront {
    public:
        template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
        void begin(CCLayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>&);

        template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
        void end(CCLayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>&);

        template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
        void next(CCLayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>&);

    private:
        int m_highestTargetRenderSurfaceLayer;
    };

    // Walks layers sorted by z-order from front to back
    class FrontToBack {
    public:
        template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
        void begin(CCLayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>&);

        template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
        void end(CCLayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>&);

        template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
        void next(CCLayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>&);

    private:
        template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
        void goToHighestInSubtree(CCLayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>&);
    };
};

} // namespace WebCore

#endif
