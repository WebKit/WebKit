/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) 2006 Apple Computer, Inc.

    This file is part of the KDE project

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#if SVG_SUPPORT
#include "SVGStyleElement.h"

#include "Document.h"
#include "ExceptionCode.h"
#include "PlatformString.h"
#include "DeprecatedString.h"
#include "css_stylesheetimpl.h"

using namespace WebCore;

SVGStyleElement::SVGStyleElement(const QualifiedName& tagName, Document *doc) : SVGElement(tagName, doc)
{
    m_loading = false;
}

SVGStyleElement::~SVGStyleElement()
{
}

AtomicString SVGStyleElement::xmlspace() const
{
    return tryGetAttribute("xml:space");
}

void SVGStyleElement::setXmlspace(const AtomicString&, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
}

AtomicString SVGStyleElement::type() const
{
    return tryGetAttribute("type", "text/css");
}

void SVGStyleElement::setType(const AtomicString&, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
}

AtomicString SVGStyleElement::media() const
{
    return tryGetAttribute("media", "all");
}

void SVGStyleElement::setMedia(const AtomicString&, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
}

AtomicString SVGStyleElement::title() const
{
    return tryGetAttribute("title");
}

void SVGStyleElement::setTitle(const AtomicString&, ExceptionCode& ec)
{
    ec = NO_MODIFICATION_ALLOWED_ERR;
}

CSSStyleSheet *SVGStyleElement::sheet()
{
    return m_sheet.get();
}

void SVGStyleElement::childrenChanged()
{
    SVGElement::childrenChanged();

    if(m_sheet)
        m_sheet = 0;

    m_loading = false;
    const AtomicString& _media = media();
    if ((type().isEmpty() || type() == "text/css") && (_media.isNull() || _media.contains("screen") || _media.contains("all") || _media.contains("print"))) {
        ownerDocument()->addPendingSheet();

        m_loading = true;
 
        m_sheet = new CSSStyleSheet(this);
        m_sheet->parseString(textContent(), false);//!document()->inCompatMode());

        MediaList *mediaList = new MediaList(m_sheet.get(), _media);
        m_sheet->setMedia(mediaList);
        m_loading = false;
    }

    if(!isLoading() && m_sheet)
        document()->stylesheetLoaded();
}

bool SVGStyleElement::isLoading() const
{
    return false;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT
