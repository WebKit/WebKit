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
 */

#include "html/html_documentimpl.h"
#include "html/html_imageimpl.h"
#include "html/html_headimpl.h"
#include "html/html_baseimpl.h"
#include "html/htmltokenizer.h"
#include "html/html_miscimpl.h"

#include "khtmlview.h"
#include "khtml_part.h"
#include "khtml_settings.h"
#include "misc/htmlattrs.h"
#include "misc/htmlhashes.h"

#include "xml/xml_tokenizer.h"
#include "xml/dom2_eventsimpl.h"

#include "khtml_factory.h"
#include "rendering/render_object.h"

#include <dcopclient.h>
#include <kapplication.h>
#include <kdebug.h>
#include <kurl.h>
#include <kglobal.h>
#include <kcharsets.h>
#include <kglobalsettings.h>

#include "css/cssproperties.h"
#include "css/cssstyleselector.h"
#include "css/css_stylesheetimpl.h"
#include <stdlib.h>
#include <qptrstack.h>

#ifdef APPLE_CHANGES
#include <KWQKCookieJar.h>
#endif

template class QPtrStack<DOM::NodeImpl>;

using namespace DOM;
using namespace khtml;


HTMLDocumentImpl::HTMLDocumentImpl(DOMImplementationImpl *_implementation, KHTMLView *v)
  : DocumentImpl(_implementation, v)
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

HTMLDocumentImpl::~HTMLDocumentImpl()
{
}

DOMString HTMLDocumentImpl::referrer() const
{
    if ( view() )
        return view()->part()->referrer();
    return DOMString();
}

DOMString HTMLDocumentImpl::domain() const
{
    if ( m_domain.isEmpty() ) // not set yet (we set it on demand to save time and space)
        m_domain = KURL(URL()).host(); // Initially set to the host
    return m_domain;
}

void HTMLDocumentImpl::setDomain(const DOMString &newDomain, bool force /*=false*/)
{
    if ( force ) {
        m_domain = newDomain;
        return;
    }
    if ( m_domain.isEmpty() ) // not set yet (we set it on demand to save time and space)
        m_domain = KURL(URL()).host(); // Initially set to the host

    // Both NS and IE specify that changing the domain is only allowed when
    // the new domain is a suffix of the old domain.
    int oldLength = m_domain.length();
    int newLength = newDomain.length();
    if ( newLength < oldLength ) // e.g. newDomain=kde.org (7) and m_domain=www.kde.org (11)
    {
        DOMString test = m_domain.copy();
        if ( test[oldLength - newLength - 1] == '.' ) // Check that it's a subdomain, not e.g. "de.org"
        {
            test.remove( 0, oldLength - newLength ); // now test is "kde.org" from m_domain
            if ( test == newDomain )                 // and we check that it's the same thing as newDomain
                m_domain = newDomain;
        }
    }
}

DOMString HTMLDocumentImpl::lastModified() const
{
    if ( view() )
        return view()->part()->lastModified();
    return DOMString();
}

DOMString HTMLDocumentImpl::cookie() const
{
#ifdef APPLE_CHANGES
    return KWQKCookieJar::cookie(URL());
#else
    long windowId = 0;
    KHTMLView *v = view ();
    
    if ( v && v->topLevelWidget() )
      windowId = v->topLevelWidget()->winId();

    QCString replyType;
    QByteArray params, reply;
    QDataStream stream(params, IO_WriteOnly);
    stream << URL() << windowId;
    if (!kapp->dcopClient()->call("kcookiejar", "kcookiejar",
                                  "findDOMCookies(QString, int)", params, 
                                  replyType, reply)) {
         // Maybe it wasn't running (e.g. we're opening local html files)
         KApplication::startServiceByDesktopName( "kcookiejar");
         if (!kapp->dcopClient()->call("kcookiejar", "kcookiejar",
                                       "findDOMCookies(QString)", params, replyType, reply)) {
           kdWarning(6010) << "Can't communicate with cookiejar!" << endl;
           return DOMString();
         }
    }

    QDataStream stream2(reply, IO_ReadOnly);
    if(replyType != "QString") {
         kdError(6010) << "DCOP function findDOMCookies(...) returns "
                       << replyType << ", expected QString" << endl;
         return DOMString();
    }

    QString result;
    stream2 >> result;
    return DOMString(result);
#endif // APPLE_CHANGES
}

void HTMLDocumentImpl::setCookie( const DOMString & value )
{
#if APPLE_CHANGES
    return KWQKCookieJar::setCookie(URL(), m_policyBaseURL.string(), value.string());
#else
    long windowId = 0;
    KHTMLView *v = view ();
    
    if ( v && v->topLevelWidget() )
      windowId = v->topLevelWidget()->winId();
     
    QByteArray params;
    QDataStream stream(params, IO_WriteOnly);
    QString fake_header("Set-Cookie: ");
    fake_header.append(value.string());
    fake_header.append("\n");
    stream << URL() << fake_header.utf8() << windowId;
    if (!kapp->dcopClient()->send("kcookiejar", "kcookiejar",
                                  "addCookies(QString,QCString,long int)", params))
    {
         // Maybe it wasn't running (e.g. we're opening local html files)
         KApplication::startServiceByDesktopName( "kcookiejar");
         if (!kapp->dcopClient()->send("kcookiejar", "kcookiejar",
                                       "addCookies(QString,QCString,long int)", params))
             kdWarning(6010) << "Can't communicate with cookiejar!" << endl;
    }
#endif
}



HTMLElementImpl *HTMLDocumentImpl::body()
{
    NodeImpl *de = documentElement();
    if (!de)
        return 0;

    // try to prefer a FRAMESET element over BODY
    NodeImpl* body = 0;
    for (NodeImpl* i = de->firstChild(); i; i = i->nextSibling()) {
        if (i->id() == ID_FRAMESET)
            return static_cast<HTMLElementImpl*>(i);

        if (i->id() == ID_BODY)
            body = i;
    }
    return static_cast<HTMLElementImpl *>(body);
}

void HTMLDocumentImpl::setBody(HTMLElementImpl *_body)
{
    int exceptioncode = 0;
    HTMLElementImpl *b = body();
    if ( !_body && !b ) return;
    if ( !_body )
        documentElement()->removeChild( b, exceptioncode );
    else if ( !b )
        documentElement()->appendChild( _body, exceptioncode );
    else
        documentElement()->replaceChild( _body, b, exceptioncode );
}

Tokenizer *HTMLDocumentImpl::createTokenizer()
{
    return new HTMLTokenizer(docPtr(),m_view);
}

// --------------------------------------------------------------------------
// not part of the DOM
// --------------------------------------------------------------------------

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

    recalcStyle( Force );
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

static bool isTransitional(const QString &spec, int start)
{
    if((spec.find("TRANSITIONAL", start, false ) != -1 ) ||
       (spec.find("LOOSE", start, false ) != -1 ) ||
       (spec.find("FRAMESET", start, false ) != -1 ) ||
       (spec.find("LATIN1", start, false ) != -1 ) ||
       (spec.find("SYMBOLS", start, false ) != -1 ) ||
       (spec.find("SPECIAL", start, false ) != -1 ) ) {
        //kdDebug() << "isTransitional" << endl;
        return true;
    }
    return false;
}

void HTMLDocumentImpl::close()
{
    bool doload = !parsing() && m_tokenizer;

    DocumentImpl::close();

    if (body() && doload) {
        body()->dispatchWindowEvent(EventImpl::LOAD_EVENT, false, false);
        updateRendering();
    }
}


void HTMLDocumentImpl::determineParseMode( const QString &str )
{
    //kdDebug() << "DocumentImpl::determineParseMode str=" << str<< endl;
    // determines the parse mode for HTML
    // quite some hints here are inspired by the mozilla code.

    // default parsing mode is Loose
    pMode = Compat;
    hMode = Html3;

    ParseMode systemId = Unknown;
    ParseMode publicId = Unknown;

    int pos = 0;
    int doctype = str.find("!doctype", 0, false);
    if( doctype > 2 ) {
        pos = doctype - 2;
        // Store doctype name
        int start = doctype + 9;
        while ( start < (int)str.length() && str[start].isSpace() )
            start++;
        int espace = str.find(' ',start);
        QString name = str.mid(start,espace-start);
        //kdDebug() << "DocumentImpl::determineParseMode setName: " << name << endl;
        m_doctype->setName( name );
    }

    // get the first tag (or the doctype tag)
    int start = str.find('<', pos);
    int stop = str.find('>', pos);
    if( start > -1 && stop > start ) {
        QString spec = str.mid( start + 1, stop - start - 1 );
        //kdDebug() << "DocumentImpl::determineParseMode dtd=" << spec<< endl;
        start = 0;
        int quote = -1;
        if( doctype != -1 ) {
            while( (quote = spec.find( "\"", start )) != -1 ) {
                int quote2 = spec.find( "\"", quote+1 );
                if(quote2 < 0) quote2 = spec.length();
                QString val = spec.mid( quote+1, quote2 - quote-1 );
                //kdDebug() << "DocumentImpl::determineParseMode val = " << val << endl;
                // find system id
                pos = val.find("http://www.w3.org/tr/", 0, false);
                if ( pos != -1 ) {
                    // loose or strict dtd?
                    if ( val.find("strict.dtd", pos, false) != -1 )
                        systemId = Strict;
                    else if (isTransitional(val, pos))
                        systemId = Transitional;
                }

                // find public id
                pos = val.find("//dtd", 0, false );
                if ( pos != -1 ) {
                    if( val.find( "xhtml", pos+6, false ) != -1 ) {
                        hMode = XHtml;
                        publicId = isTransitional(val, pos) ? Transitional : Strict;
                    } else if ( val.find( "15445:1999", pos+6 ) != -1 ) {
                        hMode = Html4;
                        publicId = Strict;
                    } else {
                        int tagPos = val.find( "html", pos+6, false );
                        if( tagPos == -1 )
                            tagPos = val.find( "hypertext markup", pos+6, false );
                        if ( tagPos != -1 ) {
                            tagPos = val.find(QRegExp("[0-9]"), tagPos );
                            int version = val.mid( tagPos, 1 ).toInt();
                            //kdDebug() << "DocumentImpl::determineParseMode tagPos = " << tagPos << " version=" << version << endl;
                            if( version > 3 ) {
                                hMode = Html4;
                                publicId = isTransitional( val, tagPos ) ? Transitional : Strict;
                            }
                        }
                    }
                }
                start = quote2 + 1;
            }
        }

        if( systemId == publicId )
            pMode = publicId;
        else if ( systemId == Unknown )
            pMode = hMode == Html4 ? Compat : publicId;
        else if ( publicId == Transitional && systemId == Strict ) {
            pMode = hMode == Html3 ? Compat : Strict;
        } else
            pMode = Compat;

        if ( hMode == XHtml )
            pMode = Strict;
    }
//     kdDebug() << "DocumentImpl::determineParseMode: publicId =" << publicId << " systemId = " << systemId << endl;
//     kdDebug() << "DocumentImpl::determineParseMode: htmlMode = " << hMode<< endl;
//     if( pMode == Strict )
//         kdDebug(6020) << " using strict parseMode" << endl;
//     else if (pMode == Compat )
//         kdDebug(6020) << " using compatibility parseMode" << endl;
//     else
//         kdDebug(6020) << " using transitional parseMode" << endl;
}

#include "html_documentimpl.moc"
