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
#include "CSSMutableStyleDeclaration.h"
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
#include <wtf/HashFunctions.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

struct MappedAttributeKey {
    uint16_t type;
    StringImpl* name;
    StringImpl* value;
    MappedAttributeKey(MappedAttributeEntry t = eNone, StringImpl* n = 0, StringImpl* v = 0)
        : type(t), name(n), value(v) { }
};

static inline bool operator==(const MappedAttributeKey& a, const MappedAttributeKey& b)
    { return a.type == b.type && a.name == b.name && a.value == b.value; } 

struct MappedAttributeKeyTraits : WTF::GenericHashTraits<MappedAttributeKey> {
    static const bool emptyValueIsZero = true;
    static const bool needsDestruction = false;
    static void constructDeletedValue(MappedAttributeKey& slot) { slot.type = eLastEntry; }
    static bool isDeletedValue(const MappedAttributeKey& value) { return value.type == eLastEntry; }
};

struct MappedAttributeHash {
    static unsigned hash(const MappedAttributeKey&);
    static bool equal(const MappedAttributeKey& a, const MappedAttributeKey& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

typedef HashMap<MappedAttributeKey, CSSMappedAttributeDeclaration*, MappedAttributeHash, MappedAttributeKeyTraits> MappedAttributeDecls;

static MappedAttributeDecls* mappedAttributeDecls = 0;

CSSMappedAttributeDeclaration* StyledElement::getMappedAttributeDecl(MappedAttributeEntry entryType, Attribute* attr)
{
    if (!mappedAttributeDecls)
        return 0;
    return mappedAttributeDecls->get(MappedAttributeKey(entryType, attr->name().localName().impl(), attr->value().impl()));
}

CSSMappedAttributeDeclaration* StyledElement::getMappedAttributeDecl(MappedAttributeEntry type, const QualifiedName& name, const AtomicString& value)
{
    if (!mappedAttributeDecls)
        return 0;
    return mappedAttributeDecls->get(MappedAttributeKey(type, name.localName().impl(), value.impl()));
}

void StyledElement::setMappedAttributeDecl(MappedAttributeEntry entryType, Attribute* attr, CSSMappedAttributeDeclaration* decl)
{
    if (!mappedAttributeDecls)
        mappedAttributeDecls = new MappedAttributeDecls;
    mappedAttributeDecls->set(MappedAttributeKey(entryType, attr->name().localName().impl(), attr->value().impl()), decl);
}

void StyledElement::setMappedAttributeDecl(MappedAttributeEntry entryType, const QualifiedName& name, const AtomicString& value, CSSMappedAttributeDeclaration* decl)
{
    if (!mappedAttributeDecls)
        mappedAttributeDecls = new MappedAttributeDecls;
    mappedAttributeDecls->set(MappedAttributeKey(entryType, name.localName().impl(), value.impl()), decl);
}

void StyledElement::removeMappedAttributeDecl(MappedAttributeEntry entryType, const QualifiedName& attrName, const AtomicString& attrValue)
{
    if (!mappedAttributeDecls)
        return;
    mappedAttributeDecls->remove(MappedAttributeKey(entryType, attrName.localName().impl(), attrValue.impl()));
}

void StyledElement::updateStyleAttribute() const
{
    ASSERT(!isStyleAttributeValid());
    setIsStyleAttributeValid();
    setIsSynchronizingStyleAttribute();
    if (CSSMutableStyleDeclaration* inlineStyle = inlineStyleDecl())
        const_cast<StyledElement*>(this)->setAttribute(styleAttr, inlineStyle->asText());
    clearIsSynchronizingStyleAttribute();
}

StyledElement::~StyledElement()
{
    destroyInlineStyleDecl();
}

PassRefPtr<Attribute> StyledElement::createAttribute(const QualifiedName& name, const AtomicString& value)
{
    return Attribute::createMapped(name, value);
}

void StyledElement::createInlineStyleDecl()
{
    ASSERT(!m_inlineStyleDecl);
    m_inlineStyleDecl = CSSMutableStyleDeclaration::createInline(this);
    m_inlineStyleDecl->setStrictParsing(isHTMLElement() && !document()->inQuirksMode());
}

void StyledElement::destroyInlineStyleDecl()
{
    if (!m_inlineStyleDecl)
        return;
    m_inlineStyleDecl->clearParentElement();
    m_inlineStyleDecl = 0;
}

void StyledElement::attributeChanged(Attribute* attr, bool preserveDecls)
{
    if (attr->name() == HTMLNames::nameAttr)
        setHasName(!attr->isNull());

    if (!attr->isMappedAttribute()) {
        Element::attributeChanged(attr, preserveDecls);
        return;
    }
 
    if (attr->decl() && !preserveDecls) {
        attr->setDecl(0);
        setNeedsStyleRecalc();
    }

    bool checkDecl = true;
    MappedAttributeEntry entry;
    bool needToParse = mapToEntry(attr->name(), entry);
    if (preserveDecls) {
        if (attr->decl()) {
            setNeedsStyleRecalc();
            checkDecl = false;
        }
    } else if (!attr->isNull() && entry != eNone) {
        CSSMappedAttributeDeclaration* decl = getMappedAttributeDecl(entry, attr);
        if (decl) {
            attr->setDecl(decl);
            setNeedsStyleRecalc();
            checkDecl = false;
        } else
            needToParse = true;
    }

    // parseMappedAttribute() might create a CSSMappedAttributeDeclaration on the attribute.  
    // Normally we would be concerned about reseting the parent of those declarations in StyledElement::didMoveToNewDocument().
    // But currently we always clear its parent and node below when adding it to the decl table.  
    // If that changes for some reason moving between documents will be buggy.
    // webarchive/adopt-attribute-styled-node-webarchive.html should catch any resulting crashes.
    if (needToParse)
        parseMappedAttribute(attr);

    if (entry == eNone)
        recalcStyleIfNeededAfterAttributeChanged(attr);

    if (checkDecl && attr->decl()) {
        // Add the decl to the table in the appropriate spot.
        setMappedAttributeDecl(entry, attr, attr->decl());
        attr->decl()->setMappedState(entry, attr->name(), attr->value());
    }

    updateAfterAttributeChanged(attr);
}

bool StyledElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    result = eNone;
    if (attrName == styleAttr)
        return !isSynchronizingStyleAttribute();
    return true;
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
        attributes()->setClass(newClassString);
        if (DOMTokenList* classList = optionalClassList())
            static_cast<ClassList*>(classList)->reset(newClassString);
    } else if (attributeMap())
        attributeMap()->clearClass();
    setNeedsStyleRecalc();
    dispatchSubtreeModifiedEvent();
}

void StyledElement::parseMappedAttribute(Attribute* attr)
{
    if (isIdAttributeName(attr->name()))
        idAttributeChanged(attr);
    else if (attr->name() == classAttr)
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

CSSMutableStyleDeclaration* StyledElement::ensureInlineStyleDecl()
{
    if (!m_inlineStyleDecl)
        createInlineStyleDecl();
    return m_inlineStyleDecl.get();
}

CSSStyleDeclaration* StyledElement::style()
{
    return ensureInlineStyleDecl();
}

void StyledElement::removeCSSProperty(Attribute* attribute, int id)
{
    if (!attribute->decl())
        createMappedDecl(attribute);
    attribute->decl()->removeMappedProperty(this, id);
}

void StyledElement::addCSSProperty(Attribute* attribute, int id, const String &value)
{
    if (!attribute->decl())
        createMappedDecl(attribute);
    attribute->decl()->setMappedProperty(this, id, value);
}

void StyledElement::addCSSProperty(Attribute* attribute, int id, int value)
{
    if (!attribute->decl())
        createMappedDecl(attribute);
    attribute->decl()->setMappedProperty(this, id, value);
}

void StyledElement::addCSSImageProperty(Attribute* attribute, int id, const String& url)
{
    if (!attribute->decl())
        createMappedDecl(attribute);
    attribute->decl()->setMappedImageProperty(this, id, url);
}

void StyledElement::addCSSLength(Attribute* attribute, int id, const String &value)
{
    // FIXME: This function should not spin up the CSS parser, but should instead just figure out the correct
    // length unit and make the appropriate parsed value.
    if (!attribute->decl())
        createMappedDecl(attribute);

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
            attribute->decl()->setMappedLengthProperty(this, id, v->substring(0, l));
            return;
        }
    }

    attribute->decl()->setMappedLengthProperty(this, id, value);
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
void StyledElement::addCSSColor(Attribute* attribute, int id, const String& attributeValue)
{
    // An empty string doesn't apply a color. (One containing only whitespace does, which is why this check occurs before stripping.)
    if (attributeValue.isEmpty())
        return;

    String colorString = attributeValue.stripWhiteSpace();

    // "transparent" doesn't apply a color either.
    if (equalIgnoringCase(colorString, "transparent"))
        return;

    if (!attribute->decl())
        createMappedDecl(attribute);

    // If the string is a named CSS color or a 3/6-digit hex color, use that.
    Color parsedColor(colorString);
    if (parsedColor.isValid()) {
        attribute->decl()->setMappedProperty(this, id, colorString);
        return;
    }

    attribute->decl()->setMappedProperty(this, id, parseColorStringWithCrazyLegacyRules(colorString));
}

void StyledElement::createMappedDecl(Attribute* attr)
{
    RefPtr<CSSMappedAttributeDeclaration> decl = CSSMappedAttributeDeclaration::create();
    attr->setDecl(decl);
    ASSERT(!decl->useStrictParsing());
}

unsigned MappedAttributeHash::hash(const MappedAttributeKey& key)
{
    COMPILE_ASSERT(sizeof(key.name) == 4 || sizeof(key.name) == 8, key_name_size);
    COMPILE_ASSERT(sizeof(key.value) == 4 || sizeof(key.value) == 8, key_value_size);

    StringHasher hasher;
    const UChar* data;

    data = reinterpret_cast<const UChar*>(&key.name);
    hasher.addCharacters(data[0], data[1]);
    if (sizeof(key.name) == 8)
        hasher.addCharacters(data[2], data[3]);

    data = reinterpret_cast<const UChar*>(&key.value);
    hasher.addCharacters(data[0], data[1]);
    if (sizeof(key.value) == 8)
        hasher.addCharacters(data[2], data[3]);

    return hasher.hash();
}

void StyledElement::copyNonAttributeProperties(const Element* sourceElement)
{
    ASSERT(sourceElement);
    ASSERT(sourceElement->isStyledElement());

    const StyledElement* source = static_cast<const StyledElement*>(sourceElement);
    if (!source->inlineStyleDecl())
        return;

    CSSMutableStyleDeclaration* inlineStyle = ensureInlineStyleDecl();
    inlineStyle->copyPropertiesFrom(*source->inlineStyleDecl());
    inlineStyle->setStrictParsing(source->inlineStyleDecl()->useStrictParsing());

    setIsStyleAttributeValid(source->isStyleAttributeValid());
    setIsSynchronizingStyleAttribute(source->isSynchronizingStyleAttribute());
    
    Element::copyNonAttributeProperties(sourceElement);
}

void StyledElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    if (!m_inlineStyleDecl)
        return;
    m_inlineStyleDecl->addSubresourceStyleURLs(urls);
}

}
