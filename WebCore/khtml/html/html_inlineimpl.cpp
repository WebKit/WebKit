/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
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
// -------------------------------------------------------------------------

#include "config.h"
#include "html/html_inlineimpl.h"

#include "EventNames.h"
#include "Frame.h"
#include "PlatformKeyboardEvent.h"
#include "csshelper.h"
#include "CSSPropertyNames.h"
#include "cssstyleselector.h"
#include "CSSValueKeywords.h"
#include "dom2_eventsimpl.h"
#include "HTMLDocument.h"
#include "html_imageimpl.h"
#include "htmlnames.h"
#include "RenderFlow.h"
#include "RenderImage.h"
#include "rendering/RenderBR.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

HTMLAnchorElement::HTMLAnchorElement(Document *doc)
    : HTMLElement(aTag, doc)
{
    m_hasTarget = false;
}

HTMLAnchorElement::HTMLAnchorElement(const QualifiedName& tagName, Document *doc)
    : HTMLElement(tagName, doc)
{
    m_hasTarget = false;
}

HTMLAnchorElement::~HTMLAnchorElement()
{
}

bool HTMLAnchorElement::isFocusable() const
{
    if (isContentEditable())
        return HTMLElement::isFocusable();

    // FIXME: Even if we are not visible, we might have a child that is visible.
    // Dave wants to fix that some day with a "has visible content" flag or the like.
    if (!(m_isLink && renderer() && renderer()->style()->visibility() == VISIBLE))
        return false;

    // Before calling absoluteRects, check for the common case where the renderer
    // or one of the continuations is non-empty, since this is a faster check and
    // almost always returns true.
    for (RenderObject *r = renderer(); r; r = r->continuation())
        if (r->width() > 0 && r->height() > 0)
            return true;

    DeprecatedValueList<IntRect> rects;
    int x = 0, y = 0;
    renderer()->absolutePosition(x, y);
    renderer()->absoluteRects(rects, x, y);
    for (DeprecatedValueList<IntRect>::ConstIterator it = rects.begin(); it != rects.end(); ++it)
        if (!(*it).isEmpty())
            return true;

    return false;
}

bool HTMLAnchorElement::isMouseFocusable() const
{
    return false;
}

bool HTMLAnchorElement::isKeyboardFocusable() const
{
    if (!isFocusable())
        return false;
    
    if (!getDocument()->frame())
        return false;

    return getDocument()->frame()->tabsToLinks();
}

void HTMLAnchorElement::defaultEventHandler(Event *evt)
{
    // React on clicks and on keypresses.
    // Don't make this KEYUP_EVENT again, it makes khtml follow links it shouldn't,
    // when pressing Enter in the combo.
    if ( ( evt->type() == clickEvent ||
         ( evt->type() == keydownEvent && m_focused)) && m_isLink) {
        MouseEvent *e = 0;
        if ( evt->type() == clickEvent )
            e = static_cast<MouseEvent*>( evt );

        KeyboardEvent *k = 0;
        if (evt->type() == keydownEvent)
            k = static_cast<KeyboardEvent *>( evt );

        DeprecatedString utarget;
        DeprecatedString url;

        if ( e && e->button() == 2 ) {
            HTMLElement::defaultEventHandler(evt);
            return;
        }

        if ( k ) {
            if (k->keyIdentifier() != "Enter") {
                HTMLElement::defaultEventHandler(evt);
                return;
            }
            if (k->keyEvent()) {
                evt->setDefaultHandled();
                click(false);
                return;
            }
        }

        url = WebCore::parseURL(getAttribute(hrefAttr)).deprecatedString();

        utarget = getAttribute(targetAttr).deprecatedString();

        if ( e && e->button() == 1 )
            utarget = "_blank";

        if (evt->target()->hasTagName(imgTag)) {
            HTMLImageElement* img = static_cast<HTMLImageElement*>( evt->target() );
            if ( img && img->isServerMap() )
            {
                WebCore::RenderImage *r = static_cast<WebCore::RenderImage *>(img->renderer());
                if(r && e)
                {
                    int absx, absy;
                    r->absolutePosition(absx, absy);
                    int x(e->clientX() - absx), y(e->clientY() - absy);
                    url += DeprecatedString("?%1,%2").arg( x ).arg( y );
                }
                else {
                    evt->setDefaultHandled();
                    HTMLElement::defaultEventHandler(evt);
                    return;
                }
            }
        }
        if (!evt->defaultPrevented()) {
            if (getDocument()->frame())
                getDocument()->frame()->urlSelected(url, utarget);
        }
        evt->setDefaultHandled();
    }
    HTMLElement::defaultEventHandler(evt);
}


void HTMLAnchorElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == hrefAttr) {
        m_isLink = !attr->isNull();
    } else if (attr->name() == targetAttr) {
        m_hasTarget = !attr->isNull();
    } else if (attr->name() == nameAttr ||
             attr->name() == titleAttr ||
             attr->name() == relAttr) {
        // Do nothing.
    } else
        HTMLElement::parseMappedAttribute(attr);
}

void HTMLAnchorElement::accessKeyAction(bool sendToAnyElement)
{
    // send the mouse button events iff the
    // caller specified sendToAnyElement
    click(sendToAnyElement);
}

bool HTMLAnchorElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == hrefAttr;
}

String HTMLAnchorElement::accessKey() const
{
    return getAttribute(accesskeyAttr);
}

void HTMLAnchorElement::setAccessKey(const String &value)
{
    setAttribute(accesskeyAttr, value);
}

String HTMLAnchorElement::charset() const
{
    return getAttribute(charsetAttr);
}

void HTMLAnchorElement::setCharset(const String &value)
{
    setAttribute(charsetAttr, value);
}

String HTMLAnchorElement::coords() const
{
    return getAttribute(coordsAttr);
}

void HTMLAnchorElement::setCoords(const String &value)
{
    setAttribute(coordsAttr, value);
}

String HTMLAnchorElement::href() const
{
    String href = getAttribute(hrefAttr);
    if (href.isNull())
        return href;
    return getDocument()->completeURL(href);
}

void HTMLAnchorElement::setHref(const String &value)
{
    setAttribute(hrefAttr, value);
}

String HTMLAnchorElement::hreflang() const
{
    return getAttribute(hreflangAttr);
}

void HTMLAnchorElement::setHreflang(const String &value)
{
    setAttribute(hreflangAttr, value);
}

String HTMLAnchorElement::name() const
{
    return getAttribute(nameAttr);
}

void HTMLAnchorElement::setName(const String &value)
{
    setAttribute(nameAttr, value);
}

String HTMLAnchorElement::rel() const
{
    return getAttribute(relAttr);
}

void HTMLAnchorElement::setRel(const String &value)
{
    setAttribute(relAttr, value);
}

String HTMLAnchorElement::rev() const
{
    return getAttribute(revAttr);
}

void HTMLAnchorElement::setRev(const String &value)
{
    setAttribute(revAttr, value);
}

String HTMLAnchorElement::shape() const
{
    return getAttribute(shapeAttr);
}

void HTMLAnchorElement::setShape(const String &value)
{
    setAttribute(shapeAttr, value);
}

int HTMLAnchorElement::tabIndex() const
{
    return getAttribute(tabindexAttr).toInt();
}

void HTMLAnchorElement::setTabIndex(int tabIndex)
{
    setAttribute(tabindexAttr, DeprecatedString::number(tabIndex));
}

String HTMLAnchorElement::target() const
{
    return getAttribute(targetAttr);
}

void HTMLAnchorElement::setTarget(const String &value)
{
    setAttribute(targetAttr, value);
}

String HTMLAnchorElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLAnchorElement::setType(const String &value)
{
    setAttribute(typeAttr, value);
}

void HTMLAnchorElement::blur()
{
    Document *d = getDocument();
    if (d->focusNode() == this)
        d->setFocusNode(0);
}

void HTMLAnchorElement::focus()
{
    getDocument()->setFocusNode(this);
}

// -------------------------------------------------------------------------

HTMLBRElement::HTMLBRElement(Document *doc) : HTMLElement(brTag, doc)
{
}

HTMLBRElement::~HTMLBRElement()
{
}

bool HTMLBRElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == clearAttr) {
        result = eUniversal;
        return false;
    }
    
    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLBRElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == clearAttr) {
        // If the string is empty, then don't add the clear property. 
        // <br clear> and <br clear=""> are just treated like <br> by Gecko, Mac IE, etc. -dwh
        const AtomicString& str = attr->value();
        if (!str.isEmpty()) {
            if (equalIgnoringCase(str, "all"))
                addCSSProperty(attr, CSS_PROP_CLEAR, "both");
            else
                addCSSProperty(attr, CSS_PROP_CLEAR, str);
        }
    } else
        HTMLElement::parseMappedAttribute(attr);
}

RenderObject *HTMLBRElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
     return new (arena) RenderBR(this);
}

String HTMLBRElement::clear() const
{
    return getAttribute(clearAttr);
}

void HTMLBRElement::setClear(const String &value)
{
    setAttribute(clearAttr, value);
}

// -------------------------------------------------------------------------

HTMLFontElement::HTMLFontElement(Document *doc)
    : HTMLElement(fontTag, doc)
{
}

HTMLFontElement::~HTMLFontElement()
{
}

// Allows leading spaces.
// Allows trailing nonnumeric characters.
// Returns 10 for any size greater than 9.
static bool parseFontSizeNumber(const String &s, int &size)
{
    unsigned pos = 0;
    
    // Skip leading spaces.
    while (pos < s.length() && s[pos].isSpace())
        ++pos;
    
    // Skip a plus or minus.
    bool sawPlus = false;
    bool sawMinus = false;
    if (pos < s.length() && s[pos] == '+') {
        ++pos;
        sawPlus = true;
    } else if (pos < s.length() && s[pos] == '-') {
        ++pos;
        sawMinus = true;
    }
    
    // Parse a single digit.
    if (pos >= s.length() || !s[pos].isNumber())
        return false;
    int num = s[pos++].digitValue();
    
    // Check for an additional digit.
    if (pos < s.length() && s[pos].isNumber())
        num = 10;
    
    if (sawPlus) {
        size = num + 3;
        return true;
    }
    
    // Don't return 0 (which means 3) or a negative number (which means the same as 1).
    if (sawMinus) {
        size = num == 1 ? 2 : 1;
        return true;
    }
    
    size = num;
    return true;
}

bool HTMLFontElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == sizeAttr ||
        attrName == colorAttr ||
        attrName == faceAttr) {
        result = eUniversal;
        return false;
    }
    
    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLFontElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == sizeAttr) {
        int num;
        if (parseFontSizeNumber(attr->value(), num)) {
            int size;
            switch (num)
            {
            case 2: size = CSS_VAL_SMALL; break;
            case 0: // treat 0 the same as 3, because people expect it to be between -1 and +1
            case 3: size = CSS_VAL_MEDIUM; break;
            case 4: size = CSS_VAL_LARGE; break;
            case 5: size = CSS_VAL_X_LARGE; break;
            case 6: size = CSS_VAL_XX_LARGE; break;
            default:
                if (num > 6)
                    size = CSS_VAL__KHTML_XXX_LARGE;
                else
                    size = CSS_VAL_X_SMALL;
            }
            addCSSProperty(attr, CSS_PROP_FONT_SIZE, size);
        }
    } else if (attr->name() == colorAttr) {
        addCSSColor(attr, CSS_PROP_COLOR, attr->value());
    } else if (attr->name() == faceAttr) {
        addCSSProperty(attr, CSS_PROP_FONT_FAMILY, attr->value());
    } else
        HTMLElement::parseMappedAttribute(attr);
}

String HTMLFontElement::color() const
{
    return getAttribute(colorAttr);
}

void HTMLFontElement::setColor(const String &value)
{
    setAttribute(colorAttr, value);
}

String HTMLFontElement::face() const
{
    return getAttribute(faceAttr);
}

void HTMLFontElement::setFace(const String &value)
{
    setAttribute(faceAttr, value);
}

String HTMLFontElement::size() const
{
    return getAttribute(sizeAttr);
}

void HTMLFontElement::setSize(const String &value)
{
    setAttribute(sizeAttr, value);
}

// -------------------------------------------------------------------------

HTMLModElement::HTMLModElement(const QualifiedName& tagName, Document *doc)
    : HTMLElement(tagName, doc)
{
}

String HTMLModElement::cite() const
{
    return getAttribute(citeAttr);
}

void HTMLModElement::setCite(const String &value)
{
    setAttribute(citeAttr, value);
}

String HTMLModElement::dateTime() const
{
    return getAttribute(datetimeAttr);
}

void HTMLModElement::setDateTime(const String &value)
{
    setAttribute(datetimeAttr, value);
}

// -------------------------------------------------------------------------

HTMLQuoteElement::HTMLQuoteElement(Document *doc)
    : HTMLElement(qTag, doc)
{
}

String HTMLQuoteElement::cite() const
{
    return getAttribute(citeAttr);
}

void HTMLQuoteElement::setCite(const String &value)
{
    setAttribute(citeAttr, value);
}

}
