/**
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
#include "WMLSetvarElement.h"

#include "Attribute.h"
#include "HTMLNames.h"
#include "WMLErrorHandling.h"
#include "WMLTaskElement.h"
#include "WMLVariables.h"

namespace WebCore {

WMLSetvarElement::WMLSetvarElement(const QualifiedName& tagName, Document* doc)
    : WMLElement(tagName, doc)
{
}

PassRefPtr<WMLSetvarElement> WMLSetvarElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new WMLSetvarElement(tagName, document));
}

WMLSetvarElement::~WMLSetvarElement()
{
}

void WMLSetvarElement::parseMappedAttribute(Attribute* attr)
{
    if (attr->name() == HTMLNames::nameAttr) {
        if (!isValidVariableName(parseValueSubstitutingVariableReferences(attr->value(), WMLErrorInvalidVariableName))) {
            reportWMLError(document(), WMLErrorInvalidVariableName);
            return;
        }
    } else
        WMLElement::parseMappedAttribute(attr);
}

void WMLSetvarElement::insertedIntoDocument()
{
    WMLElement::insertedIntoDocument();
 
    Node* parent = parentNode();
    if (!parent || !parent->isWMLElement())
        return;

    if (static_cast<WMLElement*>(parent)->isWMLTaskElement())
        static_cast<WMLTaskElement*>(parent)->registerVariableSetter(this);
}

void WMLSetvarElement::removedFromDocument()
{
    Node* parent = parentNode();
    if (parent && parent->isWMLElement()) {
        if (static_cast<WMLElement*>(parent)->isWMLTaskElement())
            static_cast<WMLTaskElement*>(parent)->deregisterVariableSetter(this);
    }

    WMLElement::removedFromDocument(); 
}

String WMLSetvarElement::name() const
{
    return parseValueSubstitutingVariableReferences(getAttribute(HTMLNames::nameAttr), WMLErrorInvalidVariableName);
}

String WMLSetvarElement::value() const
{
    return parseValueSubstitutingVariableReferences(getAttribute(HTMLNames::valueAttr));
}

}

#endif
