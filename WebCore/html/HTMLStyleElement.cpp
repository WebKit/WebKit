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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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
{
}

// other stuff...
void HTMLStyleElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == typeAttr)
        m_type = attr->value().domString().lower();
    else if (attr->name() == mediaAttr)
        m_media = attr->value().deprecatedString().lower();
    else
        HTMLElement::parseMappedAttribute(attr);
}

void HTMLStyleElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();
    if (m_sheet)
        document()->updateStyleSelector();
}

void HTMLStyleElement::removedFromDocument()
{
    HTMLElement::removedFromDocument();
    if (m_sheet)
        document()->updateStyleSelector();
}

void HTMLStyleElement::childrenChanged()
{
    String text = "";

    for (Node* c = firstChild(); c; c = c->nextSibling())
        if (c->nodeType() == TEXT_NODE || c->nodeType() == CDATA_SECTION_NODE || c->nodeType() == COMMENT_NODE)
            text += c->nodeValue();

    if (m_sheet) {
        if (static_cast<CSSStyleSheet *>(m_sheet.get())->isLoading())
            document()->stylesheetLoaded(); // Remove ourselves from the sheet list.
        m_sheet = 0;
    }

    m_loading = false;
    if ((m_type.isEmpty() || m_type == "text/css") // Type must be empty or CSS
         && (m_media.isNull() || m_media.contains("screen") || m_media.contains("all") || m_media.contains("print"))) {
        document()->addPendingSheet();
        m_loading = true;
        m_sheet = new CSSStyleSheet(this);
        m_sheet->parseString(text, !document()->inCompatMode());
        MediaList *media = new MediaList(m_sheet.get(), m_media);
        m_sheet->setMedia(media);
        m_loading = false;
    }

    if (!isLoading() && m_sheet)
        document()->stylesheetLoaded();
}

bool HTMLStyleElement::isLoading() const
{
    if (m_loading)
        return true;
    if (!m_sheet)
        return false;
    return static_cast<CSSStyleSheet *>(m_sheet.get())->isLoading();
}

void HTMLStyleElement::sheetLoaded()
{
    if (!isLoading())
        document()->stylesheetLoaded();
}

bool HTMLStyleElement::disabled() const
{
    return !getAttribute(disabledAttr).isNull();
}

void HTMLStyleElement::setDisabled(bool disabled)
{
    setAttribute(disabledAttr, disabled ? "" : 0);
}

String HTMLStyleElement::media() const
{
    return getAttribute(mediaAttr);
}

void HTMLStyleElement::setMedia(const String &value)
{
    setAttribute(mediaAttr, value);
}

String HTMLStyleElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLStyleElement::setType(const String &value)
{
    setAttribute(typeAttr, value);
}

}
