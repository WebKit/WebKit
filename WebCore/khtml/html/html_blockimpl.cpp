/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
// -------------------------------------------------------------------------
//#define DEBUG
#include "html_blockimpl.h"
#include "html_documentimpl.h"
#include "css/cssstyleselector.h"

#include "css/cssproperties.h"
#include "css/cssvalues.h"

#include <kdebug.h>

using namespace khtml;
using namespace DOM;
using namespace HTMLNames;

HTMLBlockquoteElementImpl::HTMLBlockquoteElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(blockquoteTag, doc)
{
}

HTMLBlockquoteElementImpl::~HTMLBlockquoteElementImpl()
{
}

DOMString HTMLBlockquoteElementImpl::cite() const
{
    return getAttribute(citeAttr);
}

void HTMLBlockquoteElementImpl::setCite(const DOMString &value)
{
    setAttribute(citeAttr, value);
}

// -------------------------------------------------------------------------

HTMLDivElementImpl::HTMLDivElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(divTag, doc)
{
}

HTMLDivElementImpl::~HTMLDivElementImpl()
{
}

bool HTMLDivElementImpl::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == alignAttr) {
        result = eBlock;
        return false;
    }
    return HTMLElementImpl::mapToEntry(attrName, result);
}
        
void HTMLDivElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == alignAttr) {
        DOMString v = attr->value();
	if ( strcasecmp( attr->value(), "middle" ) == 0 || strcasecmp( attr->value(), "center" ) == 0 )
           addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_CENTER);
        else if (strcasecmp(attr->value(), "left") == 0)
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_LEFT);
        else if (strcasecmp(attr->value(), "right") == 0)
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_RIGHT);
        else
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, v);
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
}

DOMString HTMLDivElementImpl::align() const
{
    return getAttribute(alignAttr);
}

void HTMLDivElementImpl::setAlign(const DOMString &value)
{
    setAttribute(alignAttr, value);
}

// -------------------------------------------------------------------------

HTMLHRElementImpl::HTMLHRElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(hrTag, doc)
{
}

HTMLHRElementImpl::~HTMLHRElementImpl()
{
}

bool HTMLHRElementImpl::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == alignAttr ||
        attrName == widthAttr ||
        attrName == colorAttr ||
        attrName == sizeAttr ||
        attrName == noshadeAttr) {
        result = eHR;
        return false;
    }
    return HTMLElementImpl::mapToEntry(attrName, result);
}

void HTMLHRElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == alignAttr) {
        if (strcasecmp(attr->value(), "left") == 0) {
            addCSSProperty(attr, CSS_PROP_MARGIN_LEFT, "0");
	    addCSSProperty(attr, CSS_PROP_MARGIN_RIGHT, CSS_VAL_AUTO);
	}
        else if (strcasecmp(attr->value(), "right") == 0) {
	    addCSSProperty(attr, CSS_PROP_MARGIN_LEFT, CSS_VAL_AUTO);
	    addCSSProperty(attr, CSS_PROP_MARGIN_RIGHT, "0");
	}
	else {
      	    addCSSProperty(attr, CSS_PROP_MARGIN_LEFT, CSS_VAL_AUTO);
            addCSSProperty(attr, CSS_PROP_MARGIN_RIGHT, CSS_VAL_AUTO);
	}
    } else if (attr->name() == widthAttr) {
        bool ok;
        int v = attr->value().implementation()->toInt(&ok);
        if(ok && !v)
            addCSSLength(attr, CSS_PROP_WIDTH, "1");
        else
            addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    } else if (attr->name() == colorAttr) {
        addCSSProperty(attr, CSS_PROP_BORDER_TOP_STYLE, CSS_VAL_SOLID);
        addCSSProperty(attr, CSS_PROP_BORDER_RIGHT_STYLE, CSS_VAL_SOLID);
        addCSSProperty(attr, CSS_PROP_BORDER_BOTTOM_STYLE, CSS_VAL_SOLID);
        addCSSProperty(attr, CSS_PROP_BORDER_LEFT_STYLE, CSS_VAL_SOLID);
        addCSSColor(attr, CSS_PROP_BORDER_COLOR, attr->value());
        addCSSColor(attr, CSS_PROP_BACKGROUND_COLOR, attr->value());
    } else if (attr->name() == noshadeAttr) {
        addCSSProperty(attr, CSS_PROP_BORDER_TOP_STYLE, CSS_VAL_SOLID);
        addCSSProperty(attr, CSS_PROP_BORDER_RIGHT_STYLE, CSS_VAL_SOLID);
        addCSSProperty(attr, CSS_PROP_BORDER_BOTTOM_STYLE, CSS_VAL_SOLID);
        addCSSProperty(attr, CSS_PROP_BORDER_LEFT_STYLE, CSS_VAL_SOLID);
        addCSSColor(attr, CSS_PROP_BORDER_COLOR, DOMString("grey"));
        addCSSColor(attr, CSS_PROP_BACKGROUND_COLOR, DOMString("grey"));
    } else if (attr->name() == sizeAttr) {
        DOMStringImpl* si = attr->value().implementation();
        int size = si->toInt();
        if (size <= 1)
            addCSSProperty(attr, CSS_PROP_BORDER_BOTTOM_WIDTH, DOMString("0"));
        else
            addCSSLength(attr, CSS_PROP_HEIGHT, DOMString(QString::number(size-2)));
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
}

DOMString HTMLHRElementImpl::align() const
{
    return getAttribute(alignAttr);
}

void HTMLHRElementImpl::setAlign(const DOMString &value)
{
    setAttribute(alignAttr, value);
}

bool HTMLHRElementImpl::noShade() const
{
    return !getAttribute(noshadeAttr).isNull();
}

void HTMLHRElementImpl::setNoShade(bool noShade)
{
    setAttribute(noshadeAttr, noShade ? "" : 0);
}

DOMString HTMLHRElementImpl::size() const
{
    return getAttribute(sizeAttr);
}

void HTMLHRElementImpl::setSize(const DOMString &value)
{
    setAttribute(sizeAttr, value);
}

DOMString HTMLHRElementImpl::width() const
{
    return getAttribute(widthAttr);
}

void HTMLHRElementImpl::setWidth(const DOMString &value)
{
    setAttribute(widthAttr, value);
}

// -------------------------------------------------------------------------

HTMLHeadingElementImpl::HTMLHeadingElementImpl(const QualifiedName& tagName, DocumentPtr *doc)
    : HTMLElementImpl(tagName, doc)
{
}

bool HTMLHeadingElementImpl::checkDTD(const NodeImpl* newChild)
{
    if (newChild->hasTagName(h1Tag) || newChild->hasTagName(h2Tag) ||
        newChild->hasTagName(h3Tag) || newChild->hasTagName(h4Tag) ||
        newChild->hasTagName(h5Tag) || newChild->hasTagName(h6Tag))
        return false;

    return inEitherTagList(newChild);
}

DOMString HTMLHeadingElementImpl::align() const
{
    return getAttribute(alignAttr);
}

void HTMLHeadingElementImpl::setAlign(const DOMString &value)
{
    setAttribute(alignAttr, value);
}

// -------------------------------------------------------------------------

HTMLParagraphElementImpl::HTMLParagraphElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(pTag, doc)
{
}

bool HTMLParagraphElementImpl::checkDTD(const NodeImpl* newChild)
{
    return inInlineTagList(newChild) || (getDocument()->inCompatMode() && newChild->hasTagName(tableTag));
}

bool HTMLParagraphElementImpl::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == alignAttr) {
        result = eBlock; // We can share with DIV here.
        return false;
    }
    return HTMLElementImpl::mapToEntry(attrName, result);
}

void HTMLParagraphElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == alignAttr) {
        DOMString v = attr->value();
        if ( strcasecmp( attr->value(), "middle" ) == 0 || strcasecmp( attr->value(), "center" ) == 0 )
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_CENTER);
        else if (strcasecmp(attr->value(), "left") == 0)
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_LEFT);
        else if (strcasecmp(attr->value(), "right") == 0)
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_RIGHT);
        else
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, v);
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
}

DOMString HTMLParagraphElementImpl::align() const
{
    return getAttribute(alignAttr);
}

void HTMLParagraphElementImpl::setAlign(const DOMString &value)
{
    setAttribute(alignAttr, value);
}

// -------------------------------------------------------------------------

HTMLPreElementImpl::HTMLPreElementImpl(const QualifiedName& tagName, DocumentPtr *doc)
    : HTMLElementImpl(tagName, doc)
{
}

long HTMLPreElementImpl::width() const
{
    return getAttribute(widthAttr).toInt();
}

void HTMLPreElementImpl::setWidth(long width)
{
    setAttribute(widthAttr, QString::number(width));
}

// -------------------------------------------------------------------------

 // WinIE uses 60ms as the minimum delay by default.
const int defaultMinimumDelay = 60;

HTMLMarqueeElementImpl::HTMLMarqueeElementImpl(DocumentPtr *doc)
: HTMLElementImpl(marqueeTag, doc),
  m_minimumDelay(defaultMinimumDelay)
{
}

bool HTMLMarqueeElementImpl::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == widthAttr ||
        attrName == heightAttr ||
        attrName == bgcolorAttr ||
        attrName == vspaceAttr ||
        attrName == hspaceAttr ||
        attrName == scrollamountAttr ||
        attrName == scrolldelayAttr ||
        attrName == loopAttr ||
        attrName == behaviorAttr ||
        attrName == directionAttr) {
        result = eUniversal;
        return false;
    }

    return HTMLElementImpl::mapToEntry(attrName, result);
}

void HTMLMarqueeElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if (attr->name() == widthAttr) {
        if (!attr->value().isEmpty())
            addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    } else if (attr->name() == heightAttr) {
        if (!attr->value().isEmpty())
            addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
    } else if (attr->name() == bgcolorAttr) {
        if (!attr->value().isEmpty())
            addCSSColor(attr, CSS_PROP_BACKGROUND_COLOR, attr->value());
    } else if (attr->name() == vspaceAttr) {
        if (!attr->value().isEmpty()) {
            addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
            addCSSLength(attr, CSS_PROP_MARGIN_BOTTOM, attr->value());
        }
    } else if (attr->name() == hspaceAttr) {
        if (!attr->value().isEmpty()) {
            addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value());
            addCSSLength(attr, CSS_PROP_MARGIN_RIGHT, attr->value());
        }
    } else if (attr->name() == scrollamountAttr) {
        if (!attr->value().isEmpty())
            addCSSLength(attr, CSS_PROP__KHTML_MARQUEE_INCREMENT, attr->value());
    } else if (attr->name() == scrolldelayAttr) {
        if (!attr->value().isEmpty())
            addCSSLength(attr, CSS_PROP__KHTML_MARQUEE_SPEED, attr->value());
    } else if (attr->name() == loopAttr) {
        if (!attr->value().isEmpty()) {
            if (attr->value() == "-1" || strcasecmp(attr->value(), "infinite") == 0)
                addCSSProperty(attr, CSS_PROP__KHTML_MARQUEE_REPETITION, CSS_VAL_INFINITE);
            else
                addCSSLength(attr, CSS_PROP__KHTML_MARQUEE_REPETITION, attr->value());
        }
    } else if (attr->name() == behaviorAttr) {
        if (!attr->value().isEmpty())
            addCSSProperty(attr, CSS_PROP__KHTML_MARQUEE_STYLE, attr->value());
    } else if (attr->name() == directionAttr) {
        if (!attr->value().isEmpty())
            addCSSProperty(attr, CSS_PROP__KHTML_MARQUEE_DIRECTION, attr->value());
    } else if (attr->name() == truespeedAttr) {
        m_minimumDelay = !attr->isNull() ? 0 : defaultMinimumDelay;
    } else
        HTMLElementImpl::parseMappedAttribute(attr);
}
