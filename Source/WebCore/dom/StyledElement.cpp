/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2010 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "StyledElement.h"

#include "Attribute.h"
#include "CSSImageValue.h"
#include "CSSStyleSelector.h"
#include "CSSStyleSheet.h"
#include "CSSValueKeywords.h"
#include "Color.h"
#include "ClassList.h"
#include "ContentSecurityPolicy.h"
#include "DOMTokenList.h"
#include "Document.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "StylePropertySet.h"
#include <wtf/HashFunctions.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

void StyledElement::updateStyleAttribute() const
{
    ASSERT(!isStyleAttributeValid());
    setIsStyleAttributeValid();
    setIsSynchronizingStyleAttribute();
    if (StylePropertySet* inlineStyle = inlineStyleDecl())
        const_cast<StyledElement*>(this)->setAttribute(styleAttr, inlineStyle->asText());
    clearIsSynchronizingStyleAttribute();
}

StyledElement::~StyledElement()
{
    destroyInlineStyleDecl();
}

void StyledElement::attributeChanged(Attribute* attr)
{
    if (!(attr->name() == styleAttr && isSynchronizingStyleAttribute()))
        parseAttribute(attr);

    Element::attributeChanged(attr);
}

void StyledElement::classAttributeChanged(const AtomicString& newClassString)
{
    const UChar* characters = newClassString.characters();
    unsigned length = newClassString.length();
    unsigned i;
    for (i = 0; i < length; ++i) {
        if (isNotHTMLSpace(characters[i]))
            break;
    }
    bool hasClass = i < length;
    setHasClass(hasClass);
    if (hasClass) {
        const bool shouldFoldCase = document()->inQuirksMode();
        ensureAttributeData()->setClass(newClassString, shouldFoldCase);
        if (DOMTokenList* classList = optionalClassList())
            static_cast<ClassList*>(classList)->reset(newClassString);
    } else if (attributeData())
        attributeData()->clearClass();
    setNeedsStyleRecalc();
    dispatchSubtreeModifiedEvent();
}

void StyledElement::parseAttribute(Attribute* attr)
{
    if (attr->name() == classAttr)
        classAttributeChanged(attr->value());
    else if (attr->name() == styleAttr) {
        if (attr->isNull())
            destroyInlineStyleDecl();
        else if (document()->contentSecurityPolicy()->allowInlineStyle())
            ensureInlineStyleDecl()->parseDeclaration(attr->value());
        setIsStyleAttributeValid();
        setNeedsStyleRecalc();
    }
}

void StyledElement::removeCSSProperties(int id1, int id2, int id3, int id4, int id5, int id6, int id7, int id8)
{
    StylePropertySet* style = attributeStyle();
    if (!style)
        return;

    ASSERT(id1 != CSSPropertyInvalid);
    style->removeProperty(id1);

    if (id2 == CSSPropertyInvalid)
        return;
    style->removeProperty(id2);
    if (id3 == CSSPropertyInvalid)
        return;
    style->removeProperty(id3);
    if (id4 == CSSPropertyInvalid)
        return;
    style->removeProperty(id4);
    if (id5 == CSSPropertyInvalid)
        return;
    style->removeProperty(id5);
    if (id6 == CSSPropertyInvalid)
        return;
    style->removeProperty(id6);
    if (id7 == CSSPropertyInvalid)
        return;
    style->removeProperty(id7);
    if (id8 == CSSPropertyInvalid)
        return;
    style->removeProperty(id8);
}

void StyledElement::addCSSProperty(int id, const String &value)
{
    if (!ensureAttributeStyle()->setProperty(id, value))
        removeCSSProperty(id);
}

void StyledElement::addCSSProperty(int id, int value)
{
    ensureAttributeStyle()->setProperty(id, value);
}

void StyledElement::addCSSImageProperty(int id, const String& url)
{
    ensureAttributeStyle()->setProperty(CSSProperty(id, CSSImageValue::create(url)));
}

void StyledElement::addCSSLength(int id, const String &value)
{
    // FIXME: This function should not spin up the CSS parser, but should instead just figure out the correct
    // length unit and make the appropriate parsed value.

    // strip attribute garbage..
    StringImpl* v = value.impl();
    if (v) {
        unsigned int l = 0;
        
        while (l < v->length() && (*v)[l] <= ' ')
            l++;
        
        for (; l < v->length(); l++) {
            UChar cc = (*v)[l];
            if (cc > '9')
                break;
            if (cc < '0') {
                if (cc == '%' || cc == '*')
                    l++;
                if (cc != '.')
                    break;
            }
        }

        if (l != v->length()) {
            addCSSProperty(id, v->substring(0, l));
            return;
        }
    }

    addCSSProperty(id, value);
}

static String parseColorStringWithCrazyLegacyRules(const String& colorString)
{
    // Per spec, only look at the first 128 digits of the string.
    const size_t maxColorLength = 128;
    // We'll pad the buffer with two extra 0s later, so reserve two more than the max.
    Vector<char, maxColorLength+2> digitBuffer;
    
    size_t i = 0;
    // Skip a leading #.
    if (colorString[0] == '#')
        i = 1;
    
    // Grab the first 128 characters, replacing non-hex characters with 0.
    // Non-BMP characters are replaced with "00" due to them appearing as two "characters" in the String.
    for (; i < colorString.length() && digitBuffer.size() < maxColorLength; i++) {
        if (!isASCIIHexDigit(colorString[i]))
            digitBuffer.append('0');
        else
            digitBuffer.append(colorString[i]);
    }

    if (!digitBuffer.size())
        return "#000000";

    // Pad the buffer out to at least the next multiple of three in size.
    digitBuffer.append('0');
    digitBuffer.append('0');

    if (digitBuffer.size() < 6)
        return String::format("#0%c0%c0%c", digitBuffer[0], digitBuffer[1], digitBuffer[2]);
    
    // Split the digits into three components, then search the last 8 digits of each component.
    ASSERT(digitBuffer.size() >= 6);
    size_t componentLength = digitBuffer.size() / 3;
    size_t componentSearchWindowLength = min<size_t>(componentLength, 8);
    size_t redIndex = componentLength - componentSearchWindowLength;
    size_t greenIndex = componentLength * 2 - componentSearchWindowLength;
    size_t blueIndex = componentLength * 3 - componentSearchWindowLength;
    // Skip digits until one of them is non-zero, or we've only got two digits left in the component.
    while (digitBuffer[redIndex] == '0' && digitBuffer[greenIndex] == '0' && digitBuffer[blueIndex] == '0' && (componentLength - redIndex) > 2) {
        redIndex++;
        greenIndex++;
        blueIndex++;
    }
    ASSERT(redIndex + 1 < componentLength);
    ASSERT(greenIndex >= componentLength);
    ASSERT(greenIndex + 1 < componentLength * 2);
    ASSERT(blueIndex >= componentLength * 2);
    ASSERT(blueIndex + 1 < digitBuffer.size());
    return String::format("#%c%c%c%c%c%c", digitBuffer[redIndex], digitBuffer[redIndex + 1], digitBuffer[greenIndex], digitBuffer[greenIndex + 1], digitBuffer[blueIndex], digitBuffer[blueIndex + 1]);   
}

// Color parsing that matches HTML's "rules for parsing a legacy color value"
void StyledElement::addCSSColor(int id, const String& attributeValue)
{
    // An empty string doesn't apply a color. (One containing only whitespace does, which is why this check occurs before stripping.)
    if (attributeValue.isEmpty()) {
        removeCSSProperty(id);
        return;
    }

    String colorString = attributeValue.stripWhiteSpace();

    // "transparent" doesn't apply a color either.
    if (equalIgnoringCase(colorString, "transparent")) {
        removeCSSProperty(id);
        return;
    }

    // If the string is a named CSS color or a 3/6-digit hex color, use that.
    Color parsedColor(colorString);
    if (parsedColor.isValid()) {
        addCSSProperty(id, colorString);
        return;
    }

    addCSSProperty(id, parseColorStringWithCrazyLegacyRules(colorString));
}

void StyledElement::copyNonAttributeProperties(const Element* sourceElement)
{
    ASSERT(sourceElement);
    ASSERT(sourceElement->isStyledElement());

    const StyledElement* source = static_cast<const StyledElement*>(sourceElement);
    if (!source->inlineStyleDecl())
        return;

    StylePropertySet* inlineStyle = ensureInlineStyleDecl();
    inlineStyle->copyPropertiesFrom(*source->inlineStyleDecl());
    inlineStyle->setStrictParsing(source->inlineStyleDecl()->useStrictParsing());

    setIsStyleAttributeValid(source->isStyleAttributeValid());
    setIsSynchronizingStyleAttribute(source->isSynchronizingStyleAttribute());
    
    Element::copyNonAttributeProperties(sourceElement);
}

void StyledElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    if (StylePropertySet* inlineStyle = inlineStyleDecl())
        inlineStyle->addSubresourceStyleURLs(urls);
}

}
