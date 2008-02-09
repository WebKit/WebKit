/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
 */
#include "config.h"
#include "HTMLTitleElement.h"

#include "Document.h"
#include "HTMLNames.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

HTMLTitleElement::HTMLTitleElement(Document* doc)
    : HTMLElement(titleTag, doc)
    , m_title("")
{
}

HTMLTitleElement::~HTMLTitleElement()
{
}

void HTMLTitleElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();
    document()->setTitle(m_title, this);
}

void HTMLTitleElement::removedFromDocument()
{
    HTMLElement::removedFromDocument();
    document()->removeTitle(this);
}

void HTMLTitleElement::childrenChanged(bool changedByParser)
{
    m_title = "";
    for (Node* c = firstChild(); c != 0; c = c->nextSibling())
        if (c->nodeType() == TEXT_NODE || c->nodeType() == CDATA_SECTION_NODE)
            m_title += c->nodeValue();
    if (inDocument())
        document()->setTitle(m_title, this);
    HTMLElement::childrenChanged(changedByParser);
}

String HTMLTitleElement::text() const
{
    String val = "";
    
    for (Node *n = firstChild(); n; n = n->nextSibling()) {
        if (n->isTextNode())
            val += static_cast<Text*>(n)->data();
    }
    
    return val;
}

void HTMLTitleElement::setText(const String &value)
{
    ExceptionCode ec = 0;
    int numChildren = childNodeCount();
    
    if (numChildren == 1 && firstChild()->isTextNode())
        static_cast<Text*>(firstChild())->setData(value, ec);
    else {  
        if (numChildren > 0)
            removeChildren();
    
        appendChild(document()->createTextNode(value.impl()), ec);
    }
}

}
