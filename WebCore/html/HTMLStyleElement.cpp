/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
 *           (C) 2007 Rob Buis (buis@kde.org)
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
#include "HTMLStyleElement.h"

#include "Document.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

HTMLStyleElement::HTMLStyleElement(Document* doc)
    : HTMLElement(styleTag, doc)
    , m_loading(false)
    , m_createdByParser(false)
{
}

// other stuff...
void HTMLStyleElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == mediaAttr)
        m_media = attr->value().domString().lower();
    else if (attr->name() == titleAttr && m_sheet)
        m_sheet->setTitle(attr->value());
     else
        HTMLElement::parseMappedAttribute(attr);
}

void HTMLStyleElement::finishParsingChildren()
{
    StyleElement::sheet(this);
    m_createdByParser = false;
    HTMLElement::finishParsingChildren();
}

void HTMLStyleElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();

    if (!m_createdByParser)
        StyleElement::insertedIntoDocument(document(), this);
}

void HTMLStyleElement::removedFromDocument()
{
    HTMLElement::removedFromDocument();
    StyleElement::removedFromDocument(document());
}

void HTMLStyleElement::childrenChanged(bool changedByParser)
{
    StyleElement::process(this);
    HTMLElement::childrenChanged(changedByParser);
}

StyleSheet* HTMLStyleElement::sheet()
{
    return StyleElement::sheet(this);
}

bool HTMLStyleElement::isLoading() const
{
    if (m_loading)
        return true;
    if (!m_sheet)
        return false;
    return static_cast<CSSStyleSheet *>(m_sheet.get())->isLoading();
}

bool HTMLStyleElement::sheetLoaded()
{
    if (!isLoading()) {
        document()->removePendingSheet();
        return true;
    }
    return false;
}

bool HTMLStyleElement::disabled() const
{
    return !getAttribute(disabledAttr).isNull();
}

void HTMLStyleElement::setDisabled(bool disabled)
{
    setAttribute(disabledAttr, disabled ? "" : 0);
}

const AtomicString& HTMLStyleElement::media() const
{
    return getAttribute(mediaAttr);
}

void HTMLStyleElement::setMedia(const AtomicString &value)
{
    setAttribute(mediaAttr, value);
}

const AtomicString& HTMLStyleElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLStyleElement::setType(const AtomicString &value)
{
    setAttribute(typeAttr, value);
}

}
