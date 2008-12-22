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
#include "WMLTaskElement.h"

#include "WMLAnchorElement.h"
#include "WMLDoElement.h"
#include "WMLNames.h"
#include "WMLOnEventElement.h"
#include "WMLPageState.h"
#include "WMLSetvarElement.h"

namespace WebCore {

using namespace WMLNames;

WMLTaskElement::WMLTaskElement(const QualifiedName& tagName, Document* doc)
    : WMLElement(tagName, doc)
{
}

WMLTaskElement::~WMLTaskElement()
{
}

void WMLTaskElement::insertedIntoDocument()
{
    WMLElement::insertedIntoDocument();

    Node* parent = parentNode();
    ASSERT(parent);

    if (!parent || !parent->isWMLElement())
        return;

    if (parent->hasTagName(anchorTag))
        static_cast<WMLAnchorElement*>(parent)->registerTask(this);
    else if (parent->hasTagName(doTag))
        static_cast<WMLDoElement*>(parent)->registerTask(this);
    else if (parent->hasTagName(oneventTag))
        static_cast<WMLOnEventElement*>(parent)->registerTask(this);
}

void WMLTaskElement::registerVariableSetter(WMLSetvarElement* element)
{
    ASSERT(element);
    m_variableSetterElements.add(element);
}

void WMLTaskElement::storeVariableState(WMLPageState* pageState)
{
    if (!pageState || m_variableSetterElements.isEmpty())
        return;

    WMLVariableMap variables;
    HashSet<WMLSetvarElement*>::iterator it = m_variableSetterElements.begin();
    HashSet<WMLSetvarElement*>::iterator end = m_variableSetterElements.end();

    for (; it != end; ++it) {
        WMLSetvarElement* setterElement = (*it);
        if (setterElement->name().isEmpty())
            continue;

        variables.set(setterElement->name(), setterElement->value());
    }

    if (variables.isEmpty())
        return;

    pageState->storeVariables(variables);
}

}

#endif
