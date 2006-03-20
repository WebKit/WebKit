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

#include "config.h"
#include "html_blockimpl.h"
#include "HTMLDocument.h"
#include "css/cssstyleselector.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

HTMLBlockquoteElement::HTMLBlockquoteElement(Document *doc)
    : HTMLElement(blockquoteTag, doc)
{
}

HTMLBlockquoteElement::~HTMLBlockquoteElement()
{
}

String HTMLBlockquoteElement::cite() const
{
    return getAttribute(citeAttr);
}

void HTMLBlockquoteElement::setCite(const String &value)
{
    setAttribute(citeAttr, value);
}

// -------------------------------------------------------------------------

HTMLDivElement::HTMLDivElement(Document *doc)
    : HTMLElement(divTag, doc)
{
}

HTMLDivElement::~HTMLDivElement()
{
}

bool HTMLDivElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == alignAttr) {
        result = eBlock;
        return false;
    }
    return HTMLElement::mapToEntry(attrName, result);
}
        
void HTMLDivElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == alignAttr) {
        String v = attr->value();
        if (equalIgnoringCase(attr->value(), "middle") || equalIgnoringCase(attr->value(), "center"))
           addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_CENTER);
        else if (equalIgnoringCase(attr->value(), "left"))
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_LEFT);
        else if (equalIgnoringCase(attr->value(), "right"))
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_RIGHT);
        else
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, v);
    } else
        HTMLElement::parseMappedAttribute(attr);
}

String HTMLDivElement::align() const
{
    return getAttribute(alignAttr);
}

void HTMLDivElement::setAlign(const String &value)
{
    setAttribute(alignAttr, value);
}

// -------------------------------------------------------------------------

HTMLHRElement::HTMLHRElement(Document *doc)
    : HTMLElement(hrTag, doc)
{
}

HTMLHRElement::~HTMLHRElement()
{
}

bool HTMLHRElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == alignAttr ||
        attrName == widthAttr ||
        attrName == colorAttr ||
        attrName == sizeAttr ||
        attrName == noshadeAttr) {
        result = eHR;
        return false;
    }
    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLHRElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == alignAttr) {
        if (equalIgnoringCase(attr->value(), "left")) {
            addCSSProperty(attr, CSS_PROP_MARGIN_LEFT, "0");
            addCSSProperty(attr, CSS_PROP_MARGIN_RIGHT, CSS_VAL_AUTO);
        } else if (equalIgnoringCase(attr->value(), "right")) {
            addCSSProperty(attr, CSS_PROP_MARGIN_LEFT, CSS_VAL_AUTO);
            addCSSProperty(attr, CSS_PROP_MARGIN_RIGHT, "0");
        } else {
            addCSSProperty(attr, CSS_PROP_MARGIN_LEFT, CSS_VAL_AUTO);
            addCSSProperty(attr, CSS_PROP_MARGIN_RIGHT, CSS_VAL_AUTO);
        }
    } else if (attr->name() == widthAttr) {
        bool ok;
        int v = attr->value().impl()->toInt(&ok);
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
        addCSSColor(attr, CSS_PROP_BORDER_COLOR, String("grey"));
        addCSSColor(attr, CSS_PROP_BACKGROUND_COLOR, String("grey"));
    } else if (attr->name() == sizeAttr) {
        StringImpl* si = attr->value().impl();
        int size = si->toInt();
        if (size <= 1)
            addCSSProperty(attr, CSS_PROP_BORDER_BOTTOM_WIDTH, String("0"));
        else
            addCSSLength(attr, CSS_PROP_HEIGHT, String(DeprecatedString::number(size-2)));
    } else
        HTMLElement::parseMappedAttribute(attr);
}

String HTMLHRElement::align() const
{
    return getAttribute(alignAttr);
}

void HTMLHRElement::setAlign(const String &value)
{
    setAttribute(alignAttr, value);
}

bool HTMLHRElement::noShade() const
{
    return !getAttribute(noshadeAttr).isNull();
}

void HTMLHRElement::setNoShade(bool noShade)
{
    setAttribute(noshadeAttr, noShade ? "" : 0);
}

String HTMLHRElement::size() const
{
    return getAttribute(sizeAttr);
}

void HTMLHRElement::setSize(const String &value)
{
    setAttribute(sizeAttr, value);
}

String HTMLHRElement::width() const
{
    return getAttribute(widthAttr);
}

void HTMLHRElement::setWidth(const String &value)
{
    setAttribute(widthAttr, value);
}

// -------------------------------------------------------------------------

HTMLHeadingElement::HTMLHeadingElement(const QualifiedName& tagName, Document *doc)
    : HTMLElement(tagName, doc)
{
}

bool HTMLHeadingElement::checkDTD(const Node* newChild)
{
    if (newChild->hasTagName(h1Tag) || newChild->hasTagName(h2Tag) ||
        newChild->hasTagName(h3Tag) || newChild->hasTagName(h4Tag) ||
        newChild->hasTagName(h5Tag) || newChild->hasTagName(h6Tag))
        return false;

    return inEitherTagList(newChild);
}

String HTMLHeadingElement::align() const
{
    return getAttribute(alignAttr);
}

void HTMLHeadingElement::setAlign(const String &value)
{
    setAttribute(alignAttr, value);
}

// -------------------------------------------------------------------------

HTMLParagraphElement::HTMLParagraphElement(Document *doc)
    : HTMLElement(pTag, doc)
{
}

bool HTMLParagraphElement::checkDTD(const Node* newChild)
{
    return inInlineTagList(newChild) || (getDocument()->inCompatMode() && newChild->hasTagName(tableTag));
}

bool HTMLParagraphElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == alignAttr) {
        result = eBlock; // We can share with DIV here.
        return false;
    }
    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLParagraphElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == alignAttr) {
        String v = attr->value();
        if (equalIgnoringCase(attr->value(), "middle") || equalIgnoringCase(attr->value(), "center"))
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_CENTER);
        else if (equalIgnoringCase(attr->value(), "left"))
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_LEFT);
        else if (equalIgnoringCase(attr->value(), "right"))
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, CSS_VAL__KHTML_RIGHT);
        else
            addCSSProperty(attr, CSS_PROP_TEXT_ALIGN, v);
    } else
        HTMLElement::parseMappedAttribute(attr);
}

String HTMLParagraphElement::align() const
{
    return getAttribute(alignAttr);
}

void HTMLParagraphElement::setAlign(const String &value)
{
    setAttribute(alignAttr, value);
}

// -------------------------------------------------------------------------

HTMLPreElement::HTMLPreElement(const QualifiedName& tagName, Document *doc)
    : HTMLElement(tagName, doc)
{
}

bool HTMLPreElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == widthAttr || attrName == wrapAttr) {
        result = ePre;
        return false;
    }
    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLPreElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == widthAttr) {
        // FIXME: Implement this some day.  Width on a <pre> is the # of characters that
        // we should size the pre to.  We basically need to take the width of a space,
        // multiply by the value of the attribute and then set that as the width CSS
        // property.
    } else if (attr->name() == wrapAttr) {
        if (!attr->value().isNull())
            addCSSProperty(attr, CSS_PROP_WHITE_SPACE, CSS_VAL_PRE_WRAP);
    } else
        return HTMLElement::parseMappedAttribute(attr);
}

int HTMLPreElement::width() const
{
    return getAttribute(widthAttr).toInt();
}

void HTMLPreElement::setWidth(int width)
{
    setAttribute(widthAttr, DeprecatedString::number(width));
}

bool HTMLPreElement::wrap() const
{
    return !getAttribute(wrapAttr).isNull();
}

void HTMLPreElement::setWrap(bool wrap)
{
    setAttribute(wrapAttr, wrap ? "" : 0);
}
    
// -------------------------------------------------------------------------

 // WinIE uses 60ms as the minimum delay by default.
const int defaultMinimumDelay = 60;

HTMLMarqueeElement::HTMLMarqueeElement(Document *doc)
: HTMLElement(marqueeTag, doc),
  m_minimumDelay(defaultMinimumDelay)
{
}

bool HTMLMarqueeElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
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

    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLMarqueeElement::parseMappedAttribute(MappedAttribute *attr)
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
            if (attr->value() == "-1" || equalIgnoringCase(attr->value(), "infinite"))
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
        HTMLElement::parseMappedAttribute(attr);
}

}
