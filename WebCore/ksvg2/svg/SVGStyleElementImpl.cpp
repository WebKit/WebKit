/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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
#include <kdom/kdom.h>
#include <kdom/DOMString.h>
#include "DocumentImpl.h"
#include "css_stylesheetimpl.h"

#include "SVGStyleElementImpl.h"

#include <qstring.h>

using namespace KSVG;

SVGStyleElementImpl::SVGStyleElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc) : SVGElementImpl(tagName, doc)
{
    m_loading = false;
}

SVGStyleElementImpl::~SVGStyleElementImpl()
{
}

KDOM::AtomicString SVGStyleElementImpl::xmlspace() const
{
    return tryGetAttribute("xml:space");
}

void SVGStyleElementImpl::setXmlspace(const KDOM::AtomicString&, int &exceptioncode)
{
    exceptioncode = KDOM::NO_MODIFICATION_ALLOWED_ERR;
}

KDOM::AtomicString SVGStyleElementImpl::type() const
{
    return tryGetAttribute("type", "text/css");
}

void SVGStyleElementImpl::setType(const KDOM::AtomicString&, int &exceptioncode)
{
    exceptioncode = KDOM::NO_MODIFICATION_ALLOWED_ERR;
}

KDOM::AtomicString SVGStyleElementImpl::media() const
{
    return tryGetAttribute("media", "all");
}

void SVGStyleElementImpl::setMedia(const KDOM::AtomicString&, int& exceptioncode)
{
    exceptioncode = KDOM::NO_MODIFICATION_ALLOWED_ERR;
}

KDOM::AtomicString SVGStyleElementImpl::title() const
{
    return tryGetAttribute("title");
}

void SVGStyleElementImpl::setTitle(const KDOM::AtomicString&, int& exceptioncode)
{
    exceptioncode = KDOM::NO_MODIFICATION_ALLOWED_ERR;
}

KDOM::CSSStyleSheetImpl *SVGStyleElementImpl::sheet()
{
    return m_sheet.get();
}

void SVGStyleElementImpl::childrenChanged()
{
    SVGElementImpl::childrenChanged();

    if(m_sheet)
        m_sheet = 0;

    m_loading = false;
    const KDOM::AtomicString &mediaDomString = media();
    QString _media = mediaDomString.qstring();
    if((type().isEmpty() || type() == "text/css") && (_media.isNull() ||
        _media.contains(QString::fromLatin1("screen")) ||
        _media.contains(QString::fromLatin1("all")) |
        _media.contains(QString::fromLatin1("print"))))
    {
        ownerDocument()->addPendingSheet();

        m_loading = true;
 
        m_sheet = new KDOM::CSSStyleSheetImpl(this);
        m_sheet->parseString(textContent(), false);//!getDocument()->inCompatMode());

        KDOM::MediaListImpl *mediaList = new KDOM::MediaListImpl(m_sheet.get(), mediaDomString);
        m_sheet->setMedia(mediaList);
        m_loading = false;
    }

    if(!isLoading() && m_sheet)
        getDocument()->stylesheetLoaded();
}

bool SVGStyleElementImpl::isLoading() const
{
    return false;
}

// vim:ts=4:noet
