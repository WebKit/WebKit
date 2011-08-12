/*
 * Copyright 2011 Adobe Systems Incorporated. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef RenderRegion_h
#define RenderRegion_h

#include "RenderBox.h"

namespace WebCore {

class RenderFlowThread;

class RenderRegion : public RenderBox {
public:
    explicit RenderRegion(Node*, RenderFlowThread*);
    virtual ~RenderRegion();

    virtual bool isRenderRegion() const { return true; }

    virtual void layout();
    virtual void paint(PaintInfo&, const LayoutPoint&);

    void setRegionRect(const IntRect& rect) { m_regionRect = rect; }
    IntRect regionRect() const { return m_regionRect; }

private:
    virtual const char* renderName() const { return "RenderRegion"; }

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

    RenderFlowThread* m_flowThread;
    IntRect m_regionRect;
};

inline RenderRegion* toRenderRegion(RenderObject* object)
{
    ASSERT(!object || object->isRenderRegion());
    return static_cast<RenderRegion*>(object);
}

inline const RenderRegion* toRenderRegion(const RenderObject* object)
{
    ASSERT(!object || object->isRenderRegion());
    return static_cast<const RenderRegion*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderRegion(const RenderRegion*);

} // namespace WebCore

#endif // RenderRegion_h
