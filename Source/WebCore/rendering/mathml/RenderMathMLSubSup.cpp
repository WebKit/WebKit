/*
 * Copyright (C) 2010 Alex Milowski (alex@milowski.com). All rights reserved.
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

#include "RenderMathMLSubSup.h"

#include "MathMLNames.h"

namespace WebCore {
    
using namespace MathMLNames;

static const int gSubsupScriptMargin = 1;

RenderMathMLSubSup::RenderMathMLSubSup(Element* element) 
    : RenderMathMLBlock(element)
    , m_scripts(0)
{
    // Determine what kind of sub/sup expression we have by element name
    if (element->hasLocalName(MathMLNames::msubTag))
        m_kind = Sub;
    else if (element->hasLocalName(MathMLNames::msupTag))
        m_kind = Sup;
    else {
        ASSERT(element->hasLocalName(MathMLNames::msubsupTag));
        m_kind = SubSup;
    }
}

RenderBoxModelObject* RenderMathMLSubSup::base() const
{
    RenderObject* baseWrapper = firstChild();
    if (!baseWrapper)
        return 0;
    RenderObject* base = baseWrapper->firstChild();
    if (!base || !base->isBoxModelObject())
        return 0;
    return toRenderBoxModelObject(base);
}

void RenderMathMLSubSup::addChild(RenderObject* child, RenderObject* beforeChild)
{
    // Note: The RenderMathMLBlock only allows element children to be added.
    Element* childElement = toElement(child->node());

    if (childElement && !childElement->previousElementSibling()) {
        // Position 1 is always the base of the msub/msup/msubsup.
        RenderBlock* baseWrapper = createAlmostAnonymousBlock(INLINE_BLOCK);
        RenderMathMLBlock::addChild(baseWrapper, firstChild());
        baseWrapper->addChild(child);
            
        // Make sure we have a script block for rendering.
        if (m_kind == SubSup && !m_scripts) {
            RefPtr<RenderStyle> scriptsStyle = RenderStyle::createAnonymousStyleWithDisplay(style(), INLINE_BLOCK);
            scriptsStyle->setVerticalAlign(TOP);
            scriptsStyle->setMarginLeft(Length(gSubsupScriptMargin, Fixed));
            scriptsStyle->setTextAlign(LEFT);
            // Set this wrapper's font-size for its line-height & baseline position.
            scriptsStyle->setBlendedFontSize(static_cast<int>(0.75 * style()->fontSize()));
            m_scripts = new (renderArena()) RenderMathMLBlock(node());
            m_scripts->setStyle(scriptsStyle);
            RenderMathMLBlock::addChild(m_scripts, beforeChild);
        }
    } else {
        if (m_kind == SubSup) {
            ASSERT(childElement);
            if (!childElement)
                return;

            RenderBlock* script = m_scripts->createAlmostAnonymousBlock();

            // The order is always backwards so the first script is the subscript and the superscript 
            // is last. That means the superscript is the first to render vertically.
            Element* previousSibling = childElement->previousElementSibling();
            if (previousSibling && !previousSibling->previousElementSibling()) 
                m_scripts->addChild(script);
            else                 
                m_scripts->addChild(script, m_scripts->firstChild());
            
            script->addChild(child);
        } else
            RenderMathMLBlock::addChild(child, beforeChild);
    }
}

RenderMathMLOperator* RenderMathMLSubSup::unembellishedOperator()
{
    RenderBoxModelObject* base = this->base();
    if (!base || !base->isRenderMathMLBlock())
        return 0;
    return toRenderMathMLBlock(base)->unembellishedOperator();
}

void RenderMathMLSubSup::layout()
{
    RenderBlock::layout();
    
    if (m_kind != SubSup || !m_scripts)
        return;
    RenderBoxModelObject* base = this->base();
    RenderObject* superscriptWrapper = m_scripts->firstChild();
    RenderObject* subscriptWrapper = m_scripts->lastChild();
    if (!base || !superscriptWrapper || !subscriptWrapper || superscriptWrapper == subscriptWrapper)
        return;
    ASSERT(superscriptWrapper->isRenderMathMLBlock());
    ASSERT(subscriptWrapper->isRenderMathMLBlock());
    RenderObject* superscript = superscriptWrapper->firstChild();
    RenderObject* subscript = subscriptWrapper->firstChild();
    if (!superscript || !subscript)
        return;
    
    LineDirectionMode lineDirection = style()->isHorizontalWritingMode() ? HorizontalLine : VerticalLine;
    LayoutUnit baseBaseline = base->baselinePosition(AlphabeticBaseline, true, lineDirection);
    LayoutUnit baseExtendUnderBaseline = getBoxModelObjectHeight(base) - baseBaseline;
    LayoutUnit axis = style()->fontMetrics().xHeight() / 2;
    LayoutUnit superscriptHeight = getBoxModelObjectHeight(superscript);
    LayoutUnit subscriptHeight = getBoxModelObjectHeight(subscript);
    
    // Our layout rules are: Don't let the superscript go below the "axis" (half x-height above the
    // baseline), or the subscript above the axis. Also, don't let the superscript's top edge be
    // below the base's top edge, or the subscript's bottom edge above the base's bottom edge.
    //
    // FIXME: Check any subscriptshift or superscriptshift attributes, and maybe use more sophisticated
    // heuristics from TeX or elsewhere. See https://bugs.webkit.org/show_bug.cgi?id=79274#c5.
    
    // Above we did scriptsStyle->setVerticalAlign(TOP) for mscripts' style, so the superscript's
    // top edge will equal the top edge of the base's padding.
    LayoutUnit basePaddingTop = superscriptHeight + axis - baseBaseline;
    // If basePaddingTop is positive, it's indeed the base's padding-top that we need. If it's negative,
    // then we should instead use its absolute value to pad the bottom of the superscript, to get the
    // superscript's bottom edge down to the axis. First we compute how much more we need to shift the
    // subscript down, once its top edge is at the axis.
    LayoutUnit superPaddingBottom = max<LayoutUnit>(baseExtendUnderBaseline + axis - subscriptHeight, 0);
    if (basePaddingTop < 0) {
        superPaddingBottom += -basePaddingTop;
        basePaddingTop = 0;
    }
    
    setChildNeedsLayout(true, MarkOnlyThis);
    
    RenderObject* baseWrapper = firstChild();
    baseWrapper->style()->setPaddingTop(Length(basePaddingTop, Fixed));
    baseWrapper->setNeedsLayout(true, MarkOnlyThis);
    
    superscriptWrapper->style()->setPaddingBottom(Length(superPaddingBottom, Fixed));
    superscriptWrapper->setNeedsLayout(true, MarkOnlyThis);
    m_scripts->setNeedsLayout(true, MarkOnlyThis);
    
    RenderBlock::layout();
}

}    

#endif // ENABLE(MATHML)
