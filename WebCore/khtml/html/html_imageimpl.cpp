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
 */

#include "html/html_imageimpl.h"
#include "html/html_documentimpl.h"

#include "misc/htmlhashes.h"
#include "khtmlview.h"
#include "khtml_part.h"

#include <kstringhandler.h>
#include <kglobal.h>
#include <kdebug.h>

#include "rendering/render_image.h"
#include "rendering/render_flow.h"
#include "css/cssstyleselector.h"
#include "css/cssproperties.h"
#include "css/cssvalues.h"
#include "css/csshelper.h"
#include "xml/dom2_eventsimpl.h"

#include <qstring.h>
#include <qpoint.h>
#include <qregion.h>
#include <qptrstack.h>
#include <qimage.h>
#include <qpointarray.h>

using namespace DOM;
using namespace khtml;

// -------------------------------------------------------------------------

HTMLImageElementImpl::HTMLImageElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
    ismap = false;
}

HTMLImageElementImpl::~HTMLImageElementImpl()
{
}


// DOM related

NodeImpl::Id HTMLImageElementImpl::id() const
{
    return ID_IMG;
}

void HTMLImageElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch (attr->id())
    {
    case ATTR_ALT:
    case ATTR_SRC:
        if (m_render) m_render->updateFromElement();
        break;
    case ATTR_WIDTH:
        addCSSLength(CSS_PROP_WIDTH, attr->value());
        break;
    case ATTR_HEIGHT:
        addCSSLength(CSS_PROP_HEIGHT, attr->value());
        break;
    case ATTR_BORDER:
        // border="noborder" -> border="0"
        if(attr->value().toInt()) {
            addCSSLength(CSS_PROP_BORDER_WIDTH, attr->value());
            addCSSProperty( CSS_PROP_BORDER_TOP_STYLE, CSS_VAL_SOLID );
            addCSSProperty( CSS_PROP_BORDER_RIGHT_STYLE, CSS_VAL_SOLID );
            addCSSProperty( CSS_PROP_BORDER_BOTTOM_STYLE, CSS_VAL_SOLID );
            addCSSProperty( CSS_PROP_BORDER_LEFT_STYLE, CSS_VAL_SOLID );
        }
        break;
    case ATTR_VSPACE:
        addCSSLength(CSS_PROP_MARGIN_TOP, attr->value());
        addCSSLength(CSS_PROP_MARGIN_BOTTOM, attr->value());
        break;
    case ATTR_HSPACE:
        addCSSLength(CSS_PROP_MARGIN_LEFT, attr->value());
        addCSSLength(CSS_PROP_MARGIN_RIGHT, attr->value());
        break;
    case ATTR_ALIGN:
	addHTMLAlignment( attr->value() );
	break;
    case ATTR_VALIGN:
	addCSSProperty(CSS_PROP_VERTICAL_ALIGN, attr->value());
        break;
    case ATTR_USEMAP:
        if ( attr->value()[0] == '#' )
            usemap = attr->value();
        else {
            QString url = getDocument()->completeURL( khtml::parseURL( attr->value() ).string() );
            // ### we remove the part before the anchor and hope
            // the map is on the same html page....
            usemap = url;
        }
        m_hasAnchor = attr->val() != 0;
    case ATTR_ISMAP:
        ismap = true;
        break;
    case ATTR_ONABORT: // ### add support for this
        setHTMLEventListener(EventImpl::ABORT_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONERROR: // ### add support for this
        setHTMLEventListener(EventImpl::ERROR_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_ONLOAD:
        setHTMLEventListener(EventImpl::LOAD_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string()));
        break;
    case ATTR_NAME:
    case ATTR_NOSAVE:
	break;
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}

DOMString HTMLImageElementImpl::altText() const
{
    // lets figure out the alt text.. magic stuff
    // http://www.w3.org/TR/1998/REC-html40-19980424/appendix/notes.html#altgen
    // also heavily discussed by Hixie on bugzilla
    DOMString alt( getAttribute( ATTR_ALT ) );
    // fall back to title attribute
    if ( alt.isNull() )
        alt = getAttribute( ATTR_TITLE );
#if 0
    if ( alt.isNull() ) {
        QString p = KURL( getDocument()->completeURL( getAttribute(ATTR_SRC).string() ) ).prettyURL();
        int pos;
        if ( ( pos = p.findRev( '.' ) ) > 0 )
            p.truncate( pos );
        alt = DOMString( KStringHandler::csqueeze( p ) );
    }
#endif

    return alt;
}

void HTMLImageElementImpl::attach()
{
    assert(!attached());
    assert(!m_render);
    assert(parentNode());

    RenderStyle* _style = getDocument()->styleSelector()->styleForElement(this);
    _style->ref();
    if (parentNode()->renderer() && _style->display() != NONE) {
        m_render = new RenderImage(this);
        m_render->setStyle(getDocument()->styleSelector()->styleForElement(this));
        parentNode()->renderer()->addChild(m_render, nextRenderer());
        m_render->updateFromElement();
    }
    _style->deref();

    NodeBaseImpl::attach();
}

long HTMLImageElementImpl::width() const
{
    if (!m_render) return getAttribute(ATTR_WIDTH).toInt();

    // ### make a unified call for this
    if (changed()) {
        getDocument()->updateRendering();
        if (getDocument()->view())
            getDocument()->view()->layout();
    }

    return m_render->contentWidth();
}

long HTMLImageElementImpl::height() const
{
    if (!m_render) return getAttribute(ATTR_HEIGHT).toInt();

    // ### make a unified call for this
    if (changed()) {
        getDocument()->updateRendering();
        if (getDocument()->view())
            getDocument()->view()->layout();
    }

    return m_render->contentHeight();
}

QImage HTMLImageElementImpl::currentImage() const
{
    RenderImage *r = static_cast<RenderImage*>(renderer());

    if(r)
        return r->pixmap().convertToImage();

    return QImage();
}

// -------------------------------------------------------------------------

HTMLMapElementImpl::HTMLMapElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLMapElementImpl::~HTMLMapElementImpl()
{
    if(getDocument() && getDocument()->isHTMLDocument())
        static_cast<HTMLDocumentImpl*>(getDocument())->mapMap.remove(name);
}

NodeImpl::Id HTMLMapElementImpl::id() const
{
    return ID_MAP;
}

bool
HTMLMapElementImpl::mapMouseEvent(int x_, int y_, int width_, int height_,
                                  RenderObject::NodeInfo& info)
{
    //cout << "map:mapMouseEvent " << endl;
    //cout << x_ << " " << y_ <<" "<< width_ <<" "<< height_ << endl;
    QPtrStack<NodeImpl> nodeStack;

    NodeImpl *current = firstChild();
    while(1)
    {
        if(!current)
        {
            if(nodeStack.isEmpty()) break;
            current = nodeStack.pop();
            current = current->nextSibling();
            continue;
        }
        if(current->id()==ID_AREA)
        {
            //cout << "area found " << endl;
            HTMLAreaElementImpl* area=static_cast<HTMLAreaElementImpl*>(current);
            if (area->mapMouseEvent(x_,y_,width_,height_, info))
                return true;
        }
        NodeImpl *child = current->firstChild();
        if(child)
        {
            nodeStack.push(current);
            current = child;
        }
        else
        {
            current = current->nextSibling();
        }
    }

    return false;
}

void HTMLMapElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch (attr->id())
    {
    case ATTR_ID:
        if (getDocument()->htmlMode() != DocumentImpl::XHtml) break;
        // fall through
    case ATTR_NAME:
    {
        DOMString s = attr->value();
        if(*s.unicode() == '#')
            name = QString(s.unicode()+1, s.length()-1);
        else
            name = s.string();
	// ### make this work for XML documents, e.g. in case of <html:map...>
        if(getDocument()->isHTMLDocument())
            static_cast<HTMLDocumentImpl*>(getDocument())->mapMap[name] = this;
        break;
    }
    default:
        HTMLElementImpl::parseAttribute(attr);
    }
}

// -------------------------------------------------------------------------

HTMLAreaElementImpl::HTMLAreaElementImpl(DocumentPtr *doc)
    : HTMLAnchorElementImpl(doc)
{
    m_coords=0;
    m_coordsLen = 0;
    nohref = false;
    shape = Unknown;
    lasth = lastw = -1;
}

HTMLAreaElementImpl::~HTMLAreaElementImpl()
{
    if (m_coords) delete [] m_coords;
}

NodeImpl::Id HTMLAreaElementImpl::id() const
{
    return ID_AREA;
}

void HTMLAreaElementImpl::parseAttribute(AttributeImpl *attr)
{
    switch (attr->id())
    {
    case ATTR_SHAPE:
        if ( strcasecmp( attr->value(), "default" ) == 0 )
            shape = Default;
        else if ( strcasecmp( attr->value(), "circle" ) == 0 )
            shape = Circle;
        else if ( strcasecmp( attr->value(), "poly" ) == 0 )
            shape = Poly;
        else if ( strcasecmp( attr->value(), "rect" ) == 0 )
            shape = Rect;
        break;
    case ATTR_COORDS:
        if (m_coords) delete [] m_coords;
        m_coords = attr->val()->toLengthArray(m_coordsLen);
        break;
    case ATTR_NOHREF:
        nohref = attr->val() != 0;
        break;
    case ATTR_TARGET:
        m_hasTarget = attr->val() != 0;
        break;
    case ATTR_ALT:
        break;
    case ATTR_ACCESSKEY:
        break;
    default:
        HTMLAnchorElementImpl::parseAttribute(attr);
    }
}

bool HTMLAreaElementImpl::mapMouseEvent(int x_, int y_, int width_, int height_,
                                   RenderObject::NodeInfo& info)
{
    bool inside = false;
    if (width_ != lastw || height_ != lasth)
    {
        region=getRegion(width_, height_);
        lastw=width_; lasth=height_;
    }
    if (region.contains(QPoint(x_,y_)))
    {
        inside = true;
        info.setInnerNode(this);
        info.setURLElement(this);
    }

    return inside;
}

QRect HTMLAreaElementImpl::getRect() const
{
    if (parentNode()->renderer()==0)
        return QRect();
    int dx, dy;
    if (!parentNode()->renderer()->absolutePosition(dx, dy))
        return QRect();
    QRegion region = getRegion(lastw,lasth);
    region.translate(dx, dy);
    return region.boundingRect();
}

QRegion HTMLAreaElementImpl::getRegion(int width_, int height_) const
{
    QRegion region;
    if (!m_coords)
        return region;

    // added broken HTML support (Dirk): some pages omit the SHAPE
    // attribute, so we try to guess by looking at the coords count
    // what the HTML author tried to tell us.

    // a Poly needs at least 3 points (6 coords), so this is correct
    if ((shape==Poly || shape==Unknown) && m_coordsLen > 5) {
        // make sure its even
        int len = m_coordsLen >> 1;
        QPointArray points(len);
        for (int i = 0; i < len; ++i)
            points.setPoint(i, m_coords[(i<<1)].minWidth(width_),
                            m_coords[(i<<1)+1].minWidth(height_));
        region = QRegion(points);
    }
    else if (shape==Circle && m_coordsLen>=3 || shape==Unknown && m_coordsLen == 3) {
        int r = kMin(m_coords[2].minWidth(width_), m_coords[2].minWidth(height_));
        region = QRegion(m_coords[0].minWidth(width_)-r,
                         m_coords[1].minWidth(height_)-r, 2*r, 2*r,QRegion::Ellipse);
    }
    else if (shape==Rect && m_coordsLen>=4 || shape==Unknown && m_coordsLen == 4) {
        int x0 = m_coords[0].minWidth(width_);
        int y0 = m_coords[1].minWidth(height_);
        int x1 = m_coords[2].minWidth(width_);
        int y1 = m_coords[3].minWidth(height_);
        region = QRegion(x0,y0,x1-x0,y1-y0);
    }
    else if (shape==Default)
        region = QRegion(0,0,width_,height_);
    // else
       // return null region

    return region;
}

