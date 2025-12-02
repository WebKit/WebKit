/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "MathMLSpaceElement.h"

#if ENABLE(MATHML)

#include "NodeName.h"
#include "RenderMathMLSpace.h"
#include "RenderObjectInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(MathMLSpaceElement);

using namespace MathMLNames;

MathMLSpaceElement::MathMLSpaceElement(const QualifiedName& tagName, Document& document)
    : MathMLPresentationElement(tagName, document)
{
}

Ref<MathMLSpaceElement> MathMLSpaceElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new MathMLSpaceElement(tagName, document));
}

const MathMLElement::Length& MathMLSpaceElement::width()
{
    return cachedMathMLLength(MathMLNames::widthAttr, m_width);
}

const MathMLElement::Length& MathMLSpaceElement::height()
{
    return cachedMathMLLength(MathMLNames::heightAttr, m_height);
}

const MathMLElement::Length& MathMLSpaceElement::depth()
{
    return cachedMathMLLength(MathMLNames::depthAttr, m_depth);
}

void MathMLSpaceElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    bool affectsLayout = false;
    switch (name.nodeName()) {
    case AttributeNames::widthAttr:
        m_width = std::nullopt;
        affectsLayout = true;
        break;
    case AttributeNames::heightAttr:
        m_height = std::nullopt;
        affectsLayout = true;
        break;
    case AttributeNames::depthAttr:
        m_depth = std::nullopt;
        affectsLayout = true;
        break;
    default:
        break;
    }

    if (affectsLayout) {
        if (CheckedPtr renderer = this->renderer())
            renderer->setNeedsLayoutAndPreferredWidthsUpdate();
    }

    MathMLPresentationElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

RenderPtr<RenderElement> MathMLSpaceElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    ASSERT(hasTagName(MathMLNames::mspaceTag));
    return createRenderer<RenderMathMLSpace>(*this, WTFMove(style));
}

}

#endif // ENABLE(MATHML)
