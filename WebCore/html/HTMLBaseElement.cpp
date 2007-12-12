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
#include "HTMLBaseElement.h"

#include "CSSHelper.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLNames.h"
#include "KURL.h"

namespace WebCore {

using namespace HTMLNames;

HTMLBaseElement::HTMLBaseElement(Document *doc)
    : HTMLElement(baseTag, doc)
{
}

HTMLBaseElement::~HTMLBaseElement()
{
}

void HTMLBaseElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == hrefAttr) {
        m_href = parseURL(attr->value());
        process();
    } else if (attr->name() == targetAttr) {
        m_target = attr->value();
        process();
    } else
        HTMLElement::parseMappedAttribute(attr);
}

void HTMLBaseElement::insertedIntoDocument()
{
    HTMLElement::insertedIntoDocument();
    process();
}

void HTMLBaseElement::removedFromDocument()
{
    HTMLElement::removedFromDocument();

    // Since the document doesn't have a base element...
    // (This will break in the case of multiple base elements, but that's not valid anyway (?))
    document()->setBaseURL(DeprecatedString::null);
    document()->setBaseTarget(DeprecatedString::null);
}

void HTMLBaseElement::process()
{
    if (!inDocument())
        return;

    if (!m_href.isEmpty() && document()->frame())
        document()->setBaseURL(KURL(document()->frame()->loader()->url(), m_href.deprecatedString()).deprecatedString());

    if (!m_target.isEmpty())
        document()->setBaseTarget(m_target);

    // ### should changing a document's base URL dynamically automatically update all images, stylesheets etc?
}

void HTMLBaseElement::setHref(const String &value)
{
    setAttribute(hrefAttr, value);
}

void HTMLBaseElement::setTarget(const String &value)
{
    setAttribute(targetAttr, value);
}

}
