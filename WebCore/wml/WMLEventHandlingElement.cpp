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
#include "WMLEventHandlingElement.h"

#include "WMLDoElement.h"
#include "WMLIntrinsicEventHandler.h"
#include "WMLTaskElement.h"
#include "WMLNames.h"

namespace WebCore {

using namespace WMLNames;

WMLEventHandlingElement::WMLEventHandlingElement(const QualifiedName& tagName, Document* doc)
    : WMLElement(tagName, doc)
{
}

WMLEventHandlingElement::~WMLEventHandlingElement()
{
}

void WMLEventHandlingElement::createEventHandlerIfNeeded()
{
    if (!m_eventHandler)
        m_eventHandler.set(new WMLIntrinsicEventHandler);
}

void WMLEventHandlingElement::registerDoElement(WMLDoElement* doElement)
{
    Vector<WMLDoElement*>::iterator it = m_doElements.begin();
    Vector<WMLDoElement*>::iterator end = m_doElements.end();

    for (; it != end; ++it) {
        if ((*it)->name() == doElement->name()) {
            reportWMLError(document(), WMLErrorDuplicatedDoElement);
            return;
        }
    }

    m_doElements.append(doElement);
    doElement->setActive(true);
}

}

#endif
