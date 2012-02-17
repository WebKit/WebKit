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
#include "CSSValuePool.h"
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

void StyledElement::insertedIntoDocument()
{
    Element::insertedIntoDocument();

    if (StylePropertySet* inlineStyle = inlineStyleDecl())
        inlineStyle->setContextStyleSheet(document()->elementSheet());
    if (StylePropertySet* attributeStyle = attributeData() ? attributeData()->attributeStyle() : 0)
        attributeStyle->setContextStyleSheet(document()->elementSheet());
}

void StyledElement::removedFromDocument()
{
    Element::removedFromDocument();

    if (StylePropertySet* inlineStyle = inlineStyleDecl())
        inlineStyle->setContextStyleSheet(0);
    if (StylePropertySet* attributeStyle = attributeData() ? attributeData()->attributeStyle() : 0)
        attributeStyle->setContextStyleSheet(0);
}

void StyledElement::attributeChanged(Attribute* attr)
{
    if (!(attr->name() == styleAttr && isSynchronizingStyleAttribute()))
        parseAttribute(attr);

    if (isPresentationAttribute(attr)) {
        setAttributeStyleDirty();
        setNeedsStyleRecalc(InlineStyleChange);
    }

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
        InspectorInstrumentation::didInvalidateStyleAttr(document(), this);
    }
}

void StyledElement::inlineStyleChanged()
{
    setNeedsStyleRecalc(InlineStyleChange);
    setIsStyleAttributeValid(false);
    InspectorInstrumentation::didInvalidateStyleAttr(document(), this);
}
    
bool StyledElement::setInlineStyleProperty(int propertyID, int value, bool important)
{
    bool changes = ensureInlineStyleDecl()->setProperty(propertyID, value, important);
    if (changes)
        inlineStyleChanged();
    return changes;
}

bool StyledElement::setInlineStyleProperty(int propertyID, double value, CSSPrimitiveValue::UnitTypes unit, bool important)
{
    bool changes = ensureInlineStyleDecl()->setProperty(propertyID, value, unit, important);
    if (changes)
        inlineStyleChanged();
    return changes;
}

bool StyledElement::setInlineStyleProperty(int propertyID, const String& value, bool important)
{
    bool changes = ensureInlineStyleDecl()->setProperty(propertyID, value, important);
    if (changes)
        inlineStyleChanged();
    return changes;
}

bool StyledElement::removeInlineStyleProperty(int propertyID)
{
    bool changes = ensureInlineStyleDecl()->removeProperty(propertyID);
    if (changes)
        inlineStyleChanged();
    return changes;
}

void StyledElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    if (StylePropertySet* inlineStyle = inlineStyleDecl())
        inlineStyle->addSubresourceStyleURLs(urls);
}

void StyledElement::updateAttributeStyle()
{
    RefPtr<StylePropertySet> style = StylePropertySet::create(document()->elementSheet());
    for (unsigned i = 0; i < attributeCount(); ++i) {
        Attribute* attribute = attributeItem(i);
        collectStyleForAttribute(attribute, style.get());
    }
    clearAttributeStyleDirty();

    if (style->isEmpty())
        attributeData()->setAttributeStyle(0);
    else
        attributeData()->setAttributeStyle(style.release());
}

}
