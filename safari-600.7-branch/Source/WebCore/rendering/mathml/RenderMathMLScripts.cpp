/*
 * Copyright (C) 2010 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2013 The MathJax Consortium.
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

#include "RenderMathMLScripts.h"

#include "MathMLElement.h"

namespace WebCore {
    
using namespace MathMLNames;

// RenderMathMLScripts implements various MathML elements drawing scripts attached to a base. For valid MathML elements, the structure of the render tree should be:
//
// - msub, msup, msubsup: BaseWrapper SubSupPairWrapper
// - mmultiscripts: BaseWrapper SubSupPairWrapper* (mprescripts SubSupPairWrapper*)*
//
// where BaseWrapper and SubSupPairWrapper do not contain any <mprescripts/> children. In addition, BaseWrapper must have one child and SubSupPairWrapper must have either one child (msub, msup) or two children (msubsup, mmultiscripts).
//
// In order to accept invalid markup and to handle the script elements consistently and uniformly, we will use a more general structure that encompasses both valid and invalid elements:
//
// BaseWrapper SubSupPairWrapper* (mprescripts SubSupPairWrapper*)*
//
// where BaseWrapper can now be empty and SubSupPairWrapper can now have one or two elements.
//

static bool isPrescript(RenderObject* renderObject)
{
    ASSERT(renderObject);
    return renderObject->node() && renderObject->node()->hasTagName(MathMLNames::mprescriptsTag);
}

RenderMathMLScripts::RenderMathMLScripts(Element& element, PassRef<RenderStyle> style)
    : RenderMathMLBlock(element, WTF::move(style))
    , m_baseWrapper(0)
{
    // Determine what kind of sub/sup expression we have by element name
    if (element.hasTagName(MathMLNames::msubTag))
        m_kind = Sub;
    else if (element.hasTagName(MathMLNames::msupTag))
        m_kind = Super;
    else if (element.hasTagName(MathMLNames::msubsupTag))
        m_kind = SubSup;
    else {
        ASSERT(element.hasTagName(MathMLNames::mmultiscriptsTag));
        m_kind = Multiscripts;
    }
}

RenderBoxModelObject* RenderMathMLScripts::base() const
{
    if (!m_baseWrapper)
        return 0;
    RenderObject* base = m_baseWrapper->firstChild();
    if (!base || !base->isBoxModelObject())
        return 0;
    return toRenderBoxModelObject(base);
}

void RenderMathMLScripts::fixAnonymousStyleForSubSupPair(RenderObject* subSupPair, bool isPostScript)
{
    ASSERT(subSupPair && subSupPair->style().refCount() == 1);
    RenderStyle& scriptsStyle = subSupPair->style();

    // subSup pairs are drawn in column from bottom (subscript) to top (superscript).
    scriptsStyle.setFlexDirection(FlowColumnReverse);

    // The MathML specification does not specify horizontal alignment of
    // scripts. We align the bottom (respectively top) edge of the subscript
    // (respectively superscript) with the bottom (respectively top) edge of
    // the flex container. Note that for valid <msub> and <msup> elements, the
    // subSupPair should actually have only one script.
    scriptsStyle.setJustifyContent(m_kind == Sub ? JustifyFlexStart : m_kind == Super ? JustifyFlexEnd : JustifySpaceBetween);

    // The MathML specification does not specify vertical alignment of scripts.
    // Let's right align prescripts and left align postscripts.
    // See http://lists.w3.org/Archives/Public/www-math/2012Aug/0006.html
    scriptsStyle.setAlignItems(isPostScript ? AlignFlexStart : AlignFlexEnd);

    // We set the order property so that the prescripts are drawn before the base.
    scriptsStyle.setOrder(isPostScript ? 0 : -1);

    // We set this wrapper's font-size for its line-height.
    LayoutUnit scriptSize = static_cast<int>(0.75 * style().fontSize());
    scriptsStyle.setFontSize(scriptSize);
}

void RenderMathMLScripts::fixAnonymousStyles()
{
    // We set the base wrapper's style so that baseHeight in layout() will be an unstretched height.
    ASSERT(m_baseWrapper && m_baseWrapper->style().hasOneRef());
    m_baseWrapper->style().setAlignSelf(AlignFlexStart);

    // This sets the style for postscript pairs.
    RenderObject* subSupPair = m_baseWrapper;
    for (subSupPair = subSupPair->nextSibling(); subSupPair && !isPrescript(subSupPair); subSupPair = subSupPair->nextSibling())
        fixAnonymousStyleForSubSupPair(subSupPair, true);

    if (subSupPair && m_kind == Multiscripts) {
        // This sets the style for prescript pairs.
        for (subSupPair = subSupPair->nextSibling(); subSupPair && !isPrescript(subSupPair); subSupPair = subSupPair->nextSibling())
            fixAnonymousStyleForSubSupPair(subSupPair, false);
    }

    // This resets style for extra subSup pairs.
    for (; subSupPair; subSupPair = subSupPair->nextSibling()) {
        if (!isPrescript(subSupPair)) {
            ASSERT(subSupPair && subSupPair->style().refCount() == 1);
            RenderStyle& scriptsStyle = subSupPair->style();
            scriptsStyle.setFlexDirection(FlowRow);
            scriptsStyle.setJustifyContent(JustifyFlexStart);
            scriptsStyle.setAlignItems(AlignCenter);
            scriptsStyle.setOrder(0);
            scriptsStyle.setFontSize(style().fontSize());
        }
    }
}

void RenderMathMLScripts::addChildInternal(bool doNotRestructure, RenderObject* child, RenderObject* beforeChild)
{
    if (doNotRestructure) {
        RenderMathMLBlock::addChild(child, beforeChild);
        return;
    }

    if (beforeChild) {
        // beforeChild may be a grandchild, so we call the addChild function of the corresponding wrapper instead.
        RenderObject* parent = beforeChild->parent();
        if (parent != this) {
            RenderMathMLBlock* parentBlock = toRenderMathMLBlock(parent);
            if (parentBlock->isRenderMathMLScriptsWrapper()) {
                RenderMathMLScriptsWrapper* wrapper = toRenderMathMLScriptsWrapper(parentBlock);
                wrapper->addChildInternal(false, child, beforeChild);
                return;
            }
        }
    }

    if (beforeChild == m_baseWrapper) {
        // This is like inserting the child at the beginning of the base wrapper.
        m_baseWrapper->addChildInternal(false, child, m_baseWrapper->firstChild());
        return;
    }
    
    if (isPrescript(child)) {
        // The new child becomes an <mprescripts/> separator.
        RenderMathMLBlock::addChild(child, beforeChild);
        return;
    }

    if (!beforeChild || isPrescript(beforeChild)) {
        // We are at the end of a sequence of subSup pairs.
        RenderMathMLBlock* previousSibling = toRenderMathMLBlock(beforeChild ? beforeChild->previousSibling() : lastChild());
        if (previousSibling && previousSibling->isRenderMathMLScriptsWrapper()) {
            RenderMathMLScriptsWrapper* wrapper = toRenderMathMLScriptsWrapper(previousSibling);
            if ((wrapper->m_kind == RenderMathMLScriptsWrapper::Base && wrapper->isEmpty()) || (wrapper->m_kind == RenderMathMLScriptsWrapper::SubSupPair && !wrapper->firstChild()->nextSibling())) {
                // The previous sibling is either an empty base or a SubSup pair with a single child so we can insert the new child into that wrapper.
                wrapper->addChildInternal(true, child);
                return;
            }
        }
        // Otherwise we create a new subSupPair to store the new child.
        RenderMathMLScriptsWrapper* subSupPair = RenderMathMLScriptsWrapper::createAnonymousWrapper(this, RenderMathMLScriptsWrapper::SubSupPair);
        subSupPair->addChildInternal(true, child);
        RenderMathMLBlock::addChild(subSupPair, beforeChild);
        return;
    }

    // beforeChild is a subSup pair. This is like inserting the new child at the beginning of the subSup wrapper.
    RenderMathMLScriptsWrapper* wrapper = toRenderMathMLScriptsWrapper(beforeChild);
    ASSERT(wrapper->m_kind == RenderMathMLScriptsWrapper::SubSupPair);
    ASSERT(!(m_baseWrapper->isEmpty() && m_baseWrapper->nextSibling() == beforeChild));
    wrapper->addChildInternal(false, child, wrapper->firstChild());
}

RenderObject* RenderMathMLScripts::removeChildInternal(bool doNotRestructure, RenderObject& child)
{
    if (doNotRestructure)
        return RenderMathMLBlock::removeChild(child);

    ASSERT(isPrescript(&child));

    RenderObject* previousSibling = child.previousSibling();
    RenderObject* nextSibling = child.nextSibling();
    ASSERT(previousSibling);

    if (nextSibling && !isPrescript(previousSibling) && !isPrescript(nextSibling)) {
        RenderMathMLScriptsWrapper* previousWrapper = toRenderMathMLScriptsWrapper(previousSibling);
        RenderMathMLScriptsWrapper* nextWrapper = toRenderMathMLScriptsWrapper(nextSibling);
        ASSERT(nextWrapper->m_kind == RenderMathMLScriptsWrapper::SubSupPair && !nextWrapper->isEmpty());
        if ((previousWrapper->m_kind == RenderMathMLScriptsWrapper::Base && previousWrapper->isEmpty()) || (previousWrapper->m_kind == RenderMathMLScriptsWrapper::SubSupPair && !previousWrapper->firstChild()->nextSibling())) {
            RenderObject* script = nextWrapper->firstChild();
            nextWrapper->removeChildInternal(false, *script);
            previousWrapper->addChildInternal(true, script);
        }
    }

    return RenderMathMLBlock::removeChild(child);
}

void RenderMathMLScripts::addChild(RenderObject* child, RenderObject* beforeChild)
{
    if (isEmpty()) {
        m_baseWrapper = RenderMathMLScriptsWrapper::createAnonymousWrapper(this, RenderMathMLScriptsWrapper::Base);
        RenderMathMLBlock::addChild(m_baseWrapper);
    }

    addChildInternal(false, child, beforeChild);

    fixAnonymousStyles();
}

RenderObject* RenderMathMLScripts::removeChild(RenderObject& child)
{
    if (beingDestroyed() || documentBeingDestroyed()) {
        // The renderer is being destroyed so we remove the child normally.
        return RenderMathMLBlock::removeChild(child);
    }

    RenderObject* next = removeChildInternal(false, child);
    
    fixAnonymousStyles();
    
    return next;
}

void RenderMathMLScripts::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderMathMLBlock::styleDidChange(diff, oldStyle);
    
    if (!isEmpty())
        fixAnonymousStyles();
}

RenderMathMLOperator* RenderMathMLScripts::unembellishedOperator()
{
    RenderBoxModelObject* base = this->base();
    if (!base || !base->isRenderMathMLBlock())
        return 0;
    return toRenderMathMLBlock(base)->unembellishedOperator();
}

void RenderMathMLScripts::layout()
{
    RenderMathMLBlock::layout();

    if (!m_baseWrapper)
        return;
    RenderBox* base = m_baseWrapper->firstChildBox();
    if (!base)
        return;

    // Our layout rules include: Don't let the superscript go below the "axis" (half x-height above the
    // baseline), or the subscript above the axis. Also, don't let the superscript's top edge be
    // below the base's top edge, or the subscript's bottom edge above the base's bottom edge.

    LayoutUnit baseHeight = base->logicalHeight();
    LayoutUnit baseBaseline = base->firstLineBaseline();
    if (baseBaseline == -1)
        baseBaseline = baseHeight;
    LayoutUnit axis = style().fontMetrics().xHeight() / 2;
    int fontSize = style().fontSize();

    ASSERT(m_baseWrapper->style().hasOneRef());
    bool needsSecondLayout = false;

    LayoutUnit topPadding = 0;
    LayoutUnit bottomPadding = 0;

    Element* scriptElement = element();
    LayoutUnit superscriptShiftValue = 0;
    LayoutUnit subscriptShiftValue = 0;
    if (m_kind == Sub || m_kind == SubSup || m_kind == Multiscripts)
        parseMathMLLength(scriptElement->fastGetAttribute(MathMLNames::subscriptshiftAttr), subscriptShiftValue, &style(), false);
    if (m_kind == Super || m_kind == SubSup || m_kind == Multiscripts)
        parseMathMLLength(scriptElement->fastGetAttribute(MathMLNames::superscriptshiftAttr), superscriptShiftValue, &style(), false);

    bool isPostScript = true;
    RenderMathMLBlock* subSupPair = toRenderMathMLBlock(m_baseWrapper->nextSibling());
    for (; subSupPair; subSupPair = toRenderMathMLBlock(subSupPair->nextSibling())) {

        // We skip the base and <mprescripts/> elements.
        if (isPrescript(subSupPair)) {
            if (!isPostScript)
                break;
            isPostScript = false;
            continue;
        }

        if (RenderBox* superscript = m_kind == Sub ? 0 : subSupPair->lastChildBox()) {
            LayoutUnit superscriptHeight = superscript->logicalHeight();
            LayoutUnit superscriptBaseline = superscript->firstLineBaseline();
            if (superscriptBaseline == -1)
                superscriptBaseline = superscriptHeight;
            LayoutUnit minBaseline = std::max<LayoutUnit>(fontSize / 3 + 1 + superscriptBaseline, superscriptHeight + axis + superscriptShiftValue);

            topPadding = std::max<LayoutUnit>(topPadding, minBaseline - baseBaseline);
        }

        if (RenderBox* subscript = m_kind == Super ? 0 : subSupPair->firstChildBox()) {
            LayoutUnit subscriptHeight = subscript->logicalHeight();
            LayoutUnit subscriptBaseline = subscript->firstLineBaseline();
            if (subscriptBaseline == -1)
                subscriptBaseline = subscriptHeight;
            LayoutUnit baseExtendUnderBaseline = baseHeight - baseBaseline;
            LayoutUnit subscriptUnderItsBaseline = subscriptHeight - subscriptBaseline;
            LayoutUnit minExtendUnderBaseline = std::max<LayoutUnit>(fontSize / 5 + 1 + subscriptUnderItsBaseline, subscriptHeight + subscriptShiftValue - axis);

            bottomPadding = std::max<LayoutUnit>(bottomPadding, minExtendUnderBaseline - baseExtendUnderBaseline);
        }
    }

    Length newPadding(topPadding, Fixed);
    if (newPadding != m_baseWrapper->style().paddingTop()) {
        m_baseWrapper->style().setPaddingTop(newPadding);
        needsSecondLayout = true;
    }

    newPadding = Length(bottomPadding, Fixed);
    if (newPadding != m_baseWrapper->style().paddingBottom()) {
        m_baseWrapper->style().setPaddingBottom(newPadding);
        needsSecondLayout = true;
    }

    if (!needsSecondLayout)
        return;

    setNeedsLayout(MarkOnlyThis);
    m_baseWrapper->setChildNeedsLayout(MarkOnlyThis);

    RenderMathMLBlock::layout();
}

int RenderMathMLScripts::firstLineBaseline() const
{
    if (m_baseWrapper) {
        LayoutUnit baseline = m_baseWrapper->firstLineBaseline();
        if (baseline != -1)
            return baseline;
    }
    return RenderMathMLBlock::firstLineBaseline();
}

RenderMathMLScriptsWrapper* RenderMathMLScriptsWrapper::createAnonymousWrapper(RenderMathMLScripts* renderObject, WrapperType type)
{
    RenderMathMLScriptsWrapper* newBlock = new RenderMathMLScriptsWrapper(renderObject->document(), RenderStyle::createAnonymousStyleWithDisplay(&renderObject->style(), FLEX), type);
    newBlock->initializeStyle();
    return newBlock;
}

void RenderMathMLScriptsWrapper::addChildInternal(bool doNotRestructure, RenderObject* child, RenderObject* beforeChild)
{
    if (doNotRestructure) {
        RenderMathMLBlock::addChild(child, beforeChild);
        return;
    }

    RenderMathMLScripts* parentNode = toRenderMathMLScripts(parent());

    if (m_kind == Base) {
        RenderObject* sibling = nextSibling();

        if (!isEmpty() && !beforeChild) {
            // This is like inserting the child after the base wrapper.
            parentNode->addChildInternal(false, sibling);
            return;
        }

        // The old base (if any) becomes a script ; the new child becomes either the base or an <mprescripts> separator.
        RenderObject* oldBase = firstChild();
        if (oldBase)
            RenderMathMLBlock::removeChild(*oldBase);
        if (isPrescript(child))
            parentNode->addChildInternal(true, child, sibling);
        else
            RenderMathMLBlock::addChild(child);
        if (oldBase)
            parentNode->addChildInternal(false, oldBase, sibling);
        return;
    }

    if (isPrescript(child)) {
        // We insert an <mprescripts> element.
        if (!beforeChild)
            parentNode->addChildInternal(true, child, nextSibling());
        else if (beforeChild == firstChild())
            parentNode->addChildInternal(true, child, this);
        else {
            // We insert the <mprescripts> in the middle of a subSup pair so we must split that pair.
            RenderObject* sibling = nextSibling();
            parentNode->removeChildInternal(true, *this);
            parentNode->addChildInternal(true, child, sibling);

            RenderObject* script = firstChild();
            RenderMathMLBlock::removeChild(*script);
            parentNode->addChildInternal(false, script, child);

            script = beforeChild;
            RenderMathMLBlock::removeChild(*script);
            parentNode->addChildInternal(false, script, sibling);
            destroy();
        }
        return;
    }

    // We first move to the last subSup pair in the curent sequence of scripts.
    RenderMathMLScriptsWrapper* subSupPair = this;
    while (subSupPair->nextSibling() && !isPrescript(subSupPair->nextSibling()))
        subSupPair = toRenderMathMLScriptsWrapper(subSupPair->nextSibling());
    if (subSupPair->firstChild()->nextSibling()) {
        // The last pair has two children so we need to create a new pair to leave room for the new child.
        RenderMathMLScriptsWrapper* newPair = createAnonymousWrapper(parentNode, RenderMathMLScriptsWrapper::SubSupPair);
        parentNode->addChildInternal(true, newPair, subSupPair->nextSibling());
        subSupPair = newPair;
    }

    // We shift the successors in the current sequence of scripts.
    for (RenderObject* previousSibling = subSupPair->previousSibling(); subSupPair != this; previousSibling = previousSibling->previousSibling()) {
        RenderMathMLScriptsWrapper* previousSubSupPair = toRenderMathMLScriptsWrapper(previousSibling);
        RenderObject* script = previousSubSupPair->lastChild();
        previousSubSupPair->removeChildInternal(true, *script);
        subSupPair->addChildInternal(true, script, subSupPair->firstChild());
        subSupPair = toRenderMathMLScriptsWrapper(previousSibling);
    }

    // This subSup pair now contain one element which is either beforeChild or the script that was before. Hence we can insert the new child before of after that element.
    RenderMathMLBlock::addChild(child, firstChild() == beforeChild ? beforeChild : 0);
}

void RenderMathMLScriptsWrapper::addChild(RenderObject* child, RenderObject* beforeChild)
{
    RenderMathMLScripts* parentNode = toRenderMathMLScripts(parent());

    addChildInternal(false, child, beforeChild);

    parentNode->fixAnonymousStyles();
}

RenderObject* RenderMathMLScriptsWrapper::removeChildInternal(bool doNotRestructure, RenderObject& child)
{
    if (doNotRestructure)
        return RenderMathMLBlock::removeChild(child);

    RenderMathMLScripts* parentNode = toRenderMathMLScripts(parent());

    if (m_kind == Base) {
        // We remove the child from the base wrapper.
        RenderObject* sibling = nextSibling();
        RenderMathMLBlock::removeChild(child);
        if (sibling && !isPrescript(sibling)) {
            // If there are postscripts, the first one becomes the base.
            RenderMathMLScriptsWrapper* wrapper = toRenderMathMLScriptsWrapper(sibling);
            RenderObject* script = wrapper->firstChild();
            wrapper->removeChildInternal(false, *script);
            RenderMathMLBlock::addChild(script);
        }
        return sibling;
    }

    // We remove the child and shift the successors in the current sequence of scripts.
    RenderObject* next = RenderMathMLBlock::removeChild(child);
    RenderMathMLScriptsWrapper* subSupPair = this;
    for (RenderObject* nextSibling = subSupPair->nextSibling(); nextSibling && !isPrescript(nextSibling); nextSibling = nextSibling->nextSibling()) {
        RenderMathMLScriptsWrapper* nextSubSupPair = toRenderMathMLScriptsWrapper(nextSibling);
        RenderObject* script = nextSubSupPair->firstChild();
        nextSubSupPair->removeChildInternal(true, *script);
        subSupPair->addChildInternal(true, script);
        subSupPair = toRenderMathMLScriptsWrapper(nextSibling);
    }

    // We remove the last subSup pair if it became empty.
    if (subSupPair->isEmpty()) {
        parentNode->removeChildInternal(true, *subSupPair);
        subSupPair->destroy();
    }
    
    return next;
}

RenderObject* RenderMathMLScriptsWrapper::removeChild(RenderObject& child)
{
    if (beingDestroyed() || documentBeingDestroyed()) {
        // The renderer is being destroyed so we remove the child normally.
        return RenderMathMLBlock::removeChild(child);
    }

    RenderMathMLScripts* parentNode = toRenderMathMLScripts(parent());
    RenderObject* next = removeChildInternal(false, child);
    parentNode->fixAnonymousStyles();
    return next;
}

}    

#endif // ENABLE(MATHML)
