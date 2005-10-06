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

#include "config.h"
#include "html/html_imageimpl.h"
#include "html/html_formimpl.h"
#include "html/html_documentimpl.h"

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
#include "xml/EventNames.h"

#include <qstring.h>
#include <qpoint.h>
#include <qregion.h>
#include <qptrstack.h>
#include <qimage.h>
#include <qpointarray.h>

using namespace DOM;
using namespace DOM::EventNames;
using namespace HTMLNames;
using namespace khtml;

// #define INSTRUMENT_LAYOUT_SCHEDULING 1

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
    DocumentImpl* document = element()->getDocument();
    if (!document || !document->renderer())
        return;

    AtomicString attr;
    if (element()->hasLocalName(objectTag))
        attr = element()->getAttribute(dataAttr);
    else
        attr = element()->getAttribute(srcAttr);
    
    // Treat a lack of src or empty string for src as no image at all.
    CachedImage* newImage = 0;
    if (!attr.isEmpty())
        newImage = element()->getDocument()->docLoader()->requestImage(khtml::parseURL(attr));

    if (newImage != m_image) {
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
        if (!document->ownerElement() && newImage)
            printf("Image requested at %d\n", element()->getDocument()->elapsedTime());
#endif
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
    if (!m_firedLoad && m_image) {
        m_firedLoad = true;
        if (m_image->isErrorImage())
            element()->dispatchHTMLEvent(errorEvent, false, false);
        else
            element()->dispatchHTMLEvent(loadEvent, false, false);
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
    : HTMLElementImpl(imgTag, doc), m_imageLoader(this), ismap(false), m_form(f)
{
    if (m_form)
        m_form->registerImgElement(this);
}

HTMLImageElementImpl::HTMLImageElementImpl(const QualifiedName& tagName, DocumentPtr *doc)
    : HTMLElementImpl(tagName, doc), m_imageLoader(this), ismap(false), m_form(0)
{
}

HTMLImageElementImpl::~HTMLImageElementImpl()
{
    if (m_form)
        m_form->removeImgElement(this);
}

bool HTMLImageElementImpl::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == widthAttr ||
        attrName == heightAttr ||
        attrName == vspaceAttr ||
        attrName == hspaceAttr ||
        attrName == valignAttr) {
        result = eUniversal;
        return false;
    }
    
    if (attrName == borderAttr ||
        attrName == alignAttr) {
        result = eReplaced; // Shared with embeds and iframes
        return false;
    }

    return HTMLElementImpl::mapToEntry(attrName, result);
}

void HTMLImageElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == altAttr) {
        if (m_render) static_cast<RenderImage*>(m_render)->updateAltText();
    } else if (attr->name() == srcAttr) {
        m_imageLoader.updateFromElement();
    } else if (attr->name() == widthAttr) {
        addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    } else if (attr->name() == heightAttr) {
        addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
    } else if (attr->name() == borderAttr) {
        // border="noborder" -> border="0"
        if(attr->value().toInt()) {
            addCSSLength(attr, CSS_PROP_BORDER_WIDTH, attr->value());
            addCSSProperty(attr, CSS_PROP_BORDER_TOP_STYLE, CSS_VAL_SOLID);
            addCSSProperty(attr, CSS_PROP_BORDER_RIGHT_STYLE, CSS_VAL_SOLID);
            addCSSProperty(attr, CSS_PROP_BORDER_BOTTOM_STYLE, CSS_VAL_SOLID);
            addCSSProperty(attr, CSS_PROP_BORDER_LEFT_STYLE, CSS_VAL_SOLID);
        }
    } else if (attr->name() == vspaceAttr) {
        addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_BOTTOM, attr->value());
    } else if (attr->name() == hspaceAttr) {
        addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_RIGHT, attr->value());
    } else if (attr->name() == alignAttr) {
        addHTMLAlignment(attr);
    } else if (attr->name() == valignAttr) {
        addCSSProperty(attr, CSS_PROP_VERTICAL_ALIGN, attr->value());
    } else if (attr->name() == usemapAttr) {
        if (attr->value().domString()[0] == '#')
            usemap = attr->value();
        else {
            QString url = getDocument()->completeURL(khtml::parseURL(attr->value()).qstring());
            // ### we remove the part before the anchor and hope
            // the map is on the same html page....
            usemap = url;
        }
        m_isLink = !attr->isNull();
    } else if (attr->name() == ismapAttr) {
        ismap = true;
    } else if (attr->name() == onabortAttr) {
        setHTMLEventListener(abortEvent,
                             getDocument()->createHTMLEventListener(attr->value().qstring(), this));
    } else if (attr->name() == onerrorAttr) {
        setHTMLEventListener(errorEvent,
                             getDocument()->createHTMLEventListener(attr->value().qstring(), this));
    } else if (attr->name() == onloadAttr) {
        setHTMLEventListener(loadEvent,
                             getDocument()->createHTMLEventListener(attr->value().qstring(), this));
    }
#if APPLE_CHANGES
    else if (attr->name() == compositeAttr)
        _compositeOperator = attr->value().qstring();
#endif
    else if (attr->name() == nameAttr) {
        DOMString newNameAttr = attr->value();
        if (inDocument() && getDocument()->isHTMLDocument()) {
            HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
            document->removeNamedItem(oldNameAttr);
            document->addNamedItem(newNameAttr);
        }
        oldNameAttr = newNameAttr;
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
}

DOMString HTMLImageElementImpl::altText() const
{
    // lets figure out the alt text.. magic stuff
    // http://www.w3.org/TR/1998/REC-html40-19980424/appendix/notes.html#altgen
    // also heavily discussed by Hixie on bugzilla
    DOMString alt(getAttribute(altAttr));
    // fall back to title attribute
    if (alt.isNull())
        alt = getAttribute(titleAttr);
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
}

void HTMLImageElementImpl::insertedIntoDocument()
{
    if (getDocument()->isHTMLDocument()) {
        HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
        document->addNamedItem(oldNameAttr);
    }

    HTMLElementImpl::insertedIntoDocument();
}

void HTMLImageElementImpl::removedFromDocument()
{
    if (getDocument()->isHTMLDocument()) {
        HTMLDocumentImpl *document = static_cast<HTMLDocumentImpl *>(getDocument());
        document->removeNamedItem(oldNameAttr);
    }

    HTMLElementImpl::removedFromDocument();
}

int HTMLImageElementImpl::width(bool ignorePendingStylesheets) const
{
    if (!m_render) {
	// check the attribute first for an explicit pixel value
	DOM::DOMString attrWidth = getAttribute(widthAttr);
	bool ok;
	int width = attrWidth.qstring().toInt(&ok);
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

int HTMLImageElementImpl::height(bool ignorePendingStylesheets) const
{
    if (!m_render) {
	// check the attribute first for an explicit pixel value
	DOM::DOMString attrHeight = getAttribute(heightAttr);
	bool ok;
	int height = attrHeight.qstring().toInt(&ok);
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
    return (attr->name() == srcAttr || (attr->name() == usemapAttr && attr->value().domString()[0] != '#'));
}

DOMString HTMLImageElementImpl::name() const
{
    return getAttribute(nameAttr);
}

void HTMLImageElementImpl::setName(const DOMString &value)
{
    setAttribute(nameAttr, value);
}

DOMString HTMLImageElementImpl::align() const
{
    return getAttribute(alignAttr);
}

void HTMLImageElementImpl::setAlign(const DOMString &value)
{
    setAttribute(alignAttr, value);
}

DOMString HTMLImageElementImpl::alt() const
{
    return getAttribute(altAttr);
}

void HTMLImageElementImpl::setAlt(const DOMString &value)
{
    setAttribute(altAttr, value);
}

int HTMLImageElementImpl::border() const
{
    // ### return value in pixels
    return getAttribute(borderAttr).toInt();
}

void HTMLImageElementImpl::setBorder(int value)
{
    setAttribute(borderAttr, QString::number(value));
}

void HTMLImageElementImpl::setHeight(int value)
{
    setAttribute(heightAttr, QString::number(value));
}

int HTMLImageElementImpl::hspace() const
{
    // ### return actual value
    return getAttribute(hspaceAttr).toInt();
}

void HTMLImageElementImpl::setHspace(int value)
{
    setAttribute(hspaceAttr, QString::number(value));
}

bool HTMLImageElementImpl::isMap() const
{
    return !getAttribute(ismapAttr).isNull();
}

void HTMLImageElementImpl::setIsMap(bool isMap)
{
    setAttribute(ismapAttr, isMap ? "" : 0);
}

DOMString HTMLImageElementImpl::longDesc() const
{
    return getAttribute(longdescAttr);
}

void HTMLImageElementImpl::setLongDesc(const DOMString &value)
{
    setAttribute(longdescAttr, value);
}

DOMString HTMLImageElementImpl::src() const
{
    return getDocument()->completeURL(getAttribute(srcAttr));
}

void HTMLImageElementImpl::setSrc(const DOMString &value)
{
    setAttribute(srcAttr, value);
}

DOMString HTMLImageElementImpl::useMap() const
{
    return getAttribute(usemapAttr);
}

void HTMLImageElementImpl::setUseMap(const DOMString &value)
{
    setAttribute(usemapAttr, value);
}

int HTMLImageElementImpl::vspace() const
{
    // ### return actual vspace
    return getAttribute(vspaceAttr).toInt();
}

void HTMLImageElementImpl::setVspace(int value)
{
    setAttribute(vspaceAttr, QString::number(value));
}

void HTMLImageElementImpl::setWidth(int value)
{
    setAttribute(widthAttr, QString::number(value));
}

int HTMLImageElementImpl::x() const
{
    RenderObject *r = renderer();
    if (!r)
        return 0;
    int x, y;
    r->absolutePosition(x, y);
    return x;
}

int HTMLImageElementImpl::y() const
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
    : HTMLElementImpl(mapTag, doc)
{
}

HTMLMapElementImpl::~HTMLMapElementImpl()
{
    if (getDocument())
        getDocument()->removeImageMap(this);
}

bool HTMLMapElementImpl::checkDTD(const NodeImpl* newChild)
{
    // FIXME: This seems really odd, allowing only blocks inside map.
    return newChild->hasTagName(areaTag) || newChild->hasTagName(scriptTag) ||
           inBlockTagList(newChild);
}

bool
HTMLMapElementImpl::mapMouseEvent(int x_, int y_, int width_, int height_,
                                  RenderObject::NodeInfo& info)
{
    //cout << "map:mapMouseEvent " << endl;
    //cout << x_ << " " << y_ <<" "<< width_ <<" "<< height_ << endl;
    QPtrStack<NodeImpl> nodeStack;

    NodeImpl *current = firstChild();
    while (1) {
        if (!current) {
            if(nodeStack.isEmpty()) break;
            current = nodeStack.pop();
            current = current->nextSibling();
            continue;
        }
        
        if (current->hasTagName(areaTag)) {
            //cout << "area found " << endl;
            HTMLAreaElementImpl* area = static_cast<HTMLAreaElementImpl*>(current);
            if (area->mapMouseEvent(x_,y_,width_,height_, info))
                return true;
        }
        
        NodeImpl *child = current->firstChild();
        if (child) {
            nodeStack.push(current);
            current = child;
        }
        else
            current = current->nextSibling();
    }

    return false;
}

void HTMLMapElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == idAttr || attr->name() == nameAttr) {
        if (attr->name() == idAttr) {
            // Must call base class so that hasID bit gets set.
            HTMLElementImpl::parseMappedAttribute(attr);
            if (getDocument()->htmlMode() != DocumentImpl::XHtml)
                return;
        }
        getDocument()->removeImageMap(this);
        m_name = attr->value();
        if (m_name.length() != 0 && m_name[0] == '#')
            m_name.remove(0, 1);
        getDocument()->addImageMap(this);
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
}

SharedPtr<HTMLCollectionImpl> HTMLMapElementImpl::areas()
{
    return SharedPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::MAP_AREAS));
}

DOMString HTMLMapElementImpl::name() const
{
    return getAttribute(nameAttr);
}

void HTMLMapElementImpl::setName(const DOMString &value)
{
    setAttribute(nameAttr, value);
}

// -------------------------------------------------------------------------

HTMLAreaElementImpl::HTMLAreaElementImpl(DocumentPtr *doc)
    : HTMLAnchorElementImpl(areaTag, doc)
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

void HTMLAreaElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == shapeAttr) {
        if ( strcasecmp( attr->value(), "default" ) == 0 )
            m_shape = Default;
        else if ( strcasecmp( attr->value(), "circle" ) == 0 )
            m_shape = Circle;
        else if ( strcasecmp( attr->value(), "poly" ) == 0 )
            m_shape = Poly;
        else if ( strcasecmp( attr->value(), "rect" ) == 0 )
            m_shape = Rect;
    } else if (attr->name() == coordsAttr) {
        if (m_coords) delete [] m_coords;
        m_coords = attr->value().toCoordsArray(m_coordsLen);
    } else if (attr->name() == targetAttr) {
        m_hasTarget = !attr->isNull();
    } else if (attr->name() == altAttr ||
               attr->name() == accesskeyAttr) {
        // Do nothing
    } else
        HTMLAnchorElementImpl::parseMappedAttribute(attr);
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
    return getAttribute(accesskeyAttr);
}

void HTMLAreaElementImpl::setAccessKey(const DOMString &value)
{
    setAttribute(accesskeyAttr, value);
}

DOMString HTMLAreaElementImpl::alt() const
{
    return getAttribute(altAttr);
}

void HTMLAreaElementImpl::setAlt(const DOMString &value)
{
    setAttribute(altAttr, value);
}

DOMString HTMLAreaElementImpl::coords() const
{
    return getAttribute(coordsAttr);
}

void HTMLAreaElementImpl::setCoords(const DOMString &value)
{
    setAttribute(coordsAttr, value);
}

DOMString HTMLAreaElementImpl::href() const
{
    return getDocument()->completeURL(getAttribute(hrefAttr));
}

void HTMLAreaElementImpl::setHref(const DOMString &value)
{
    setAttribute(hrefAttr, value);
}

bool HTMLAreaElementImpl::noHref() const
{
    return !getAttribute(nohrefAttr).isNull();
}

void HTMLAreaElementImpl::setNoHref(bool noHref)
{
    setAttribute(nohrefAttr, noHref ? "" : 0);
}

DOMString HTMLAreaElementImpl::shape() const
{
    return getAttribute(shapeAttr);
}

void HTMLAreaElementImpl::setShape(const DOMString &value)
{
    setAttribute(shapeAttr, value);
}

int HTMLAreaElementImpl::tabIndex() const
{
    return getAttribute(tabindexAttr).toInt();
}

void HTMLAreaElementImpl::setTabIndex(int tabIndex)
{
    setAttribute(tabindexAttr, QString::number(tabIndex));
}

DOMString HTMLAreaElementImpl::target() const
{
    return getAttribute(targetAttr);
}

void HTMLAreaElementImpl::setTarget(const DOMString &value)
{
    setAttribute(targetAttr, value);
}
