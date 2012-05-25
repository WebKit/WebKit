/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef RenderGeometryMap_h
#define RenderGeometryMap_h

#include "FloatPoint.h"
#include "FloatQuad.h"
#include "IntSize.h"
#include "RenderObject.h"
#include "TransformationMatrix.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class RenderGeometryMapStep;

// Can be used while walking the Renderer tree to cache data about offsets and transforms.
class RenderGeometryMap {
public:
    RenderGeometryMap();
    ~RenderGeometryMap();

    FloatPoint absolutePoint(const FloatPoint&) const;
    FloatRect absoluteRect(const FloatRect&) const;
    
    // Called by code walking the renderer or layer trees.
    void pushMappingsToAncestor(const RenderObject*, const RenderBoxModelObject* ancestor);
    void popMappingsToAncestor(const RenderBoxModelObject*);
    
    // The following methods should only be called by renderers inside a call to pushMappingsToAncestor().

    // Push geometry info between this renderer and some ancestor. The ancestor must be its container() or some
    // stacking context between the renderer and its container.
    void push(const RenderObject*, const LayoutSize&, bool accumulatingTransform = false, bool isNonUniform = false, bool isFixedPosition = false, bool hasTransform = false);
    void push(const RenderObject*, const TransformationMatrix&, bool accumulatingTransform = false, bool isNonUniform = false, bool isFixedPosition = false, bool hasTransform = false);

    // RenderView gets special treatment, because it applies the scroll offset only for elements inside in fixed position.
    void pushView(const RenderView*, const LayoutSize& scrollOffset, const TransformationMatrix* = 0);

private:
    void mapToAbsolute(TransformState&) const;

    void stepInserted(const RenderGeometryMapStep&);
    void stepRemoved(const RenderGeometryMapStep&);
    
    bool hasNonUniformStep() const { return m_nonUniformStepsCount; }
    bool hasTransformStep() const { return m_transformedStepsCount; }
    bool hasFixedPositionStep() const { return m_fixedStepsCount; }
    
    typedef Vector<OwnPtr<RenderGeometryMapStep> > RenderGeometryMapSteps; // FIXME: inline capacity?
    
    size_t m_insertionPosition;
    int m_nonUniformStepsCount;
    int m_transformedStepsCount;
    int m_fixedStepsCount;
    RenderGeometryMapSteps m_mapping;
    LayoutSize m_accumulatedOffset;
};

} // namespace WebCore

#endif // RenderGeometryMap_h
