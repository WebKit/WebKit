/*
 * Copyright (C) 2009 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "MathMLInlineContainerElement.h"

#include "MathMLNames.h"
#include "RenderMathMLBlock.h"
#include "RenderMathMLFenced.h"
#include "RenderMathMLFraction.h"
#include "RenderMathMLMenclose.h"
#include "RenderMathMLRoot.h"
#include "RenderMathMLRow.h"
#include "RenderMathMLScripts.h"
#include "RenderMathMLSquareRoot.h"
#include "RenderMathMLUnderOver.h"

namespace WebCore {
    
using namespace MathMLNames;

MathMLInlineContainerElement::MathMLInlineContainerElement(const QualifiedName& tagName, Document& document)
    : MathMLElement(tagName, document)
{
}

PassRefPtr<MathMLInlineContainerElement> MathMLInlineContainerElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new MathMLInlineContainerElement(tagName, document));
}

RenderPtr<RenderElement> MathMLInlineContainerElement::createElementRenderer(PassRef<RenderStyle> style)
{
    if (hasLocalName(annotation_xmlTag))
        return createRenderer<RenderMathMLRow>(*this, std::move(style));
    if (hasLocalName(merrorTag) || hasLocalName(mphantomTag) || hasLocalName(mrowTag) || hasLocalName(mstyleTag))
        return createRenderer<RenderMathMLRow>(*this, std::move(style));
    if (hasLocalName(msubTag))
        return createRenderer<RenderMathMLScripts>(*this, std::move(style));
    if (hasLocalName(msupTag))
        return createRenderer<RenderMathMLScripts>(*this, std::move(style));
    if (hasLocalName(msubsupTag))
        return createRenderer<RenderMathMLScripts>(*this, std::move(style));
    if (hasLocalName(mmultiscriptsTag))
        return createRenderer<RenderMathMLScripts>(*this, std::move(style));
    if (hasLocalName(moverTag))
        return createRenderer<RenderMathMLUnderOver>(*this, std::move(style));
    if (hasLocalName(munderTag))
        return createRenderer<RenderMathMLUnderOver>(*this, std::move(style));
    if (hasLocalName(munderoverTag))
        return createRenderer<RenderMathMLUnderOver>(*this, std::move(style));
    if (hasLocalName(mfracTag))
        return createRenderer<RenderMathMLFraction>(*this, std::move(style));
    if (hasLocalName(msqrtTag))
        return createRenderer<RenderMathMLSquareRoot>(*this, std::move(style));
    if (hasLocalName(mrootTag))
        return createRenderer<RenderMathMLRoot>(*this, std::move(style));
    if (hasLocalName(mfencedTag))
        return createRenderer<RenderMathMLFenced>(*this, std::move(style));
    if (hasLocalName(mtableTag))
        return createRenderer<RenderMathMLTable>(*this, std::move(style));

    return createRenderer<RenderMathMLBlock>(*this, std::move(style));
}

}

#endif // ENABLE(MATHML)
