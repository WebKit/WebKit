/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
#include "html_imageimpl.h"

#include "DocLoader.h"
#include "EventNames.h"
#include "HTMLFormElement.h"
#include "IntPointArray.h"
#include "csshelper.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "HTMLDocument.h"
#include "RenderImage.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

HTMLImageLoader::HTMLImageLoader(Element* elt)
    : m_element(elt), m_image(0), m_firedLoad(true), m_imageComplete(true)
{
}

HTMLImageLoader::~HTMLImageLoader()
{
    if (m_image)
        m_image->deref(this);
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
    Element* elem = element();
    Document* doc = elem->getDocument();
    if (!doc->renderer())
        return;

    AtomicString attr = elem->getAttribute(elem->hasLocalName(objectTag) ? dataAttr : srcAttr);
    
    // Treat a lack of src or empty string for src as no image at all.
    CachedImage *newImage = 0;
    if (!attr.isEmpty())
        newImage = doc->docLoader()->requestImage(parseURL(attr));

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
    Element* elem = element();
    Document* doc = elem->getDocument();
    doc->dispatchImageLoadEventSoon(this);
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
        if (!doc->ownerElement())
            printf("Image loaded at %d\n", doc->elapsedTime());
#endif
    if (RenderImage* renderer = static_cast<RenderImage*>(elem->renderer()))
        renderer->setCachedImage(m_image);
}

// -------------------------------------------------------------------------

HTMLImageElement::HTMLImageElement(Document *doc, HTMLFormElement *f)
    : HTMLElement(imgTag, doc), m_imageLoader(this), ismap(false), m_form(f)
{
    if (f)
        f->registerImgElement(this);
}

HTMLImageElement::HTMLImageElement(const QualifiedName& tagName, Document *doc)
    : HTMLElement(tagName, doc), m_imageLoader(this), ismap(false), m_form(0)
{
}

HTMLImageElement::~HTMLImageElement()
{
    if (m_form)
        m_form->removeImgElement(this);
}

bool HTMLImageElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
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

    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLImageElement::parseMappedAttribute(MappedAttribute *attr)
{
    const QualifiedName& attrName = attr->name();
    if (attrName == altAttr) {
        if (renderer())
            static_cast<RenderImage*>(renderer())->updateAltText();
    } else if (attrName == srcAttr)
        m_imageLoader.updateFromElement();
    else if (attrName == widthAttr)
        addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    else if (attrName == heightAttr)
        addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
    else if (attrName == borderAttr) {
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
    } else if (attrName == alignAttr)
        addHTMLAlignment(attr);
    else if (attrName == valignAttr)
        addCSSProperty(attr, CSS_PROP_VERTICAL_ALIGN, attr->value());
    else if (attrName == usemapAttr) {
        if (attr->value().domString()[0] == '#')
            usemap = attr->value();
        else
            usemap = getDocument()->completeURL(parseURL(attr->value()));
        m_isLink = !attr->isNull();
    } else if (attrName == ismapAttr)
        ismap = true;
    else if (attrName == onabortAttr)
        setHTMLEventListener(abortEvent, attr);
    else if (attrName == onerrorAttr)
        setHTMLEventListener(errorEvent, attr);
    else if (attrName == onloadAttr)
        setHTMLEventListener(loadEvent, attr);
    else if (attrName == compositeAttr)
        _compositeOperator = attr->value().domString();
    else if (attrName == nameAttr) {
        String newNameAttr = attr->value();
        if (inDocument() && getDocument()->isHTMLDocument()) {
            HTMLDocument* doc = static_cast<HTMLDocument*>(getDocument());
            doc->removeNamedItem(oldNameAttr);
            doc->addNamedItem(newNameAttr);
        }
        oldNameAttr = newNameAttr;
    } else
        HTMLElement::parseMappedAttribute(attr);
}

String HTMLImageElement::altText() const
{
    // lets figure out the alt text.. magic stuff
    // http://www.w3.org/TR/1998/REC-html40-19980424/appendix/notes.html#altgen
    // also heavily discussed by Hixie on bugzilla
    String alt = getAttribute(altAttr);
    // fall back to title attribute
    if (alt.isNull())
        alt = getAttribute(titleAttr);
    return alt;
}

RenderObject *HTMLImageElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
     return new (arena) RenderImage(this);
}

void HTMLImageElement::attach()
{
    HTMLElement::attach();

    if (RenderImage* imageObj = static_cast<RenderImage*>(renderer()))
        imageObj->setCachedImage(m_imageLoader.image());
}

void HTMLImageElement::insertedIntoDocument()
{
    Document* doc = getDocument();
    if (doc->isHTMLDocument())
        static_cast<HTMLDocument*>(doc)->addNamedItem(oldNameAttr);

    HTMLElement::insertedIntoDocument();
}

void HTMLImageElement::removedFromDocument()
{
    Document* doc = getDocument();
    if (doc->isHTMLDocument())
        static_cast<HTMLDocument*>(doc)->removeNamedItem(oldNameAttr);

    HTMLElement::removedFromDocument();
}

int HTMLImageElement::width(bool ignorePendingStylesheets) const
{
    if (!renderer()) {
        // check the attribute first for an explicit pixel value
        bool ok;
        int width = getAttribute(widthAttr).toInt(&ok);
        if (ok)
            return width;
        
        // if the image is available, use its width
        if (m_imageLoader.image())
            return m_imageLoader.image()->imageSize().width();
    }

    Document* doc = getDocument();
    if (ignorePendingStylesheets)
        doc->updateLayoutIgnorePendingStylesheets();
    else
        doc->updateLayout();

    return renderer() ? renderer()->contentWidth() : 0;
}

int HTMLImageElement::height(bool ignorePendingStylesheets) const
{
    if (!renderer()) {
        // check the attribute first for an explicit pixel value
        bool ok;
        int height = getAttribute(heightAttr).toInt(&ok);
        if (ok)
            return height;
        
        // if the image is available, use its height
        if (m_imageLoader.image())
            return m_imageLoader.image()->imageSize().height();        
    }

    Document* doc = getDocument();
    if (ignorePendingStylesheets)
        doc->updateLayoutIgnorePendingStylesheets();
    else
        doc->updateLayout();

    return renderer() ? renderer()->contentHeight() : 0;
}

bool HTMLImageElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == srcAttr || (attr->name() == usemapAttr && attr->value().domString()[0] != '#');
}

String HTMLImageElement::name() const
{
    return getAttribute(nameAttr);
}

void HTMLImageElement::setName(const String& value)
{
    setAttribute(nameAttr, value);
}

String HTMLImageElement::align() const
{
    return getAttribute(alignAttr);
}

void HTMLImageElement::setAlign(const String& value)
{
    setAttribute(alignAttr, value);
}

String HTMLImageElement::alt() const
{
    return getAttribute(altAttr);
}

void HTMLImageElement::setAlt(const String& value)
{
    setAttribute(altAttr, value);
}

int HTMLImageElement::border() const
{
    // ### return value in pixels
    return getAttribute(borderAttr).toInt();
}

void HTMLImageElement::setBorder(int value)
{
    setAttribute(borderAttr, String::number(value));
}

void HTMLImageElement::setHeight(int value)
{
    setAttribute(heightAttr, String::number(value));
}

int HTMLImageElement::hspace() const
{
    // ### return actual value
    return getAttribute(hspaceAttr).toInt();
}

void HTMLImageElement::setHspace(int value)
{
    setAttribute(hspaceAttr, String::number(value));
}

bool HTMLImageElement::isMap() const
{
    return !getAttribute(ismapAttr).isNull();
}

void HTMLImageElement::setIsMap(bool isMap)
{
    setAttribute(ismapAttr, isMap ? "" : 0);
}

String HTMLImageElement::longDesc() const
{
    return getAttribute(longdescAttr);
}

void HTMLImageElement::setLongDesc(const String& value)
{
    setAttribute(longdescAttr, value);
}

String HTMLImageElement::src() const
{
    return getDocument()->completeURL(getAttribute(srcAttr));
}

void HTMLImageElement::setSrc(const String& value)
{
    setAttribute(srcAttr, value);
}

String HTMLImageElement::useMap() const
{
    return getAttribute(usemapAttr);
}

void HTMLImageElement::setUseMap(const String& value)
{
    setAttribute(usemapAttr, value);
}

int HTMLImageElement::vspace() const
{
    // ### return actual vspace
    return getAttribute(vspaceAttr).toInt();
}

void HTMLImageElement::setVspace(int value)
{
    setAttribute(vspaceAttr, String::number(value));
}

void HTMLImageElement::setWidth(int value)
{
    setAttribute(widthAttr, String::number(value));
}

int HTMLImageElement::x() const
{
    RenderObject *r = renderer();
    if (!r)
        return 0;
    int x, y;
    r->absolutePosition(x, y);
    return x;
}

int HTMLImageElement::y() const
{
    RenderObject *r = renderer();
    if (!r)
        return 0;
    int x, y;
    r->absolutePosition(x, y);
    return y;
}

bool HTMLImageElement::complete() const
{
    return m_imageLoader.imageComplete();
}

// -------------------------------------------------------------------------

HTMLMapElement::HTMLMapElement(Document *doc)
    : HTMLElement(mapTag, doc)
{
}

HTMLMapElement::~HTMLMapElement()
{
    getDocument()->removeImageMap(this);
}

bool HTMLMapElement::checkDTD(const Node* newChild)
{
    // FIXME: This seems really odd, allowing only blocks inside map elements.
    return newChild->hasTagName(areaTag) || newChild->hasTagName(scriptTag) || inBlockTagList(newChild);
}

bool HTMLMapElement::mapMouseEvent(int x, int y, int width, int height, RenderObject::NodeInfo& info)
{
    Node *node = this;
    while ((node = node->traverseNextNode(this)))
        if (node->hasTagName(areaTag))
            if (static_cast<HTMLAreaElement *>(node)->mapMouseEvent(x, y, width, height, info))
                return true;
    return false;
}

void HTMLMapElement::parseMappedAttribute(MappedAttribute *attr)
{
    const QualifiedName& attrName = attr->name();
    if (attrName == idAttr || attrName == nameAttr) {
        Document* doc = getDocument();
        if (attrName == idAttr) {
            // Call base class so that hasID bit gets set.
            HTMLElement::parseMappedAttribute(attr);
            if (doc->htmlMode() != Document::XHtml)
                return;
        }
        doc->removeImageMap(this);
        m_name = attr->value();
        if (m_name[0] == '#') {
            String mapName = mapName.copy();
            mapName.remove(0, 1);
            m_name = mapName.impl();
        }
        doc->addImageMap(this);
    } else
        HTMLElement::parseMappedAttribute(attr);
}

PassRefPtr<HTMLCollection> HTMLMapElement::areas()
{
    return new HTMLCollection(this, HTMLCollection::MAP_AREAS);
}

String HTMLMapElement::name() const
{
    return getAttribute(nameAttr);
}

void HTMLMapElement::setName(const String& value)
{
    setAttribute(nameAttr, value);
}

// -------------------------------------------------------------------------

HTMLAreaElement::HTMLAreaElement(Document *doc)
    : HTMLAnchorElement(areaTag, doc)
{
    m_coords = 0;
    m_coordsLen = 0;
    m_shape = Unknown;
    lasth = -1;
    lastw = -1;
}

HTMLAreaElement::~HTMLAreaElement()
{
    delete [] m_coords;
}

void HTMLAreaElement::parseMappedAttribute(MappedAttribute *attr)
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
        HTMLAnchorElement::parseMappedAttribute(attr);
}

bool HTMLAreaElement::mapMouseEvent(int x, int y, int width, int height, RenderObject::NodeInfo& info)
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

IntRect HTMLAreaElement::getRect(RenderObject* obj) const
{
    int dx, dy;
    obj->absolutePosition(dx, dy);
    Path p = getRegion(lastw,lasth);
    p.translate(dx, dy);
    return p.boundingRect();
}

Path HTMLAreaElement::getRegion(int width, int height) const
{
    if (!m_coords)
        return Path();

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
                    points.setPoint(i, m_coords[i * 2].calcMinValue(width), m_coords[i * 2 + 1].calcMinValue(height));
                return Path(points);
            }
            break;
        case Circle:
            if (m_coordsLen >= 3) {
                Length radius = m_coords[2];
                int r = kMin(radius.calcMinValue(width), radius.calcMinValue(height));
                return Path(IntRect(m_coords[0].calcMinValue(width) - r, m_coords[1].calcMinValue(height) - r,
                    2 * r, 2 * r), Path::Ellipse);
            }
            break;
        case Rect:
            if (m_coordsLen >= 4) {
                int x0 = m_coords[0].calcMinValue(width);
                int y0 = m_coords[1].calcMinValue(height);
                int x1 = m_coords[2].calcMinValue(width);
                int y1 = m_coords[3].calcMinValue(height);
                return Path(IntRect(x0, y0, x1 - x0, y1 - y0));
            }
            break;
        case Default:
            return Path(IntRect(0, 0, width, height));
        case Unknown:
            break;
    }

    return Path();
}

String HTMLAreaElement::accessKey() const
{
    return getAttribute(accesskeyAttr);
}

void HTMLAreaElement::setAccessKey(const String& value)
{
    setAttribute(accesskeyAttr, value);
}

String HTMLAreaElement::alt() const
{
    return getAttribute(altAttr);
}

void HTMLAreaElement::setAlt(const String& value)
{
    setAttribute(altAttr, value);
}

String HTMLAreaElement::coords() const
{
    return getAttribute(coordsAttr);
}

void HTMLAreaElement::setCoords(const String& value)
{
    setAttribute(coordsAttr, value);
}

String HTMLAreaElement::href() const
{
    return getDocument()->completeURL(getAttribute(hrefAttr));
}

void HTMLAreaElement::setHref(const String& value)
{
    setAttribute(hrefAttr, value);
}

bool HTMLAreaElement::noHref() const
{
    return !getAttribute(nohrefAttr).isNull();
}

void HTMLAreaElement::setNoHref(bool noHref)
{
    setAttribute(nohrefAttr, noHref ? "" : 0);
}

String HTMLAreaElement::shape() const
{
    return getAttribute(shapeAttr);
}

void HTMLAreaElement::setShape(const String& value)
{
    setAttribute(shapeAttr, value);
}

int HTMLAreaElement::tabIndex() const
{
    return getAttribute(tabindexAttr).toInt();
}

void HTMLAreaElement::setTabIndex(int tabIndex)
{
    setAttribute(tabindexAttr, String::number(tabIndex));
}

String HTMLAreaElement::target() const
{
    return getAttribute(targetAttr);
}

void HTMLAreaElement::setTarget(const String& value)
{
    setAttribute(targetAttr, value);
}

}
