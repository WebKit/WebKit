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

#include "MathMLTextElement.h"

#include "HTMLElement.h"
#include "HTMLNames.h"
#include "MathMLNames.h"
#include "RenderMathMLOperator.h"
#include "RenderMathMLSpace.h"
#include "RenderMathMLToken.h"
#include "SVGElement.h"
#include "SVGNames.h"

namespace WebCore {
    
using namespace MathMLNames;

inline MathMLTextElement::MathMLTextElement(const QualifiedName& tagName, Document& document)
    : MathMLElement(tagName, document)
{
    setHasCustomStyleResolveCallbacks();
}

PassRefPtr<MathMLTextElement> MathMLTextElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new MathMLTextElement(tagName, document));
}

void MathMLTextElement::didAttachRenderers()
{
    MathMLElement::didAttachRenderers();
    if (renderer() && renderer()->isRenderMathMLToken())
        toRenderMathMLToken(renderer())->updateTokenContent();
}

void MathMLTextElement::childrenChanged(const ChildChange& change)
{
    MathMLElement::childrenChanged(change);
    if (renderer() && renderer()->isRenderMathMLToken())
        toRenderMathMLToken(renderer())->updateTokenContent();
}

RenderPtr<RenderElement> MathMLTextElement::createElementRenderer(PassRef<RenderStyle> style)
{
    if (hasTagName(MathMLNames::moTag))
        return createRenderer<RenderMathMLOperator>(*this, std::move(style));
    if (hasTagName(MathMLNames::mspaceTag))
        return createRenderer<RenderMathMLSpace>(*this, std::move(style));

    ASSERT(hasTagName(MathMLNames::miTag) || hasTagName(MathMLNames::mnTag) || hasTagName(MathMLNames::msTag) || hasTagName(MathMLNames::mtextTag));

    return createRenderer<RenderMathMLToken>(*this, std::move(style));
}

static bool isPhrasingContent(const Node& node)
{
    // Phrasing content is described in the HTML 5 specification:
    // http://www.w3.org/TR/html5/dom.html#phrasing-content.

    if (!node.isElementNode())
        return node.isTextNode();

    auto& element = toElement(node);

    if (element.isMathMLElement()) {
        auto& mathmlElement = toMathMLElement(element);
        return mathmlElement.hasTagName(MathMLNames::mathTag);
    }

    if (element.isSVGElement()) {
        auto& svgElement = toSVGElement(element);
        return svgElement.hasTagName(SVGNames::svgTag);
    }

    if (element.isHTMLElement()) {
        // FIXME: add the <data> and <time> tags when they are implemented.
        auto& htmlElement = toHTMLElement(element);
        return htmlElement.hasTagName(HTMLNames::aTag)
            || htmlElement.hasTagName(HTMLNames::abbrTag)
            || htmlElement.hasTagName(HTMLNames::areaTag)
            || htmlElement.hasTagName(HTMLNames::audioTag)
            || htmlElement.hasTagName(HTMLNames::bTag)
            || htmlElement.hasTagName(HTMLNames::bdiTag)
            || htmlElement.hasTagName(HTMLNames::bdoTag)
            || htmlElement.hasTagName(HTMLNames::brTag)
            || htmlElement.hasTagName(HTMLNames::buttonTag)
            || htmlElement.hasTagName(HTMLNames::canvasTag)
            || htmlElement.hasTagName(HTMLNames::citeTag)
            || htmlElement.hasTagName(HTMLNames::codeTag)
            || htmlElement.hasTagName(HTMLNames::datalistTag)
            || htmlElement.hasTagName(HTMLNames::delTag)
            || htmlElement.hasTagName(HTMLNames::dfnTag)
            || htmlElement.hasTagName(HTMLNames::emTag)
            || htmlElement.hasTagName(HTMLNames::embedTag)
            || htmlElement.hasTagName(HTMLNames::iTag)
            || htmlElement.hasTagName(HTMLNames::iframeTag)
            || htmlElement.hasTagName(HTMLNames::imgTag)
            || htmlElement.hasTagName(HTMLNames::inputTag)
            || htmlElement.hasTagName(HTMLNames::insTag)
            || htmlElement.hasTagName(HTMLNames::kbdTag)
            || htmlElement.hasTagName(HTMLNames::keygenTag)
            || htmlElement.hasTagName(HTMLNames::labelTag)
            || htmlElement.hasTagName(HTMLNames::mapTag)
            || htmlElement.hasTagName(HTMLNames::markTag)
            || htmlElement.hasTagName(HTMLNames::meterTag)
            || htmlElement.hasTagName(HTMLNames::noscriptTag)
            || htmlElement.hasTagName(HTMLNames::objectTag)
            || htmlElement.hasTagName(HTMLNames::outputTag)
            || htmlElement.hasTagName(HTMLNames::progressTag)
            || htmlElement.hasTagName(HTMLNames::qTag)
            || htmlElement.hasTagName(HTMLNames::rubyTag)
            || htmlElement.hasTagName(HTMLNames::sTag)
            || htmlElement.hasTagName(HTMLNames::sampTag)
            || htmlElement.hasTagName(HTMLNames::scriptTag)
            || htmlElement.hasTagName(HTMLNames::selectTag)
            || htmlElement.hasTagName(HTMLNames::smallTag)
            || htmlElement.hasTagName(HTMLNames::spanTag)
            || htmlElement.hasTagName(HTMLNames::strongTag)
            || htmlElement.hasTagName(HTMLNames::subTag)
            || htmlElement.hasTagName(HTMLNames::supTag)
            || htmlElement.hasTagName(HTMLNames::templateTag)
            || htmlElement.hasTagName(HTMLNames::textareaTag)
            || htmlElement.hasTagName(HTMLNames::uTag)
            || htmlElement.hasTagName(HTMLNames::varTag)
            || htmlElement.hasTagName(HTMLNames::videoTag)
            || htmlElement.hasTagName(HTMLNames::wbrTag);
    }

    return false;
}

bool MathMLTextElement::childShouldCreateRenderer(const Node& child) const
{
    // The HTML specification defines <mi>, <mo>, <mn>, <ms> and <mtext> as insertion points.

    // FIXME: phrasing content should be accepted in <mo> elements too (https://bugs.webkit.org/show_bug.cgi?id=130245).
    if (hasTagName(moTag))
        return child.isTextNode();

    return !hasTagName(MathMLNames::mspaceTag) && isPhrasingContent(child);
}

}

#endif // ENABLE(MATHML)
