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
#include "WMLNoopElement.h"

#include "WMLDoElement.h"
#include "WMLErrorHandling.h"
#include "WMLNames.h"

namespace WebCore {

using namespace WMLNames;

WMLNoopElement::WMLNoopElement(const QualifiedName& tagName, Document* doc)
    : WMLElement(tagName, doc)
{
}

PassRefPtr<WMLNoopElement> WMLNoopElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new WMLNoopElement(tagName, document));
}

WMLNoopElement::~WMLNoopElement()
{
}

void WMLNoopElement::insertedIntoDocument()
{
    WMLElement::insertedIntoDocument();

    ContainerNode* parent = parentNode();
    if (!parent || !parent->isWMLElement())
        return;

    if (parent->hasTagName(doTag)) {
        WMLDoElement* doElement = static_cast<WMLDoElement*>(parent);
        doElement->setNoop(true);

        if (doElement->attached())
            doElement->detach();

        ASSERT(!doElement->attached());
        doElement->attach();
    } else if (parent->hasTagName(anchorTag))
        reportWMLError(document(), WMLErrorForbiddenTaskInAnchorElement);
}

}

#endif
