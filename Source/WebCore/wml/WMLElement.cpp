/*
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#if ENABLE(WML)
#include "WMLElement.h"

#include "Attribute.h"
#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "RenderObject.h"
#include "WMLErrorHandling.h"
#include "WMLNames.h"
#include "WMLVariables.h"

using std::max;
using std::min;

namespace WebCore {

using namespace WMLNames;

WMLElement::WMLElement(const QualifiedName& tagName, Document* document)
    : StyledElement(tagName, document, CreateStyledElement)
{
}

PassRefPtr<WMLElement> WMLElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new WMLElement(tagName, document));
}

bool WMLElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == HTMLNames::alignAttr) {
        result = eUniversal;
        return false;
    }

    return StyledElement::mapToEntry(attrName, result);
}
    
void WMLElement::parseMappedAttribute(Attribute* attr)
{
    if (isIdAttributeName(attr->name())
        || attr->name() == HTMLNames::classAttr
        || attr->name() == HTMLNames::styleAttr)
        return StyledElement::parseMappedAttribute(attr);

    if (attr->name() == HTMLNames::alignAttr) {
        if (equalIgnoringCase(attr->value(), "middle"))
            addCSSProperty(attr, CSSPropertyTextAlign, "center");
        else
            addCSSProperty(attr, CSSPropertyTextAlign, attr->value());
    } else if (attr->name() == HTMLNames::tabindexAttr) {
        String indexstring = attr->value();
        int tabindex = 0;
        if (parseHTMLInteger(indexstring, tabindex)) {
            // Clamp tabindex to the range of 'short' to match Firefox's behavior.
            setTabIndexExplicitly(max(static_cast<int>(std::numeric_limits<short>::min()), min(tabindex, static_cast<int>(std::numeric_limits<short>::max()))));
        }
    }
}

String WMLElement::title() const
{
    return parseValueSubstitutingVariableReferences(getAttribute(HTMLNames::titleAttr));
}

bool WMLElement::rendererIsNeeded(RenderStyle* style)
{
    return document()->documentElement() == this || style->display() != NONE;
}

RenderObject* WMLElement::createRenderer(RenderArena*, RenderStyle* style)
{
    return RenderObject::createObject(this, style);
}

String WMLElement::parseValueSubstitutingVariableReferences(const AtomicString& value, WMLErrorCode defaultErrorCode) const
{
    bool isValid = false;
    if (!containsVariableReference(value, isValid))
        return value;

    if (!isValid) {
        reportWMLError(document(), defaultErrorCode);
        return String();
    }

    return substituteVariableReferences(value, document());
}

String WMLElement::parseValueForbiddingVariableReferences(const AtomicString& value) const
{
    bool isValid = false;
    if (containsVariableReference(value, isValid)) {
        reportWMLError(document(), WMLErrorInvalidVariableReferenceLocation);
        return String();
    }

    return value;
}

}

#endif
