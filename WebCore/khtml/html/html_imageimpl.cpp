/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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
#include "html/html_formimpl.h"
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

//#define INSTRUMENT_LAYOUT_SCHEDULING 1

HTMLImageLoader::HTMLImageLoader(ElementImpl* elt)
:m_element(elt), m_image(0), m_firedLoad(true), m_imageComplete(true)
{
}

HTMLImageLoader::~HTMLImageLoader()
{
    if (m_image)
        m_image->deref(this);
    if (m_element->getDocument())
        m_element->getDocument()->removeImage(this);
}

void HTMLImageLoader::updateFromElement()
{
    // If we're not making renderers for the page, then don't load images.  We don't want to slow
    // down the raw HTML parsing case by loading images we don't intend to display.
    if (!element()->getDocument()->renderer())
        return;

    AtomicString attr;
    if (element()->id() == ID_OBJECT)
        attr = element()->getAttribute(ATTR_DATA);
    else
        attr = element()->getAttribute(ATTR_SRC);
    
    // Treat a lack of src or empty string for src as no image at all.
    CachedImage* newImage = 0;
    if (!attr.isEmpty())
        newImage = element()->getDocument()->docLoader()->requestImage(khtml::parseURL(attr));

    if (newImage != m_image) {
        m_firedLoad = false;
        m_imageComplete = false;
        CachedImage* oldImage = m_image;
        m_image = newImage;
        if (m_image)
            m_image->ref(this);
        if (oldImage)
            oldImage->deref(this);
    }
#if APPLE_CHANGES
    khtml::RenderImage *renderer = static_cast<khtml::RenderImage*>(element()->renderer());
    if (renderer)
        renderer->resetAnimation();
#endif
}

void HTMLImageLoader::dispatchLoadEvent()
{
    if (!m_firedLoad) {
        m_firedLoad = true;
        if (m_image->isErrorImage())
            element()->dispatchHTMLEvent(EventImpl::ERROR_EVENT, false, false);
        else
            element()->dispatchHTMLEvent(EventImpl::LOAD_EVENT, false, false);
    }
}

void HTMLImageLoader::notifyFinished(CachedObject* image)
{
    m_imageComplete = true;
    DocumentImpl* document = element()->getDocument();
    if (document) {
        document->dispatchImageLoadEventSoon(this);
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
        if (!document->ownerElement())
            printf("Image loaded at %d\n", element()->getDocument()->elapsedTime());
#endif
    }
    if (element()->renderer()) {
        RenderImage* imageObj = static_cast<RenderImage*>(element()->renderer());
        imageObj->setImage(m_image);
    }
}

// -------------------------------------------------------------------------

HTMLImageElementImpl::HTMLImageElementImpl(DocumentPtr *doc, HTMLFormElementImpl *f)
    : HTMLElementImpl(doc), m_imageLoader(this), ismap(false), m_form(f)
{
    if (m_form)
        m_form->registerImgElement(this);
}

HTMLImageElementImpl::~HTMLImageElementImpl()
{
    if (m_form)
        m_form->removeImgElement(this);
}

NodeImpl::Id HTMLImageElementImpl::id() const
{
    return ID_IMG;
}

bool HTMLImageElementImpl::mapToEntry(NodeImpl::Id attr, MappedAttributeEntry& result) const
{
    switch(attr)
    {
        case ATTR_WIDTH:
        case ATTR_HEIGHT:
        case ATTR_VSPACE:
        case ATTR_HSPACE:
        case ATTR_VALIGN:
            result = eUniversal;
            return false;
        case ATTR_BORDER:
        case ATTR_ALIGN:
            result = eReplaced; // Shared with embeds and iframes
            return false;
        default:
            break;
    }

    return HTMLElementImpl::mapToEntry(attr, result);
}

void HTMLImageElementImpl::parseHTMLAttribute(HTMLAttributeImpl *attr)
{
    switch (attr->id())
    {
    case ATTR_ALT:
        if (m_render) static_cast<RenderImage*>(m_render)->updateAltText();
        break;
    case ATTR_SRC:
        m_imageLoader.updateFromElement();
        break;
    case ATTR_WIDTH:
        addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
        break;
    case ATTR_HEIGHT:
        addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
        break;
    case ATTR_BORDER:
        // border="noborder" -> border="0"
        if(attr->value().toInt()) {
            addCSSLength(attr, CSS_PROP_BORDER_WIDTH, attr->value());
            addCSSProperty(attr, CSS_PROP_BORDER_TOP_STYLE, CSS_VAL_SOLID);
            addCSSProperty(attr, CSS_PROP_BORDER_RIGHT_STYLE, CSS_VAL_SOLID);
            addCSSProperty(attr, CSS_PROP_BORDER_BOTTOM_STYLE, CSS_VAL_SOLID);
            addCSSProperty(attr, CSS_PROP_BORDER_LEFT_STYLE, CSS_VAL_SOLID);
        }
        break;
    case ATTR_VSPACE:
        addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_BOTTOM, attr->value());
        break;
    case ATTR_HSPACE:
        addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_RIGHT, attr->value());
        break;
    case ATTR_ALIGN:
        addHTMLAlignment(attr);
	break;
    case ATTR_VALIGN:
        addCSSProperty(attr, CSS_PROP_VERTICAL_ALIGN, attr->value());
        break;
    case ATTR_USEMAP:
        if ( attr->value().domString()[0] == '#' )
            usemap = attr->value();
        else {
            QString url = getDocument()->completeURL( khtml::parseURL( attr->value() ).string() );
            // ### we remove the part before the anchor and hope
            // the map is on the same html page....
            usemap = url;
        }
        m_hasAnchor = !attr->isNull();
    case ATTR_ISMAP:
        ismap = true;
        break;
    case ATTR_ONABORT: // ### add support for this
        setHTMLEventListener(EventImpl::ABORT_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONERROR:
        setHTMLEventListener(EventImpl::ERROR_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_ONLOAD:
        setHTMLEventListener(EventImpl::LOAD_EVENT,
	    getDocument()->createHTMLEventListener(attr->value().string(), this));
        break;
    case ATTR_NOSAVE:
	break;
#if APPLE_CHANGES
    case ATTR_COMPOSITE:
        _compositeOperator = attr->value().string();
        break;
#endif
    case ATTR_NAME:
	{
	    QString newNameAttr = attr->value().string();
	    if (attached() && getDocument()->isHTMLDocument()) {
		HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
		document->removeNamedImageOrForm(oldNameAttr);
		document->addNamedImageOrForm(newNameAttr);
	    }
	    oldNameAttr = newNameAttr;
	}
	break;
    case ATTR_ID:
	{
	    QString newIdAttr = attr->value().string();
	    if (attached() && getDocument()->isHTMLDocument()) {
		HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
		document->removeNamedImageOrForm(oldIdAttr);
		document->addNamedImageOrForm(newIdAttr);
	    }
	    oldIdAttr = newIdAttr;
	}
	// fall through
    default:
        HTMLElementImpl::parseHTMLAttribute(attr);
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

RenderObject *HTMLImageElementImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
     return new (arena) RenderImage(this);
}

void HTMLImageElementImpl::attach()
{
    HTMLElementImpl::attach();

    if (renderer()) {
        RenderImage* imageObj = static_cast<RenderImage*>(renderer());
        imageObj->setImage(m_imageLoader.image());
    }

    if (getDocument()->isHTMLDocument()) {
        HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
        document->addNamedImageOrForm(oldIdAttr);
        document->addNamedImageOrForm(oldNameAttr);
    }
}

void HTMLImageElementImpl::detach()
{
    if (getDocument()->isHTMLDocument()) {
        HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
        document->removeNamedImageOrForm(oldIdAttr);
        document->removeNamedImageOrForm(oldNameAttr);
    }

    HTMLElementImpl::detach();
}

long HTMLImageElementImpl::width(bool ignorePendingStylesheets) const
{
    if (!m_render) {
	// check the attribute first for an explicit pixel value
	DOM::DOMString attrWidth = getAttribute(ATTR_WIDTH);
	bool ok;
	long width = attrWidth.string().toLong(&ok);
	if (ok) {
	  return width;
	}
    }

    DOM::DocumentImpl* docimpl = getDocument();
    if (docimpl) {
	if (ignorePendingStylesheets)
            docimpl->updateLayoutIgnorePendingStylesheets();
        else
            docimpl->updateLayout();
    }

    if (!m_render) {
	return 0;
    }

    return m_render->contentWidth();
}

long HTMLImageElementImpl::height(bool ignorePendingStylesheets) const
{
    if (!m_render) {
	// check the attribute first for an explicit pixel value
	DOM::DOMString attrHeight = getAttribute(ATTR_HEIGHT);
	bool ok;
	long height = attrHeight.string().toLong(&ok);
	if (ok) {
	  return height;
	}
    }

    DOM::DocumentImpl* docimpl = getDocument();
    if (docimpl) {
	if (ignorePendingStylesheets)
            docimpl->updateLayoutIgnorePendingStylesheets();
        else
            docimpl->updateLayout();
    }

    if (!m_render) {
	return 0;
    }

    return m_render->contentHeight();
}

QImage HTMLImageElementImpl::currentImage() const
{
    RenderImage *r = static_cast<RenderImage*>(renderer());
    if (r)
        return r->pixmap().convertToImage();
    return QImage();
}

bool HTMLImageElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return (attr->id() == ATTR_SRC || (attr->id() == ATTR_USEMAP && attr->value().domString()[0] != '#'));
}

DOMString HTMLImageElementImpl::name() const
{
    return getAttribute(ATTR_NAME);
}

void HTMLImageElementImpl::setName(const DOMString &value)
{
    setAttribute(ATTR_NAME, value);
}

DOMString HTMLImageElementImpl::align() const
{
    return getAttribute(ATTR_ALIGN);
}

void HTMLImageElementImpl::setAlign(const DOMString &value)
{
    setAttribute(ATTR_ALIGN, value);
}

DOMString HTMLImageElementImpl::alt() const
{
    return getAttribute(ATTR_ALT);
}

void HTMLImageElementImpl::setAlt(const DOMString &value)
{
    setAttribute(ATTR_ALT, value);
}

long HTMLImageElementImpl::border() const
{
    // ### return value in pixels
    return getAttribute(ATTR_BORDER).toInt();
}

void HTMLImageElementImpl::setBorder(long value)
{
    setAttribute(ATTR_BORDER, QString::number(value));
}

void HTMLImageElementImpl::setHeight(long value)
{
    setAttribute(ATTR_HEIGHT, QString::number(value));
}

long HTMLImageElementImpl::hspace() const
{
    // ### return actual value
    return getAttribute(ATTR_HSPACE).toInt();
}

void HTMLImageElementImpl::setHspace(long value)
{
    setAttribute(ATTR_HSPACE, QString::number(value));
}

bool HTMLImageElementImpl::isMap() const
{
    return !getAttribute(ATTR_ISMAP).isNull();
}

void HTMLImageElementImpl::setIsMap(bool isMap)
{
    setAttribute(ATTR_ISMAP, isMap ? "" : 0);
}

DOMString HTMLImageElementImpl::longDesc() const
{
    return getAttribute(ATTR_LONGDESC);
}

void HTMLImageElementImpl::setLongDesc(const DOMString &value)
{
    setAttribute(ATTR_LONGDESC, value);
}

DOMString HTMLImageElementImpl::src() const
{
    return getDocument()->completeURL(getAttribute(ATTR_SRC));
}

void HTMLImageElementImpl::setSrc(const DOMString &value)
{
    setAttribute(ATTR_SRC, value);
}

DOMString HTMLImageElementImpl::useMap() const
{
    return getAttribute(ATTR_USEMAP);
}

void HTMLImageElementImpl::setUseMap(const DOMString &value)
{
    setAttribute(ATTR_USEMAP, value);
}

long HTMLImageElementImpl::vspace() const
{
    // ### return actual vspace
    return getAttribute(ATTR_VSPACE).toInt();
}

void HTMLImageElementImpl::setVspace(long value)
{
    setAttribute(ATTR_VSPACE, QString::number(value));
}

void HTMLImageElementImpl::setWidth(long value)
{
    setAttribute(ATTR_WIDTH, QString::number(value));
}

long HTMLImageElementImpl::x() const
{
    RenderObject *r = renderer();
    if (!r)
        return 0;
    int x, y;
    r->absolutePosition(x, y);
    return x;
}

long HTMLImageElementImpl::y() const
{
    RenderObject *r = renderer();
    if (!r)
        return 0;
    int x, y;
    r->absolutePosition(x, y);
    return y;
}

// -------------------------------------------------------------------------

HTMLMapElementImpl::HTMLMapElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLMapElementImpl::~HTMLMapElementImpl()
{
    if (getDocument())
        getDocument()->removeImageMap(this);
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

void HTMLMapElementImpl::parseHTMLAttribute(HTMLAttributeImpl *attr)
{
    switch (attr->id())
    {
    case ATTR_ID:
        // Must call base class so that hasID bit gets set.
        HTMLElementImpl::parseHTMLAttribute(attr);
        if (getDocument()->htmlMode() != DocumentImpl::XHtml) break;
        // fall through
    case ATTR_NAME:
        getDocument()->removeImageMap(this);
        m_name = attr->value();
        if (m_name.length() != 0 && m_name[0] == '#')
            m_name.remove(0, 1);
        getDocument()->addImageMap(this);
        break;
    default:
        HTMLElementImpl::parseHTMLAttribute(attr);
    }
}

SharedPtr<HTMLCollectionImpl> HTMLMapElementImpl::areas()
{
    return SharedPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::MAP_AREAS));
}

DOMString HTMLMapElementImpl::name() const
{
    return getAttribute(ATTR_NAME);
}

void HTMLMapElementImpl::setName(const DOMString &value)
{
    setAttribute(ATTR_NAME, value);
}

// -------------------------------------------------------------------------

HTMLAreaElementImpl::HTMLAreaElementImpl(DocumentPtr *doc)
    : HTMLAnchorElementImpl(doc)
{
    m_coords=0;
    m_coordsLen = 0;
    m_shape = Unknown;
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

void HTMLAreaElementImpl::parseHTMLAttribute(HTMLAttributeImpl *attr)
{
    switch (attr->id())
    {
    case ATTR_SHAPE:
        if ( strcasecmp( attr->value(), "default" ) == 0 )
            m_shape = Default;
        else if ( strcasecmp( attr->value(), "circle" ) == 0 )
            m_shape = Circle;
        else if ( strcasecmp( attr->value(), "poly" ) == 0 )
            m_shape = Poly;
        else if ( strcasecmp( attr->value(), "rect" ) == 0 )
            m_shape = Rect;
        break;
    case ATTR_COORDS:
        if (m_coords) delete [] m_coords;
        m_coords = attr->value().toLengthArray(m_coordsLen);
        break;
    case ATTR_TARGET:
        m_hasTarget = !attr->isNull();
        break;
    case ATTR_ALT:
        break;
    case ATTR_ACCESSKEY:
        break;
    default:
        HTMLAnchorElementImpl::parseHTMLAttribute(attr);
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

QRect HTMLAreaElementImpl::getRect(RenderObject* obj) const
{
    int dx, dy;
    obj->absolutePosition(dx, dy);
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
    if ((m_shape==Poly || m_shape==Unknown) && m_coordsLen > 5) {
        // make sure its even
        int len = m_coordsLen >> 1;
        QPointArray points(len);
        for (int i = 0; i < len; ++i)
            points.setPoint(i, m_coords[(i<<1)].minWidth(width_),
                            m_coords[(i<<1)+1].minWidth(height_));
        region = QRegion(points);
    }
    else if (m_shape==Circle && m_coordsLen>=3 || m_shape==Unknown && m_coordsLen == 3) {
        int r = kMin(m_coords[2].minWidth(width_), m_coords[2].minWidth(height_));
        region = QRegion(m_coords[0].minWidth(width_)-r,
                         m_coords[1].minWidth(height_)-r, 2*r, 2*r,QRegion::Ellipse);
    }
    else if (m_shape==Rect && m_coordsLen>=4 || m_shape==Unknown && m_coordsLen == 4) {
        int x0 = m_coords[0].minWidth(width_);
        int y0 = m_coords[1].minWidth(height_);
        int x1 = m_coords[2].minWidth(width_);
        int y1 = m_coords[3].minWidth(height_);
        region = QRegion(x0,y0,x1-x0,y1-y0);
    }
    else if (m_shape==Default)
        region = QRegion(0,0,width_,height_);
    // else
       // return null region

    return region;
}

DOMString HTMLAreaElementImpl::accessKey() const
{
    return getAttribute(ATTR_ACCESSKEY);
}

void HTMLAreaElementImpl::setAccessKey(const DOMString &value)
{
    setAttribute(ATTR_ACCESSKEY, value);
}

DOMString HTMLAreaElementImpl::alt() const
{
    return getAttribute(ATTR_ALT);
}

void HTMLAreaElementImpl::setAlt(const DOMString &value)
{
    setAttribute(ATTR_ALT, value);
}

DOMString HTMLAreaElementImpl::coords() const
{
    return getAttribute(ATTR_COORDS);
}

void HTMLAreaElementImpl::setCoords(const DOMString &value)
{
    setAttribute(ATTR_COORDS, value);
}

DOMString HTMLAreaElementImpl::href() const
{
    return getDocument()->completeURL(getAttribute(ATTR_HREF));
}

void HTMLAreaElementImpl::setHref(const DOMString &value)
{
    setAttribute(ATTR_HREF, value);
}

bool HTMLAreaElementImpl::noHref() const
{
    return !getAttribute(ATTR_NOHREF).isNull();
}

void HTMLAreaElementImpl::setNoHref(bool noHref)
{
    setAttribute(ATTR_NOHREF, noHref ? "" : 0);
}

DOMString HTMLAreaElementImpl::shape() const
{
    return getAttribute(ATTR_SHAPE);
}

void HTMLAreaElementImpl::setShape(const DOMString &value)
{
    setAttribute(ATTR_SHAPE, value);
}

long HTMLAreaElementImpl::tabIndex() const
{
    return getAttribute(ATTR_TABINDEX).toInt();
}

void HTMLAreaElementImpl::setTabIndex(long tabIndex)
{
    setAttribute(ATTR_TABINDEX, QString::number(tabIndex));
}

DOMString HTMLAreaElementImpl::target() const
{
    return getAttribute(ATTR_TARGET);
}

void HTMLAreaElementImpl::setTarget(const DOMString &value)
{
    setAttribute(ATTR_TARGET, value);
}
