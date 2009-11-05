/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
    Copyright (C) 2006 Apple Computer, Inc.
    Copyright (C) 2009 Cameron McCormack <cam@mcc.id.au>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGStyleElement.h"

#include "CSSStyleSheet.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "MappedAttribute.h"
#include "SVGNames.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace SVGNames;

SVGStyleElement::SVGStyleElement(const QualifiedName& tagName, Document* doc, bool createdByParser)
     : SVGElement(tagName, doc)
     , SVGLangSpace()
     , m_createdByParser(createdByParser)
{
}

const AtomicString& SVGStyleElement::type() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, defaultValue, ("text/css"));
    const AtomicString& n = getAttribute(typeAttr);
    return n.isNull() ? defaultValue : n;
}

void SVGStyleElement::setType(const AtomicString& type, ExceptionCode& ec)
{
    setAttribute(typeAttr, type, ec);
}

const AtomicString& SVGStyleElement::media() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, defaultValue, ("all"));
    const AtomicString& n = getAttribute(mediaAttr);
    return n.isNull() ? defaultValue : n;
}

void SVGStyleElement::setMedia(const AtomicString& media, ExceptionCode& ec)
{
    setAttribute(mediaAttr, media, ec);
}

String SVGStyleElement::title() const
{
    return getAttribute(titleAttr);
}

void SVGStyleElement::setTitle(const AtomicString& title, ExceptionCode& ec)
{
    setAttribute(titleAttr, title, ec);
}

void SVGStyleElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == titleAttr && m_sheet)
        m_sheet->setTitle(attr->value());
    else {
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        SVGElement::parseMappedAttribute(attr);
    }
}

void SVGStyleElement::finishParsingChildren()
{
    StyleElement::sheet(this);
    m_createdByParser = false;
    SVGElement::finishParsingChildren();
}

void SVGStyleElement::insertedIntoDocument()
{
    SVGElement::insertedIntoDocument();
    document()->addStyleSheetCandidateNode(this, m_createdByParser);
    if (!m_createdByParser)
        StyleElement::insertedIntoDocument(document(), this);
}

void SVGStyleElement::removedFromDocument()
{
    SVGElement::removedFromDocument();
    if (document()->renderer())
        document()->removeStyleSheetCandidateNode(this);
    StyleElement::removedFromDocument(document());
}

void SVGStyleElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SVGElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    StyleElement::process(this);
}

StyleSheet* SVGStyleElement::sheet()
{
    return StyleElement::sheet(this);
}

bool SVGStyleElement::sheetLoaded()
{
    document()->removePendingSheet();
    return true;
}

}

// vim:ts=4:noet
#endif // ENABLE(SVG)
