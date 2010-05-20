/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "CSSStyleSelector.h"
#include "CSSStyleSheet.h"
#include "CSSValueKeywords.h"
#include "Document.h"
#include "HTMLNames.h"
#include "MappedAttribute.h"
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
    if (m_inlineStyleDecl)
        const_cast<StyledElement*>(this)->setAttribute(styleAttr, m_inlineStyleDecl->cssText());
    clearIsSynchronizingStyleAttribute();
}

StyledElement::~StyledElement()
{
    destroyInlineStyleDecl();
}

PassRefPtr<Attribute> StyledElement::createAttribute(const QualifiedName& name, const AtomicString& value)
{
    return MappedAttribute::create(name, value);
}

void StyledElement::createInlineStyleDecl()
{
    m_inlineStyleDecl = CSSMutableStyleDeclaration::create();
    m_inlineStyleDecl->setParent(document()->elementSheet());
    m_inlineStyleDecl->setNode(this);
    m_inlineStyleDecl->setStrictParsing(isHTMLElement() && !document()->inCompatMode());
}

void StyledElement::destroyInlineStyleDecl()
{
    if (m_inlineStyleDecl) {
        m_inlineStyleDecl->setNode(0);
        m_inlineStyleDecl->setParent(0);
        m_inlineStyleDecl = 0;
    }
}

void StyledElement::attributeChanged(Attribute* attr, bool preserveDecls)
{
    if (!attr->isMappedAttribute()) {
        Element::attributeChanged(attr, preserveDecls);
        return;
    }
 
    MappedAttribute* mappedAttr = toMappedAttribute(attr);
    if (mappedAttr->decl() && !preserveDecls) {
        mappedAttr->setDecl(0);
        setNeedsStyleRecalc();
        if (namedAttrMap)
            mappedAttributes()->declRemoved();
    }

    bool checkDecl = true;
    MappedAttributeEntry entry;
    bool needToParse = mapToEntry(attr->name(), entry);
    if (preserveDecls) {
        if (mappedAttr->decl()) {
            setNeedsStyleRecalc();
            if (namedAttrMap)
                mappedAttributes()->declAdded();
            checkDecl = false;
        }
    }
    else if (!attr->isNull() && entry != eNone) {
        CSSMappedAttributeDeclaration* decl = getMappedAttributeDecl(entry, attr);
        if (decl) {
            mappedAttr->setDecl(decl);
            setNeedsStyleRecalc();
            if (namedAttrMap)
                mappedAttributes()->declAdded();
            checkDecl = false;
        } else
            needToParse = true;
    }

    // parseMappedAttribute() might create a CSSMappedAttributeDeclaration on the attribute.  
    // Normally we would be concerned about reseting the parent of those declarations in StyledElement::didMoveToNewOwnerDocument().
    // But currently we always clear its parent and node below when adding it to the decl table.  
    // If that changes for some reason moving between documents will be buggy.
    // webarchive/adopt-attribute-styled-node-webarchive.html should catch any resulting crashes.
    if (needToParse)
        parseMappedAttribute(mappedAttr);

    if (entry == eNone)
        recalcStyleIfNeededAfterAttributeChanged(attr);

    if (checkDecl && mappedAttr->decl()) {
        // Add the decl to the table in the appropriate spot.
        setMappedAttributeDecl(entry, attr, mappedAttr->decl());
        mappedAttr->decl()->setMappedState(entry, attr->name(), attr->value());
        mappedAttr->decl()->setParent(0);
        mappedAttr->decl()->setNode(0);
        if (namedAttrMap)
            mappedAttributes()->declAdded();
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
        if (!isClassWhitespace(characters[i]))
            break;
    }
    setHasClass(i < length);
    if (namedAttrMap) {
        if (i < length)
            mappedAttributes()->setClass(newClassString);
        else
            mappedAttributes()->clearClass();
    }
    setNeedsStyleRecalc();
    dispatchSubtreeModifiedEvent();
}

void StyledElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == idAttributeName()) {
        // unique id
        setHasID(!attr->isNull());
        if (namedAttrMap) {
            if (attr->isNull())
                namedAttrMap->setID(nullAtom);
            else if (document()->inCompatMode())
                namedAttrMap->setID(attr->value().lower());
            else
                namedAttrMap->setID(attr->value());
        }
        setNeedsStyleRecalc();
    } else if (attr->name() == classAttr)
        classAttributeChanged(attr->value());
    else if (attr->name() == styleAttr) {
        if (attr->isNull())
            destroyInlineStyleDecl();
        else
            getInlineStyleDecl()->parseDeclaration(attr->value());
        setIsStyleAttributeValid();
        setNeedsStyleRecalc();
    }
}

CSSMutableStyleDeclaration* StyledElement::getInlineStyleDecl()
{
    if (!m_inlineStyleDecl)
        createInlineStyleDecl();
    return m_inlineStyleDecl.get();
}

CSSStyleDeclaration* StyledElement::style()
{
    return getInlineStyleDecl();
}

void StyledElement::addCSSProperty(MappedAttribute* attr, int id, const String &value)
{
    if (!attr->decl()) createMappedDecl(attr);
    attr->decl()->setProperty(id, value, false);
}

void StyledElement::addCSSProperty(MappedAttribute* attr, int id, int value)
{
    if (!attr->decl()) createMappedDecl(attr);
    attr->decl()->setProperty(id, value, false);
}

void StyledElement::addCSSImageProperty(MappedAttribute* attr, int id, const String& url)
{
    if (!attr->decl()) createMappedDecl(attr);
    attr->decl()->setImageProperty(id, url, false);
}

void StyledElement::addCSSLength(MappedAttribute* attr, int id, const String &value)
{
    // FIXME: This function should not spin up the CSS parser, but should instead just figure out the correct
    // length unit and make the appropriate parsed value.
    if (!attr->decl())
        createMappedDecl(attr);

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
            attr->decl()->setLengthProperty(id, v->substring(0, l), false);
            return;
        }
    }
    
    attr->decl()->setLengthProperty(id, value, false);
}

/* color parsing that tries to match as close as possible IE 6. */
void StyledElement::addCSSColor(MappedAttribute* attr, int id, const String& c)
{
    // this is the only case no color gets applied in IE.
    if (!c.length())
        return;

    if (!attr->decl())
        createMappedDecl(attr);
    
    if (attr->decl()->setProperty(id, c, false))
        return;
    
    String color = c;
    // not something that fits the specs.
    
    // we're emulating IEs color parser here. It maps transparent to black, otherwise it tries to build a rgb value
    // out of everything you put in. The algorithm is experimentally determined, but seems to work for all test cases I have.
    
    // the length of the color value is rounded up to the next
    // multiple of 3. each part of the rgb triple then gets one third
    // of the length.
    //
    // Each triplet is parsed byte by byte, mapping
    // each number to a hex value (0-9a-fA-F to their values
    // everything else to 0).
    //
    // The highest non zero digit in all triplets is remembered, and
    // used as a normalization point to normalize to values between 0
    // and 255.
    
    if (!equalIgnoringCase(color, "transparent")) {
        if (color[0] == '#')
            color.remove(0, 1);
        int basicLength = (color.length() + 2) / 3;
        if (basicLength > 1) {
            // IE ignores colors with three digits or less
            int colors[3] = { 0, 0, 0 };
            int component = 0;
            int pos = 0;
            int maxDigit = basicLength-1;
            while (component < 3) {
                // search forward for digits in the string
                int numDigits = 0;
                while (pos < (int)color.length() && numDigits < basicLength) {
                    colors[component] <<= 4;
                    if (isASCIIHexDigit(color[pos])) {
                        colors[component] += toASCIIHexValue(color[pos]);
                        maxDigit = min(maxDigit, numDigits);
                    }
                    numDigits++;
                    pos++;
                }
                while (numDigits++ < basicLength)
                    colors[component] <<= 4;
                component++;
            }
            maxDigit = basicLength - maxDigit;
            
            // normalize to 00-ff. The highest filled digit counts, minimum is 2 digits
            maxDigit -= 2;
            colors[0] >>= 4 * maxDigit;
            colors[1] >>= 4 * maxDigit;
            colors[2] >>= 4 * maxDigit;
            
            color = String::format("#%02x%02x%02x", colors[0], colors[1], colors[2]);
            if (attr->decl()->setProperty(id, color, false))
                return;
        }
    }
    attr->decl()->setProperty(id, CSSValueBlack, false);
}

void StyledElement::createMappedDecl(MappedAttribute* attr)
{
    RefPtr<CSSMappedAttributeDeclaration> decl = CSSMappedAttributeDeclaration::create();
    attr->setDecl(decl);
    decl->setParent(document()->elementSheet());
    decl->setNode(this);
    decl->setStrictParsing(false); // Mapped attributes are just always quirky.
}

// Paul Hsieh's SuperFastHash
// http://www.azillionmonkeys.com/qed/hash.html
unsigned MappedAttributeHash::hash(const MappedAttributeKey& key)
{
    uint32_t hash = WTF::stringHashingStartValue;
    uint32_t tmp;

    const uint16_t* p;

    p = reinterpret_cast<const uint16_t*>(&key.name);
    hash += p[0];
    tmp = (p[1] << 11) ^ hash;
    hash = (hash << 16) ^ tmp;
    hash += hash >> 11;
    ASSERT(sizeof(key.name) == 4 || sizeof(key.name) == 8);
    if (sizeof(key.name) == 8) {
        p += 2;
        hash += p[0];
        tmp = (p[1] << 11) ^ hash;
        hash = (hash << 16) ^ tmp;
        hash += hash >> 11;
    }

    p = reinterpret_cast<const uint16_t*>(&key.value);
    hash += p[0];
    tmp = (p[1] << 11) ^ hash;
    hash = (hash << 16) ^ tmp;
    hash += hash >> 11;
    ASSERT(sizeof(key.value) == 4 || sizeof(key.value) == 8);
    if (sizeof(key.value) == 8) {
        p += 2;
        hash += p[0];
        tmp = (p[1] << 11) ^ hash;
        hash = (hash << 16) ^ tmp;
        hash += hash >> 11;
    }

    // Handle end case
    hash += key.type;
    hash ^= hash << 11;
    hash += hash >> 17;

    // Force "avalanching" of final 127 bits
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 2;
    hash += hash >> 15;
    hash ^= hash << 10;

    // This avoids ever returning a hash code of 0, since that is used to
    // signal "hash not computed yet", using a value that is likely to be
    // effectively the same as 0 when the low bits are masked
    if (hash == 0)
        hash = 0x80000000;

    return hash;
}

void StyledElement::copyNonAttributeProperties(const Element *sourceElement)
{
    const StyledElement* source = static_cast<const StyledElement*>(sourceElement);
    if (!source->m_inlineStyleDecl)
        return;

    *getInlineStyleDecl() = *source->m_inlineStyleDecl;
    setIsStyleAttributeValid(source->isStyleAttributeValid());
    setIsSynchronizingStyleAttribute(source->isSynchronizingStyleAttribute());
    
    Element::copyNonAttributeProperties(sourceElement);
}

void StyledElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    if (CSSMutableStyleDeclaration* style = inlineStyleDecl())
        style->addSubresourceStyleURLs(urls);
}


void StyledElement::didMoveToNewOwnerDocument()
{
    if (m_inlineStyleDecl)
        m_inlineStyleDecl->setParent(document()->elementSheet());

    Element::didMoveToNewOwnerDocument();
}

}
