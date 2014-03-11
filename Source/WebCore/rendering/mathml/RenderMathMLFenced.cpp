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

#include "RenderMathMLFenced.h"

#include "FontSelector.h"
#include "MathMLNames.h"
#include "RenderInline.h"
#include "RenderMathMLOperator.h"
#include "RenderText.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
    
using namespace MathMLNames;
    
static const char* gOpeningBraceChar = "(";
static const char* gClosingBraceChar = ")";

RenderMathMLFenced::RenderMathMLFenced(MathMLInlineContainerElement& element, PassRef<RenderStyle> style)
    : RenderMathMLRow(element, std::move(style))
    , m_closeFenceRenderer(nullptr)
{
}

void RenderMathMLFenced::updateFromElement()
{
    const auto& fenced = element();
 
    // The open operator defaults to a left parenthesis.
    AtomicString open = fenced.fastGetAttribute(MathMLNames::openAttr);
    m_open = open.isNull() ? gOpeningBraceChar : open;

    // The close operator defaults to a right parenthesis.
    AtomicString close = fenced.fastGetAttribute(MathMLNames::closeAttr);
    m_close = close.isNull() ? gClosingBraceChar : close;

    AtomicString separators = fenced.fastGetAttribute(MathMLNames::separatorsAttr);
    if (!separators.isNull()) {
        StringBuilder characters;
        for (unsigned int i = 0; i < separators.length(); i++) {
            if (!isSpaceOrNewline(separators[i]))
                characters.append(separators[i]);
        }
        m_separators = !characters.length() ? 0 : characters.toString().impl();
    } else {
        // The separator defaults to a single comma.
        m_separators = StringImpl::create(",");
    }
    
    if (isEmpty())
        makeFences();
    else {
        // FIXME: The mfenced element fails to update dynamically when its open, close and separators attributes are changed (https://bugs.webkit.org/show_bug.cgi?id=57696).
        toRenderMathMLOperator(firstChild())->updateTokenContent(m_open);
        m_closeFenceRenderer->updateTokenContent(m_close);
    }
}

RenderPtr<RenderMathMLOperator> RenderMathMLFenced::createMathMLOperator(const String& operatorString, MathMLOperatorDictionary::Form form, MathMLOperatorDictionary::Flag flag)
{
    RenderPtr<RenderMathMLOperator> newOperator = createRenderer<RenderMathMLOperator>(document(), RenderStyle::createAnonymousStyleWithDisplay(&style(), FLEX), operatorString, form, flag);
    newOperator->initializeStyle();
    return newOperator;
}

void RenderMathMLFenced::makeFences()
{
    RenderPtr<RenderMathMLOperator> openFence = createMathMLOperator(m_open, MathMLOperatorDictionary::Prefix, MathMLOperatorDictionary::Fence);
    RenderMathMLRow::addChild(openFence.leakPtr(), firstChild());

    RenderPtr<RenderMathMLOperator> closeFence = createMathMLOperator(m_close, MathMLOperatorDictionary::Postfix, MathMLOperatorDictionary::Fence);
    m_closeFenceRenderer = closeFence.get();
    RenderMathMLRow::addChild(closeFence.leakPtr());
}

void RenderMathMLFenced::addChild(RenderObject* child, RenderObject* beforeChild)
{
    // make the fences if the render object is empty
    if (isEmpty())
        updateFromElement();
    
    // FIXME: Adding or removing a child should possibly cause all later separators to shift places if they're different, as later child positions change by +1 or -1. This should also handle surrogate pairs. See https://bugs.webkit.org/show_bug.cgi?id=125938.
    
    RenderPtr<RenderMathMLOperator> separatorRenderer;
    if (m_separators.get()) {
        unsigned int count = 0;
        for (Node* position = child->node(); position; position = position->previousSibling()) {
            if (position->isElementNode())
                count++;
        }
        if (!beforeChild) {
            // We're adding at the end (before the closing fence), so a new separator would go before the new child, not after it.
            --count;
        }
        // |count| is now the number of element children that will be before our new separator, i.e. it's the 1-based index of the separator.
        
        if (count > 0) {
            UChar separator;
            
            // Use the last separator if we've run out of specified separators.
            if (count > m_separators.get()->length())
                separator = (*m_separators.get())[m_separators.get()->length() - 1];
            else
                separator = (*m_separators.get())[count - 1];
                
            StringBuilder builder;
            builder.append(separator);
            separatorRenderer = createMathMLOperator(builder.toString(), MathMLOperatorDictionary::Infix, MathMLOperatorDictionary::Separator);
        }
    }
    
    if (beforeChild) {
        // Adding |x| before an existing |y| e.g. in element (y) - first insert our new child |x|, then its separator, to get (x, y).
        RenderMathMLRow::addChild(child, beforeChild);
        if (separatorRenderer)
            RenderMathMLRow::addChild(separatorRenderer.leakPtr(), beforeChild);
    } else {
        // Adding |y| at the end of an existing element e.g. (x) - insert the separator first before the closing fence, then |y|, to get (x, y).
        if (separatorRenderer)
            RenderMathMLRow::addChild(separatorRenderer.leakPtr(), m_closeFenceRenderer);
        RenderMathMLRow::addChild(child, m_closeFenceRenderer);
    }
}

}    

#endif
