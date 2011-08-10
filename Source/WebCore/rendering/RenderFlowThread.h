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

#ifndef RenderFlowThread_h
#define RenderFlowThread_h


#include "RenderBlock.h"
#include <wtf/ListHashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/UnusedParam.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class RenderStyle;

// RenderFlowThread is used to collect all the render objects that participate in a
// flow thread. It will also help in doing the layout. However, it will not render
// directly to screen. Instead, RenderRegion objects will redirect their paint 
// and nodeAtPoint methods to this object. Each RenderRegion will actually be a viewPort
// of the RenderFlowThread.

class RenderFlowThread: public RenderBlock {
public:
    RenderFlowThread(Node*, const AtomicString& flowThread);

    virtual bool isRenderFlowThread() const { return true; }

    AtomicString flowThread() const { return m_flowThread; }

    // Always create a RenderLayer for the RenderFlowThread, so that we 
    // can easily avoid to draw it's children directly.
    virtual bool requiresLayer() const { return true; }

    RenderObject* nextRendererForNode(Node*) const;
    RenderObject* previousRendererForNode(Node*) const;
    
    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = 0);
    virtual void removeChild(RenderObject*);

private:
    static PassRefPtr<RenderStyle> createFlowThreadStyle();
    
    virtual const char* renderName() const { return "RenderFlowThread"; }
    
    typedef ListHashSet<RenderObject*> FlowThreadChildList;
    FlowThreadChildList m_flowThreadChildList;
    
    AtomicString m_flowThread;
};

inline RenderFlowThread* toRenderFlowThread(RenderObject* object)
{
    ASSERT(!object || object->isRenderFlowThread());
    return static_cast<RenderFlowThread*>(object);
}

inline const RenderFlowThread* toRenderFlowThread(const RenderObject* object)
{
    ASSERT(!object || object->isRenderFlowThread());
    return static_cast<const RenderFlowThread*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderFlowThread(const RenderFlowThread*);

} // namespace WebCore

#endif // RenderFlowThread_h
