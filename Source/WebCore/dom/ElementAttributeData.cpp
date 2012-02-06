/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
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
#include "ElementAttributeData.h"

#include "StyledElement.h"

namespace WebCore {

void ElementAttributeData::setClass(const String& className, bool shouldFoldCase)
{
    m_classNames.set(className, shouldFoldCase);
}

StylePropertySet* ElementAttributeData::ensureInlineStyleDecl(Element* element)
{
    if (!m_inlineStyleDecl) {
        ASSERT(element->isStyledElement());
        m_inlineStyleDecl = StylePropertySet::createInline(static_cast<StyledElement*>(element));
        m_inlineStyleDecl->setStrictParsing(element->isHTMLElement() && !element->document()->inQuirksMode());
    }
    return m_inlineStyleDecl.get();
}

void ElementAttributeData::destroyInlineStyleDecl()
{
    if (!m_inlineStyleDecl)
        return;
    m_inlineStyleDecl->clearParentElement();
    m_inlineStyleDecl = 0;
}

StylePropertySet* ElementAttributeData::ensureAttributeStyle(StyledElement* element)
{
    if (!m_attributeStyle)
        m_attributeStyle = StylePropertySet::createAttributeStyle(element);
    return m_attributeStyle.get();
}

}
