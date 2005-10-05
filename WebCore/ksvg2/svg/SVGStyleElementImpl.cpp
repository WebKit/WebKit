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
#include <ksvg2/css/SVGCSSStyleSheetImpl.h>
#include <kdom/css/MediaListImpl.h>

#include "SVGDocumentImpl.h"
#include "SVGStyleElementImpl.h"

#include <qstring.h>

using namespace KSVG;

SVGStyleElementImpl::SVGStyleElementImpl(KDOM::DocumentPtr *doc, KDOM::NodeImpl::Id id, KDOM::DOMStringImpl *prefix) : SVGElementImpl(doc, id, prefix)
{
    m_sheet = 0;
    m_loading = false;
}

SVGStyleElementImpl::~SVGStyleElementImpl()
{
    if(m_sheet)
        m_sheet->deref();
}

KDOM::DOMStringImpl *SVGStyleElementImpl::xmlspace() const
{
    return tryGetAttribute(KDOM::DOMString("xml:space").handle());
}

void SVGStyleElementImpl::setXmlspace(KDOM::DOMStringImpl *)
{
    throw new KDOM::DOMExceptionImpl(KDOM::NO_MODIFICATION_ALLOWED_ERR);
}

KDOM::DOMStringImpl *SVGStyleElementImpl::type() const
{
    return tryGetAttribute(KDOM::DOMString("type").handle(), KDOM::DOMString("text/css").handle());
}

void SVGStyleElementImpl::setType(KDOM::DOMStringImpl *)
{
    throw new KDOM::DOMExceptionImpl(KDOM::NO_MODIFICATION_ALLOWED_ERR);
}

KDOM::DOMStringImpl *SVGStyleElementImpl::media() const
{
    return tryGetAttribute(KDOM::DOMString("media").handle(), KDOM::DOMString("all").handle());
}

void SVGStyleElementImpl::setMedia(KDOM::DOMStringImpl *)
{
    throw new KDOM::DOMExceptionImpl(KDOM::NO_MODIFICATION_ALLOWED_ERR);
}

KDOM::DOMStringImpl *SVGStyleElementImpl::title() const
{
    return tryGetAttribute(KDOM::DOMString("title").handle());
}

void SVGStyleElementImpl::setTitle(KDOM::DOMStringImpl *)
{
    throw new KDOM::DOMExceptionImpl(KDOM::NO_MODIFICATION_ALLOWED_ERR);
}

KDOM::CSSStyleSheetImpl *SVGStyleElementImpl::sheet()
{
    return m_sheet;
}

void SVGStyleElementImpl::childrenChanged()
{
    SVGElementImpl::childrenChanged();

    KDOM::DOMString text(textContent());

    if(m_sheet)
    {
        m_sheet->deref();
        m_sheet = 0;
    }

    m_loading = false;
    KDOM::DOMString mediaDomString(media());
    QString _media = mediaDomString.string();
    if((KDOM::DOMString(type()).isEmpty() || KDOM::DOMString(type()) == "text/css") && (_media.isNull() ||
        _media.contains(QString::fromLatin1("screen")) ||
        _media.contains(QString::fromLatin1("all")) |
        _media.contains(QString::fromLatin1("print"))))
    {
        ownerDocument()->addPendingSheet();

        m_loading = true;
 
        m_sheet = new SVGCSSStyleSheetImpl(this);
        m_sheet->ref();
        m_sheet->parseString(text.handle(), false);//!getDocument()->inCompatMode());

        KDOM::MediaListImpl *media = new KDOM::MediaListImpl(m_sheet, mediaDomString.handle());
        m_sheet->setMedia(media);
        m_loading = false;
    }

    if(!isLoading() && m_sheet)
    {
        if(getDocument())
            getDocument()->styleSheetLoaded();
    }
}

bool SVGStyleElementImpl::isLoading() const
{
    return false;
}

// vim:ts=4:noet
