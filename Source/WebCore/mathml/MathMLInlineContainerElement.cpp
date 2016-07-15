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
#include "RenderMathMLMenclose.h"
#include "RenderMathMLRoot.h"
#include "RenderMathMLRow.h"
#include "RenderMathMLScripts.h"
#include "RenderMathMLUnderOver.h"

namespace WebCore {

using namespace MathMLNames;

MathMLInlineContainerElement::MathMLInlineContainerElement(const QualifiedName& tagName, Document& document)
    : MathMLElement(tagName, document)
{
}

Ref<MathMLInlineContainerElement> MathMLInlineContainerElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new MathMLInlineContainerElement(tagName, document));
}

void MathMLInlineContainerElement::childrenChanged(const ChildChange& change)
{
    // FIXME: Parsing of operator properties should be done in the element classes rather than in the renderer classes.
    // See https://webkit.org/b/156537
    if (renderer() && is<RenderMathMLRow>(*renderer()))
        downcast<RenderMathMLRow>(*renderer()).updateOperatorProperties();
    MathMLElement::childrenChanged(change);
}

RenderPtr<RenderElement> MathMLInlineContainerElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    if (hasTagName(annotation_xmlTag) || hasTagName(merrorTag) || hasTagName(mphantomTag) || hasTagName(mrowTag) || hasTagName(mstyleTag))
        return createRenderer<RenderMathMLRow>(*this, WTFMove(style));
    if (hasTagName(msubTag) || hasTagName(msupTag) || hasTagName(msubsupTag) || hasTagName(mmultiscriptsTag))
        return createRenderer<RenderMathMLScripts>(*this, WTFMove(style));
    if (hasTagName(moverTag) || hasTagName(munderTag) || hasTagName(munderoverTag))
        return createRenderer<RenderMathMLUnderOver>(*this, WTFMove(style));
    if (hasTagName(msqrtTag) || hasTagName(mrootTag))
        return createRenderer<RenderMathMLRoot>(*this, WTFMove(style));
    if (hasTagName(mfencedTag))
        return createRenderer<RenderMathMLFenced>(*this, WTFMove(style));
    if (hasTagName(mtableTag))
        return createRenderer<RenderMathMLTable>(*this, WTFMove(style));

    return createRenderer<RenderMathMLBlock>(*this, WTFMove(style));
}

void MathMLInlineContainerElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    bool displayStyleAttribute = (name == displaystyleAttr && (hasTagName(mstyleTag) || hasTagName(mtableTag)));
    bool mathVariantAttribute = (name == mathvariantAttr && (hasTagName(mathTag) || hasTagName(mstyleTag)));
    if ((displayStyleAttribute || mathVariantAttribute) && renderer())
        MathMLStyle::resolveMathMLStyleTree(renderer());

    MathMLElement::parseAttribute(name, value);
}

}

#endif // ENABLE(MATHML)
