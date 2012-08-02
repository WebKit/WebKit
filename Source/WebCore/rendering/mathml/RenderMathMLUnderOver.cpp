/*
 * Copyright (C) 2009 Alex Milowski (alex@milowski.com). All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MATHML)

#include "RenderMathMLUnderOver.h"

#include "MathMLNames.h"

namespace WebCore {

using namespace MathMLNames;
    
RenderMathMLUnderOver::RenderMathMLUnderOver(Element* element)
    : RenderMathMLBlock(element)
{
    // Determine what kind of under/over expression we have by element name
    if (element->hasLocalName(MathMLNames::munderTag))
        m_kind = Under;
    else if (element->hasLocalName(MathMLNames::moverTag))
        m_kind = Over;
    else {
        ASSERT(element->hasLocalName(MathMLNames::munderoverTag));
        m_kind = UnderOver;
    }
}

RenderBoxModelObject* RenderMathMLUnderOver::base() const
{
    RenderObject* baseWrapper = firstChild();
    if ((m_kind == Over || m_kind == UnderOver) && baseWrapper)
        baseWrapper = baseWrapper->nextSibling();
    if (!baseWrapper)
        return 0;
    RenderObject* base = baseWrapper->firstChild();
    if (!base || !base->isBoxModelObject())
        return 0;
    return toRenderBoxModelObject(base);
}

void RenderMathMLUnderOver::addChild(RenderObject* child, RenderObject* beforeChild)
{    
    RenderMathMLBlock* row = createAnonymousMathMLBlock();
    
    // look through the children for rendered elements counting the blocks so we know what child
    // we are adding
    int blocks = 0;
    RenderObject* current = this->firstChild();
    while (current) {
        blocks++;
        current = current->nextSibling();
    }
    
    switch (blocks) {
    case 0:
        // this is the base so just append it
        RenderBlock::addChild(row, beforeChild);
        break;
    case 1:
        // the under or over
        row->style()->setTextAlign(CENTER);
        if (m_kind == Over) {
            // add the over as first
            RenderBlock::addChild(row, firstChild());
        } else {
            // add the under as last
            RenderBlock::addChild(row, beforeChild);
        }
        break;
    case 2:
        // the under or over
        row->style()->setTextAlign(CENTER);
        if (m_kind == UnderOver) {
            // add the over as first
            RenderBlock::addChild(row, firstChild());
        } else {
            // we really shouldn't get here as only munderover should have three children
            RenderBlock::addChild(row, beforeChild);
        }
        break;
    default:
        // munderover shouldn't have more than three children. In theory we shouldn't 
        // get here if the MathML is correctly formed, but that isn't a guarantee.
        // We will treat this as another under element and they'll get something funky.
        RenderBlock::addChild(row, beforeChild);
    }
    row->addChild(child);    
}

void RenderMathMLUnderOver::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderMathMLBlock::styleDidChange(diff, oldStyle);
    
    RenderObject* base = this->base();
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        ASSERT(child->isAnonymous() && child->style()->refCount() == 1);
        if (child->firstChild() != base)
            child->style()->setTextAlign(CENTER);
    }
}

RenderMathMLOperator* RenderMathMLUnderOver::unembellishedOperator()
{
    RenderBoxModelObject* base = this->base();
    if (!base || !base->isRenderMathMLBlock())
        return 0;
    return toRenderMathMLBlock(base)->unembellishedOperator();
}

inline int getOffsetHeight(RenderObject* obj) 
{
    if (obj->isBoxModelObject()) {
        RenderBoxModelObject* box = toRenderBoxModelObject(obj);
        return box->pixelSnappedOffsetHeight();
    }
   
    return 0;
}

LayoutUnit RenderMathMLUnderOver::baselinePosition(FontBaseline baselineType, bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    RenderObject* current = firstChild();
    if (!current || linePositionMode == PositionOfInteriorLineBoxes)
        return RenderMathMLBlock::baselinePosition(baselineType, firstLine, direction, linePositionMode);

    LayoutUnit baseline = direction == HorizontalLine ? marginTop() : marginRight();
    switch (m_kind) {
    case UnderOver:
    case Over:
        if (current->nextSibling()) {
            baseline += getOffsetHeight(current);
            current = current->nextSibling();
        }
        // fall through
    case Under:
        ASSERT(current->isRenderBlock());
        baseline += toRenderBox(current)->firstLineBoxBaseline();
    }

    return baseline;
}

}

#endif // ENABLE(MATHML)
