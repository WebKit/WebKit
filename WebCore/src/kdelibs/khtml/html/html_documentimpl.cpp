/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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
 *
 * $Id$
 */
#include "html_documentimpl.h"

#include "khtmlview.h"
#include "khtml_part.h"
#include "khtml_settings.h"
#include "misc/htmlattrs.h"

#include "htmltokenizer.h"
#include "xml_tokenizer.h"
#include "htmlhashes.h"
#include "html_imageimpl.h"
#include "html_headimpl.h"
#include "html/html_baseimpl.h"
#include "dom2_eventsimpl.h"

#include "khtml_factory.h"
#include "html_miscimpl.h"
#include "rendering/render_object.h"

#include <kdebug.h>
#include <kurl.h>
#include <kglobal.h>
#include <kcharsets.h>
#include <kglobalsettings.h>

#include "css/cssstyleselector.h"
#include "css/css_stylesheetimpl.h"
#include <stdlib.h>
#include <qstack.h>

template class QStack<DOM::NodeImpl>;

using namespace DOM;
using namespace khtml;


HTMLDocumentImpl::HTMLDocumentImpl() : DocumentImpl()
{
//    kdDebug( 6090 ) << "HTMLDocumentImpl constructor this = " << this << endl;
    bodyElement = 0;
    htmlElement = 0;

/* dynamic history stuff to be fixed later (pfeiffer)
    connect( KHTMLFactory::vLinks(), SIGNAL( inserted( const QString& )),
             SLOT( slotHistoryChanged() ));
    connect( KHTMLFactory::vLinks(), SIGNAL( removed( const QString& )),
             SLOT( slotHistoryChanged() ));
*/
    connect( KHTMLFactory::vLinks(), SIGNAL( cleared()),
             SLOT( slotHistoryChanged() ));
}

HTMLDocumentImpl::HTMLDocumentImpl(KHTMLView *v)
    : DocumentImpl(v)
{
//    kdDebug( 6090 ) << "HTMLDocumentImpl constructor2 this = " << this << endl;
    bodyElement = 0;
    htmlElement = 0;

    m_styleSelector = new CSSStyleSelector(this);

/*
    connect( KHTMLFactory::vLinks(), SIGNAL( inserted( const QString& )),
             SLOT( slotHistoryChanged() ));
    connect( KHTMLFactory::vLinks(), SIGNAL( removed( const QString& )),
             SLOT( slotHistoryChanged() ));
*/
    connect( KHTMLFactory::vLinks(), SIGNAL( cleared()),
             SLOT( slotHistoryChanged() ));
}

HTMLDocumentImpl::~HTMLDocumentImpl()
{
}

DOMString HTMLDocumentImpl::referrer() const
{
    // ### should we fix that? I vote against for privacy reasons
    return DOMString();
}

DOMString HTMLDocumentImpl::domain() const
{
    // ### do they want the host or the domain????
    KURL u(url.string());
    return u.host();
}

HTMLElementImpl *HTMLDocumentImpl::body()
{
    NodeImpl *de = documentElement();
    if (!de)
        return 0;
    NodeImpl *b = de->firstChild();
    while (b && b->id() != ID_BODY && b->id() != ID_FRAMESET)
        b = b->nextSibling();
    return static_cast<HTMLElementImpl *>(b);
}

void HTMLDocumentImpl::setBody(HTMLElementImpl *_body)
{
    int exceptioncode = 0;
    HTMLElementImpl *b = body();
    if ( !_body && !b ) return;
    if ( !_body )
        documentElement()->removeChild( body(), exceptioncode );
    if ( !b )
        documentElement()->appendChild( _body, exceptioncode );
    documentElement()->replaceChild( _body, body(), exceptioncode );
}

Tokenizer *HTMLDocumentImpl::createTokenizer()
{
    return new HTMLTokenizer(docPtr(),m_view);
}

NodeListImpl *HTMLDocumentImpl::getElementsByName( const DOMString &elementName )
{
    return new NameNodeListImpl( documentElement(), elementName );
}

// --------------------------------------------------------------------------
// not part of the DOM
// --------------------------------------------------------------------------

void HTMLDocumentImpl::detach()
{
    if ( view() && view()->part() )
        view()->part()->emitUnloadEvent();

    DocumentImpl::detach();
}

bool HTMLDocumentImpl::childAllowed( NodeImpl *newChild )
{
    // ### support comments. etc as a child
    return (newChild->id() == ID_HTML || newChild->id() == ID_COMMENT);
}

ElementImpl *HTMLDocumentImpl::createElement( const DOMString &name )
{
    return createHTMLElement(name);
}

void HTMLDocumentImpl::slotHistoryChanged()
{
    if ( true || !m_render ) // disabled for now
        return;

    recalcStyle();
    m_render->repaint();
}

HTMLMapElementImpl* HTMLDocumentImpl::getMap(const DOMString& _url)
{
    QString url = _url.string();
    QString s;
    int pos = url.find('#');
    //kdDebug(0) << "map pos of #:" << pos << endl;
    s = QString(_url.unicode() + pos + 1, _url.length() - pos - 1);

    QMapConstIterator<QString,HTMLMapElementImpl*> it = mapMap.find(s);

    if (it != mapMap.end())
        return *it;
    else
        return 0;
}

//-----------------------------------------------------------------------------


XHTMLDocumentImpl::XHTMLDocumentImpl() : HTMLDocumentImpl()
{
}

XHTMLDocumentImpl::XHTMLDocumentImpl(KHTMLView *v) : HTMLDocumentImpl(v)
{
}

XHTMLDocumentImpl::~XHTMLDocumentImpl()
{
}

Tokenizer *XHTMLDocumentImpl::createTokenizer()
{
    return new XMLTokenizer(docPtr(),m_view);
}



#include "html_documentimpl.moc"
