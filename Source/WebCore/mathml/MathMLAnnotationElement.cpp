/*
 * Copyright (C) 2016 Igalia S.L. All rights reserved.
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
 *
 */

#include "config.h"
#include "MathMLAnnotationElement.h"

#if ENABLE(MATHML)

#include "ElementInlines.h"
#include "HTMLHtmlElement.h"
#include "MathMLMathElement.h"
#include "MathMLNames.h"
#include "MathMLSelectElement.h"
#include "RenderMathMLBlock.h"
#include "SVGElementTypeHelpers.h"
#include "SVGSVGElement.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MathMLAnnotationElement);

using namespace MathMLNames;

MathMLAnnotationElement::MathMLAnnotationElement(const QualifiedName& tagName, Document& document)
    : MathMLPresentationElement(tagName, document)
{
    ASSERT(hasTagName(annotationTag) || hasTagName(annotation_xmlTag));
}

Ref<MathMLAnnotationElement> MathMLAnnotationElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new MathMLAnnotationElement(tagName, document));
}

RenderPtr<RenderElement> MathMLAnnotationElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition& insertionPosition)
{
    if (hasTagName(MathMLNames::annotationTag))
        return MathMLElement::createElementRenderer(WTFMove(style), insertionPosition);

    ASSERT(hasTagName(annotation_xmlTag));
    return createRenderer<RenderMathMLBlock>(RenderObject::Type::MathMLBlock, *this, WTFMove(style));
}

bool MathMLAnnotationElement::childShouldCreateRenderer(const Node& child) const
{
    // For <annotation>, only text children are allowed.
    if (hasTagName(MathMLNames::annotationTag))
        return child.isTextNode();

    ASSERT(hasTagName(annotation_xmlTag));
    return StyledElement::childShouldCreateRenderer(child);
}

void MathMLAnnotationElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason reason)
{
    if (name == MathMLNames::srcAttr || name == MathMLNames::encodingAttr) {
        auto* parent = parentElement();
        if (is<MathMLElement>(parent) && parent->hasTagName(semanticsTag))
            downcast<MathMLElement>(*parent).updateSelectedChild();
    }
    MathMLPresentationElement::attributeChanged(name, oldValue, newValue, reason);
}

}

#endif // ENABLE(MATHML)
