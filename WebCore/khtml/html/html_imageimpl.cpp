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

#include "html/html_documentimpl.h"
#include "HTMLFormElementImpl.h"

#include <kdebug.h>

#include "rendering/render_image.h"
#include "rendering/render_flow.h"
#include "css/cssstyleselector.h"
#include "cssproperties.h"
#include "cssvalues.h"
#include "css/csshelper.h"
#include "xml/dom2_eventsimpl.h"
#include "xml/EventNames.h"
#include "DocLoader.h"

#include <qstring.h>
#include <qregion.h>
#include "IntPointArray.h"

// #define INSTRUMENT_LAYOUT_SCHEDULING 1

using namespace khtml;

namespace DOM {

using namespace EventNames;
using namespace HTMLNames;

HTMLImageLoader::HTMLImageLoader(ElementImpl* elt)
    : m_element(elt), m_image(0), m_firedLoad(true), m_imageComplete(true)
{
}

HTMLImageLoader::~HTMLImageLoader()
{
    if (m_image)
        m_image->deref(this);
    if (m_element->getDocument())
        m_element->getDocument()->removeImage(this);
}

void HTMLImageLoader::setLoadingImage(CachedImage *loadingImage)
{
    m_firedLoad = false;
    m_imageComplete = false;
    m_image = loadingImage;
}

void HTMLImageLoader::updateFromElement()
{
    // If we're not making renderers for the page, then don't load images.  We don't want to slow
    // down the raw HTML parsing case by loading images we don't intend to display.
    ElementImpl* elem = element();
    DocumentImpl* doc = elem->getDocument();
    if (!doc || !doc->renderer())
        return;

    AtomicString attr = elem->getAttribute(elem->hasLocalName(objectTag) ? dataAttr : srcAttr);
    
    // Treat a lack of src or empty string for src as no image at all.
    CachedImage *newImage = 0;
    if (!attr.isEmpty())
        newImage = doc->docLoader()->requestImage(khtml::parseURL(attr));

    CachedImage *oldImage = m_image;
    if (newImage != oldImage) {
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
        if (!doc->ownerElement() && newImage)
            printf("Image requested at %d\n", doc->elapsedTime());
#endif
        setLoadingImage(newImage);
        if (newImage)
            newImage->ref(this);
        if (oldImage)
            oldImage->deref(this);
    }

    if (RenderImage* renderer = static_cast<RenderImage*>(elem->renderer()))
        renderer->resetAnimation();
}

void HTMLImageLoader::dispatchLoadEvent()
{
    if (!m_firedLoad && m_image) {
        m_firedLoad = true;
        element()->dispatchHTMLEvent(m_image->isErrorImage() ? errorEvent : loadEvent, false, false);
    }
}

void HTMLImageLoader::notifyFinished(CachedObject *image)
{
    m_imageComplete = true;
    ElementImpl* elem = element();
    if (DocumentImpl* doc = elem->getDocument()) {
        doc->dispatchImageLoadEventSoon(this);
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
        if (!doc->ownerElement())
            printf("Image loaded at %d\n", doc->elapsedTime());
#endif
    }
    if (RenderImage* renderer = static_cast<RenderImage*>(elem->renderer()))
        renderer->setImage(m_image);
}

// -------------------------------------------------------------------------

HTMLImageElementImpl::HTMLImageElementImpl(DocumentImpl *doc, HTMLFormElementImpl *f)
    : HTMLElementImpl(imgTag, doc), m_imageLoader(this), ismap(false), m_form(f)
{
    if (f)
        f->registerImgElement(this);
}

HTMLImageElementImpl::HTMLImageElementImpl(const QualifiedName& tagName, DocumentImpl *doc)
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
    
    if (attrName == borderAttr || attrName == alignAttr) {
        result = eReplaced; // Shared with embed and iframe elements.
        return false;
    }

    return HTMLElementImpl::mapToEntry(attrName, result);
}

void HTMLImageElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    const QualifiedName& attrName = attr->name();
    if (attrName == altAttr) {
        if (m_render) static_cast<RenderImage*>(m_render)->updateAltText();
    } else if (attrName == srcAttr) {
        m_imageLoader.updateFromElement();
    } else if (attrName == widthAttr) {
        addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    } else if (attrName == heightAttr) {
        addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
    } else if (attrName == borderAttr) {
        // border="noborder" -> border="0"
        if(attr->value().toInt()) {
            addCSSLength(attr, CSS_PROP_BORDER_WIDTH, attr->value());
            addCSSProperty(attr, CSS_PROP_BORDER_TOP_STYLE, CSS_VAL_SOLID);
            addCSSProperty(attr, CSS_PROP_BORDER_RIGHT_STYLE, CSS_VAL_SOLID);
            addCSSProperty(attr, CSS_PROP_BORDER_BOTTOM_STYLE, CSS_VAL_SOLID);
            addCSSProperty(attr, CSS_PROP_BORDER_LEFT_STYLE, CSS_VAL_SOLID);
        }
    } else if (attrName == vspaceAttr) {
        addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_BOTTOM, attr->value());
    } else if (attrName == hspaceAttr) {
        addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value());
        addCSSLength(attr, CSS_PROP_MARGIN_RIGHT, attr->value());
    } else if (attrName == alignAttr) {
        addHTMLAlignment(attr);
    } else if (attrName == valignAttr) {
        addCSSProperty(attr, CSS_PROP_VERTICAL_ALIGN, attr->value());
    } else if (attrName == usemapAttr) {
        if (attr->value().domString()[0] == '#')
            usemap = attr->value();
        else
            usemap = getDocument()->completeURL(khtml::parseURL(attr->value()));
        m_isLink = !attr->isNull();
    } else if (attrName == ismapAttr) {
        ismap = true;
    } else if (attrName == onabortAttr) {
        setHTMLEventListener(abortEvent, attr);
    } else if (attrName == onerrorAttr) {
        setHTMLEventListener(errorEvent, attr);
    } else if (attrName == onloadAttr) {
        setHTMLEventListener(loadEvent, attr);
    } else if (attrName == compositeAttr) {
        _compositeOperator = attr->value().qstring();
    } else if (attrName == nameAttr) {
        DOMString newNameAttr = attr->value();
        if (inDocument() && getDocument()->isHTMLDocument()) {
            HTMLDocumentImpl* doc = static_cast<HTMLDocumentImpl*>(getDocument());
            doc->removeNamedItem(oldNameAttr);
            doc->addNamedItem(newNameAttr);
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
    DOMString alt = getAttribute(altAttr);
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

    if (RenderImage* imageObj = static_cast<RenderImage*>(renderer()))
        imageObj->setImage(m_imageLoader.image());
}

void HTMLImageElementImpl::insertedIntoDocument()
{
    DocumentImpl* doc = getDocument();
    if (doc->isHTMLDocument())
        static_cast<HTMLDocumentImpl*>(doc)->addNamedItem(oldNameAttr);

    HTMLElementImpl::insertedIntoDocument();
}

void HTMLImageElementImpl::removedFromDocument()
{
    DocumentImpl* doc = getDocument();
    if (doc->isHTMLDocument())
        static_cast<HTMLDocumentImpl*>(doc)->removeNamedItem(oldNameAttr);

    HTMLElementImpl::removedFromDocument();
}

int HTMLImageElementImpl::width(bool ignorePendingStylesheets) const
{
    if (!m_render) {
        // check the attribute first for an explicit pixel value
        bool ok;
        int width = getAttribute(widthAttr).qstring().toInt(&ok);
        if (ok)
            return width;
    }

    if (DocumentImpl* doc = getDocument()) {
        if (ignorePendingStylesheets)
            doc->updateLayoutIgnorePendingStylesheets();
        else
            doc->updateLayout();
    }

    return m_render ? m_render->contentWidth() : 0;
}

int HTMLImageElementImpl::height(bool ignorePendingStylesheets) const
{
    if (!m_render) {
        // check the attribute first for an explicit pixel value
        bool ok;
        int height = getAttribute(heightAttr).qstring().toInt(&ok);
        if (ok)
            return height;
    }

    if (DocumentImpl* doc = getDocument()) {
        if (ignorePendingStylesheets)
            doc->updateLayoutIgnorePendingStylesheets();
        else
            doc->updateLayout();
    }

    return m_render ? m_render->contentHeight() : 0;
}

bool HTMLImageElementImpl::isURLAttribute(AttributeImpl *attr) const
{
    return attr->name() == srcAttr || (attr->name() == usemapAttr && attr->value().domString()[0] != '#');
}

DOMString HTMLImageElementImpl::name() const
{
    return getAttribute(nameAttr);
}

void HTMLImageElementImpl::setName(const DOMString& value)
{
    setAttribute(nameAttr, value);
}

DOMString HTMLImageElementImpl::align() const
{
    return getAttribute(alignAttr);
}

void HTMLImageElementImpl::setAlign(const DOMString& value)
{
    setAttribute(alignAttr, value);
}

DOMString HTMLImageElementImpl::alt() const
{
    return getAttribute(altAttr);
}

void HTMLImageElementImpl::setAlt(const DOMString& value)
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

void HTMLImageElementImpl::setLongDesc(const DOMString& value)
{
    setAttribute(longdescAttr, value);
}

DOMString HTMLImageElementImpl::src() const
{
    return getDocument()->completeURL(getAttribute(srcAttr));
}

void HTMLImageElementImpl::setSrc(const DOMString& value)
{
    setAttribute(srcAttr, value);
}

DOMString HTMLImageElementImpl::useMap() const
{
    return getAttribute(usemapAttr);
}

void HTMLImageElementImpl::setUseMap(const DOMString& value)
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

HTMLMapElementImpl::HTMLMapElementImpl(DocumentImpl *doc)
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
    // FIXME: This seems really odd, allowing only blocks inside map elements.
    return newChild->hasTagName(areaTag) || newChild->hasTagName(scriptTag) || inBlockTagList(newChild);
}

bool HTMLMapElementImpl::mapMouseEvent(int x, int y, int width, int height, RenderObject::NodeInfo& info)
{
    NodeImpl *node = this;
    while ((node = node->traverseNextNode(this)))
        if (node->hasTagName(areaTag))
            if (static_cast<HTMLAreaElementImpl *>(node)->mapMouseEvent(x, y, width, height, info))
                return true;
    return false;
}

void HTMLMapElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    const QualifiedName& attrName = attr->name();
    if (attrName == idAttr || attrName == nameAttr) {
        DocumentImpl* doc = getDocument();
        if (attrName == idAttr) {
            // Call base class so that hasID bit gets set.
            HTMLElementImpl::parseMappedAttribute(attr);
            if (doc->htmlMode() != DocumentImpl::XHtml)
                return;
        }
        doc->removeImageMap(this);
        DOMString mapName = attr->value();
        if (mapName[0] == '#') {
            mapName = mapName.copy();
            mapName.remove(0, 1);
        }
        m_name = mapName;
        doc->addImageMap(this);
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
}

RefPtr<HTMLCollectionImpl> HTMLMapElementImpl::areas()
{
    return RefPtr<HTMLCollectionImpl>(new HTMLCollectionImpl(this, HTMLCollectionImpl::MAP_AREAS));
}

DOMString HTMLMapElementImpl::name() const
{
    return getAttribute(nameAttr);
}

void HTMLMapElementImpl::setName(const DOMString& value)
{
    setAttribute(nameAttr, value);
}

// -------------------------------------------------------------------------

HTMLAreaElementImpl::HTMLAreaElementImpl(DocumentImpl *doc)
    : HTMLAnchorElementImpl(areaTag, doc)
{
    m_coords = 0;
    m_coordsLen = 0;
    m_shape = Unknown;
    lasth = -1;
    lastw = -1;
}

HTMLAreaElementImpl::~HTMLAreaElementImpl()
{
    delete [] m_coords;
}

void HTMLAreaElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == shapeAttr) {
        if (equalIgnoringCase(attr->value(), "default"))
            m_shape = Default;
        else if (equalIgnoringCase(attr->value(), "circle"))
            m_shape = Circle;
        else if (equalIgnoringCase(attr->value(), "poly"))
            m_shape = Poly;
        else if (equalIgnoringCase(attr->value(), "rect"))
            m_shape = Rect;
    } else if (attr->name() == coordsAttr) {
        delete [] m_coords;
        m_coords = attr->value().toCoordsArray(m_coordsLen);
    } else if (attr->name() == targetAttr) {
        m_hasTarget = !attr->isNull();
    } else if (attr->name() == altAttr || attr->name() == accesskeyAttr) {
        // Do nothing.
    } else
        HTMLAnchorElementImpl::parseMappedAttribute(attr);
}

bool HTMLAreaElementImpl::mapMouseEvent(int x, int y, int width, int height, RenderObject::NodeInfo& info)
{
    if (width != lastw || height != lasth) {
        region = getRegion(width, height);
        lastw = width;
        lasth = height;
    }

    if (!region.contains(IntPoint(x, y)))
        return false;
    
    info.setInnerNode(this);
    info.setURLElement(this);
    return true;
}

IntRect HTMLAreaElementImpl::getRect(RenderObject* obj) const
{
    int dx, dy;
    obj->absolutePosition(dx, dy);
    QRegion region = getRegion(lastw,lasth);
    region.translate(dx, dy);
    return region.boundingRect();
}

QRegion HTMLAreaElementImpl::getRegion(int width, int height) const
{
    if (!m_coords)
        return QRegion();

    // If element omits the shape attribute, select shape based on number of coordinates.
    Shape shape = m_shape;
    if (shape == Unknown) {
        if (m_coordsLen == 3)
            shape = Circle;
        else if (m_coordsLen == 4)
            shape = Rect;
        else if (m_coordsLen >= 6)
            shape = Poly;
    }

    switch (shape) {
        case Poly:
            if (m_coordsLen >= 6) {
                int numPoints = m_coordsLen / 2;
                IntPointArray points(numPoints);
                for (int i = 0; i < numPoints; ++i)
                    points.setPoint(i, m_coords[i * 2].minWidth(width), m_coords[i * 2 + 1].minWidth(height));
                return QRegion(points);
            }
            break;
        case Circle:
            if (m_coordsLen >= 3) {
                Length radius = m_coords[2];
                int r = kMin(radius.minWidth(width), radius.minWidth(height));
                return QRegion(m_coords[0].minWidth(width) - r, m_coords[1].minWidth(height) - r,
                    2 * r, 2 * r, QRegion::Ellipse);
            }
            break;
        case Rect:
            if (m_coordsLen >= 4) {
                int x0 = m_coords[0].minWidth(width);
                int y0 = m_coords[1].minWidth(height);
                int x1 = m_coords[2].minWidth(width);
                int y1 = m_coords[3].minWidth(height);
                return QRegion(x0, y0, x1 - x0, y1 - y0);
            }
            break;
        case Default:
            return QRegion(0, 0, width, height);
        case Unknown:
            break;
    }

    return QRegion();
}

DOMString HTMLAreaElementImpl::accessKey() const
{
    return getAttribute(accesskeyAttr);
}

void HTMLAreaElementImpl::setAccessKey(const DOMString& value)
{
    setAttribute(accesskeyAttr, value);
}

DOMString HTMLAreaElementImpl::alt() const
{
    return getAttribute(altAttr);
}

void HTMLAreaElementImpl::setAlt(const DOMString& value)
{
    setAttribute(altAttr, value);
}

DOMString HTMLAreaElementImpl::coords() const
{
    return getAttribute(coordsAttr);
}

void HTMLAreaElementImpl::setCoords(const DOMString& value)
{
    setAttribute(coordsAttr, value);
}

DOMString HTMLAreaElementImpl::href() const
{
    return getDocument()->completeURL(getAttribute(hrefAttr));
}

void HTMLAreaElementImpl::setHref(const DOMString& value)
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

void HTMLAreaElementImpl::setShape(const DOMString& value)
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

void HTMLAreaElementImpl::setTarget(const DOMString& value)
{
    setAttribute(targetAttr, value);
}

}
