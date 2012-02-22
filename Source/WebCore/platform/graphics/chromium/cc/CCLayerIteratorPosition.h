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

#ifndef CCLayerIteratorPosition_h
#define CCLayerIteratorPosition_h

#include "cc/CCLayerTreeHostCommon.h"

#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

// Non-templated constants
struct CCLayerIteratorPositionValue {
    static const int InvalidTargetRenderSurfaceLayerIndex = -1;
    // This must be -1 since the iterator action code assumes that this value can be
    // reached by subtracting one from the position of the first layer in the current
    // target surface's child layer list, which is 0.
    static const int LayerIndexRepresentingTargetRenderSurface = -1;
};

// A struct to hold the iterator's current position, which is passed to the various CCLayerIteratorAction functions, for them to
// read and update. This struct exists so the templated action functions don't need to access the CCLayerIterator class.
//
// There is a 1:1 relationship between an instance of the CCLayerIteratorPosition class and a CCLayerIteratorActions::Foo class,
// so an iterator action class can hold extra position data, if required, in its own class instance.
//
// The current position of the iterator is held in two integer variables.
// - The targetRenderSurfaceLayerIndex is a position in the renderSurfaceLayerList. This points to a layer which owns the current
// target surface. This is a value from 0 to n-1 (n = size of renderSurfaceLayerList = number of surfaces). A value outside of
// this range (for example, CCLayerIteratorPositionValue::InvalidTargetRenderSurfaceLayerIndex) is used to indicate a position
// outside the bounds of the tree.
// - The currentLayerIndex is a position in the list of layers that are children of the current target surface. When pointing to
// one of these layers, this is a value from 0 to n-1 (n = number of children). Since the iterator must also stop at the layers
// representing the target surface, this is done by setting the currentLayerIndex to a value of
// CCLayerIteratorPositionValue::LayerRepresentingTargetRenderSurface.
template<typename LayerType, typename RenderSurfaceType>
struct CCLayerIteratorPosition {
    CCLayerIteratorPosition() : renderSurfaceLayerList(0) { }
    explicit CCLayerIteratorPosition(const Vector<RefPtr<LayerType> >* renderSurfaceLayerList) : renderSurfaceLayerList(renderSurfaceLayerList) { }

    inline LayerType* currentLayer() const { return currentLayerRepresentsTargetRenderSurface() ? targetRenderSurfaceLayer() : targetRenderSurfaceChildren()[currentLayerIndex].get(); }

    inline bool currentLayerRepresentsContributingRenderSurface() const { return CCLayerTreeHostCommon::renderSurfaceContributesToTarget<LayerType>(currentLayer(), targetRenderSurfaceLayer()->id()); }
    inline bool currentLayerRepresentsTargetRenderSurface() const { return currentLayerIndex == CCLayerIteratorPositionValue::LayerIndexRepresentingTargetRenderSurface; }

    inline LayerType* targetRenderSurfaceLayer() const { return (*renderSurfaceLayerList)[targetRenderSurfaceLayerIndex].get(); }
    inline RenderSurfaceType* targetRenderSurface() const { return targetRenderSurfaceLayer()->renderSurface(); }
    inline const Vector<RefPtr<LayerType> >& targetRenderSurfaceChildren() const { return targetRenderSurface()->layerList(); }

    inline bool operator==(const CCLayerIteratorPosition& other) const
    {
        return targetRenderSurfaceLayerIndex == other.targetRenderSurfaceLayerIndex
            && currentLayerIndex == other.currentLayerIndex;
    }

    const Vector<RefPtr<LayerType> >* renderSurfaceLayerList;
    int targetRenderSurfaceLayerIndex;
    int currentLayerIndex;
};

} // namespace WebCore

#endif
