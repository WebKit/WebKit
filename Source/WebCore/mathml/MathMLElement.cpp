/*
 * Copyright (C) 2009 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 François Sausset (sausset@gmail.com). All rights reserved.
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

#include "MathMLElement.h"

#include "MathMLNames.h"
#include "RenderObject.h"

namespace WebCore {
    
using namespace MathMLNames;
    
MathMLElement::MathMLElement(const QualifiedName& tagName, Document* document)
    : StyledElement(tagName, document, CreateStyledElement)
{
}
    
PassRefPtr<MathMLElement> MathMLElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new MathMLElement(tagName, document));
}

bool MathMLElement::isPresentationAttribute(Attribute* attr) const
{
    if (attr->name() == mathbackgroundAttr || attr->name() == mathsizeAttr || attr->name() == mathcolorAttr || attr->name() == fontsizeAttr || attr->name() == backgroundAttr || attr->name() == colorAttr || attr->name() == fontstyleAttr || attr->name() == fontweightAttr || attr->name() == fontfamilyAttr)
        return true;
    return StyledElement::isPresentationAttribute(attr);
}

void MathMLElement::collectStyleForAttribute(Attribute* attr, StylePropertySet* style)
{
    if (attr->name() == mathbackgroundAttr)
        style->setProperty(CSSPropertyBackgroundColor, attr->value());
    else if (attr->name() == mathsizeAttr) {
        // The following three values of mathsize are handled in WebCore/css/mathml.css
        if (attr->value() != "normal" && attr->value() != "small" && attr->value() != "big")
            style->setProperty(CSSPropertyFontSize, attr->value());
    } else if (attr->name() == mathcolorAttr)
        style->setProperty(CSSPropertyColor, attr->value());
    // FIXME: deprecated attributes that should loose in a conflict with a non deprecated attribute
    else if (attr->name() == fontsizeAttr)
        style->setProperty(CSSPropertyFontSize, attr->value());
    else if (attr->name() == backgroundAttr)
        style->setProperty(CSSPropertyBackgroundColor, attr->value());
    else if (attr->name() == colorAttr)
        style->setProperty(CSSPropertyColor, attr->value());
    else if (attr->name() == fontstyleAttr)
        style->setProperty(CSSPropertyFontStyle, attr->value());
    else if (attr->name() == fontweightAttr)
        style->setProperty(CSSPropertyFontWeight, attr->value());
    else if (attr->name() == fontfamilyAttr)
        style->setProperty(CSSPropertyFontFamily, attr->value());
    else {
        ASSERT(!isPresentationAttribute(attr));
        StyledElement::collectStyleForAttribute(attr, style);
    }
}

}

#endif // ENABLE(MATHML)
