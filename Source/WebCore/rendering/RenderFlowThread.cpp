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

#include "config.h"

#include "RenderFlowThread.h"
#include "Node.h"

namespace WebCore {

RenderFlowThread::RenderFlowThread(Node* node, const AtomicString& flowThread)
      : RenderBlock(node)
      , m_flowThread(flowThread)
{
    setIsAnonymous(false);
    setStyle(createFlowThreadStyle());
}

PassRefPtr<RenderStyle> RenderFlowThread::createFlowThreadStyle()
{
    RefPtr<RenderStyle> newStyle(RenderStyle::create());
    newStyle->setDisplay(BLOCK);
    newStyle->setPosition(AbsolutePosition);
    newStyle->setLeft(Length(0, Fixed));
    newStyle->setTop(Length(0, Fixed));
    newStyle->setWidth(Length(100, Percent));
    newStyle->setHeight(Length(100, Percent));
    newStyle->setOverflowX(OHIDDEN);
    newStyle->setOverflowY(OHIDDEN);
    newStyle->font().update(0);
    
    return newStyle.release();
}

RenderObject* RenderFlowThread::nextRendererForNode(Node* node) const
{
    FlowThreadChildList::const_iterator it = m_flowThreadChildList.begin();
    FlowThreadChildList::const_iterator end = m_flowThreadChildList.end();
    
    for (; it != end; ++it) {
        RenderObject* child = *it;
        ASSERT(child->node());
        unsigned short position = node->compareDocumentPosition(child->node());
        if (position & Node::DOCUMENT_POSITION_FOLLOWING)
            return child;
    }
    
    return 0;
}

RenderObject* RenderFlowThread::previousRendererForNode(Node* node) const
{
    if (m_flowThreadChildList.isEmpty())
        return 0;
    
    FlowThreadChildList::const_iterator begin = m_flowThreadChildList.begin();
    FlowThreadChildList::const_iterator end = m_flowThreadChildList.end();
    FlowThreadChildList::const_iterator it = end;
    
    do {
        --it;
        RenderObject* child = *it;
        ASSERT(child->node());
        unsigned short position = node->compareDocumentPosition(child->node());
        if (position & Node::DOCUMENT_POSITION_PRECEDING)
            return child;
    } while (it != begin);
    
    return 0;
}

void RenderFlowThread::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    if (beforeChild)
        m_flowThreadChildList.insertBefore(beforeChild, newChild);
    else
        m_flowThreadChildList.add(newChild);
    RenderBlock::addChild(newChild, beforeChild);
}

void RenderFlowThread::removeChild(RenderObject* child)
{
    m_flowThreadChildList.remove(child);
    RenderBlock::removeChild(child);
}

} // namespace WebCore
