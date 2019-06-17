/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "SelectorFilter.h"

#include "CSSSelector.h"
#include "ShadowRoot.h"
#include "StyledElement.h"

namespace WebCore {

// Salt to separate otherwise identical string hashes so a class-selector like .article won't match <article> elements.
enum { TagNameSalt = 13, IdSalt = 17, ClassSalt = 19, AttributeSalt = 23 };

static bool isExcludedAttribute(const AtomString& name)
{
    return name == HTMLNames::classAttr->localName() || name == HTMLNames::idAttr->localName() || name == HTMLNames::styleAttr->localName();
}

static inline void collectElementIdentifierHashes(const Element& element, Vector<unsigned, 4>& identifierHashes)
{
    AtomString tagLowercaseLocalName = element.localName().convertToASCIILowercase();
    identifierHashes.append(tagLowercaseLocalName.impl()->existingHash() * TagNameSalt);

    auto& id = element.idForStyleResolution();
    if (!id.isNull())
        identifierHashes.append(id.impl()->existingHash() * IdSalt);

    if (element.hasClass()) {
        const SpaceSplitString& classNames = element.classNames();
        size_t count = classNames.size();
        for (size_t i = 0; i < count; ++i)
            identifierHashes.append(classNames[i].impl()->existingHash() * ClassSalt);
    }
    
    if (element.hasAttributesWithoutUpdate()) {
        for (auto& attribute : element.attributesIterator()) {
            auto attributeName = element.isHTMLElement() ? attribute.localName() : attribute.localName().convertToASCIILowercase();
            if (isExcludedAttribute(attributeName))
                continue;
            identifierHashes.append(attributeName.impl()->existingHash() * AttributeSalt);
        }
    }
}

bool SelectorFilter::parentStackIsConsistent(const ContainerNode* parentNode) const
{
    if (!parentNode || is<Document>(parentNode) || is<ShadowRoot>(parentNode))
        return m_parentStack.isEmpty();

    return !m_parentStack.isEmpty() && m_parentStack.last().element == parentNode;
}

void SelectorFilter::initializeParentStack(Element& parent)
{
    Vector<Element*, 20> ancestors;
    for (auto* ancestor = &parent; ancestor; ancestor = ancestor->parentElement())
        ancestors.append(ancestor);
    for (unsigned i = ancestors.size(); i--;)
        pushParent(ancestors[i]);
}

void SelectorFilter::pushParent(Element* parent)
{
    ASSERT(m_parentStack.isEmpty() || m_parentStack.last().element == parent->parentElement());
    ASSERT(!m_parentStack.isEmpty() || !parent->parentElement());
    m_parentStack.append(ParentStackFrame(parent));
    ParentStackFrame& parentFrame = m_parentStack.last();
    // Mix tags, class names and ids into some sort of weird bouillabaisse.
    // The filter is used for fast rejection of child and descendant selectors.
    collectElementIdentifierHashes(*parent, parentFrame.identifierHashes);
    size_t count = parentFrame.identifierHashes.size();
    for (size_t i = 0; i < count; ++i)
        m_ancestorIdentifierFilter.add(parentFrame.identifierHashes[i]);
}

void SelectorFilter::pushParentInitializingIfNeeded(Element& parent)
{
    if (UNLIKELY(m_parentStack.isEmpty())) {
        initializeParentStack(parent);
        return;
    }
    pushParent(&parent);
}

void SelectorFilter::popParent()
{
    ASSERT(!m_parentStack.isEmpty());
    const ParentStackFrame& parentFrame = m_parentStack.last();
    size_t count = parentFrame.identifierHashes.size();
    for (size_t i = 0; i < count; ++i)
        m_ancestorIdentifierFilter.remove(parentFrame.identifierHashes[i]);
    m_parentStack.removeLast();
    if (m_parentStack.isEmpty()) {
        ASSERT(m_ancestorIdentifierFilter.likelyEmpty());
        m_ancestorIdentifierFilter.clear();
    }
}

void SelectorFilter::popParentsUntil(Element* parent)
{
    while (!m_parentStack.isEmpty()) {
        if (parent && m_parentStack.last().element == parent)
            return;
        popParent();
    }
}

struct CollectedSelectorHashes {
    using HashVector = Vector<unsigned, 8>;
    HashVector ids;
    HashVector classes;
    HashVector tags;
    HashVector attributes;
};

static inline void collectSimpleSelectorHash(CollectedSelectorHashes& collectedHashes, const CSSSelector& selector)
{
    switch (selector.match()) {
    case CSSSelector::Id:
        if (!selector.value().isEmpty())
            collectedHashes.ids.append(selector.value().impl()->existingHash() * IdSalt);
        break;
    case CSSSelector::Class:
        if (!selector.value().isEmpty())
            collectedHashes.classes.append(selector.value().impl()->existingHash() * ClassSalt);
        break;
    case CSSSelector::Tag: {
        auto& tagLowercaseLocalName = selector.tagLowercaseLocalName();
        if (tagLowercaseLocalName != starAtom())
            collectedHashes.tags.append(tagLowercaseLocalName.impl()->existingHash() * TagNameSalt);
        break;
    }
    case CSSSelector::Exact:
    case CSSSelector::Set:
    case CSSSelector::List:
    case CSSSelector::Hyphen:
    case CSSSelector::Contain:
    case CSSSelector::Begin:
    case CSSSelector::End: {
        auto attributeName = selector.attributeCanonicalLocalName().convertToASCIILowercase();
        if (!isExcludedAttribute(attributeName))
            collectedHashes.attributes.append(attributeName.impl()->existingHash() * AttributeSalt);
        break;
    }
    default:
        break;
    }
}

static CollectedSelectorHashes collectSelectorHashes(const CSSSelector& rightmostSelector)
{
    CollectedSelectorHashes collectedHashes;

    auto* selector = &rightmostSelector;
    auto relation = selector->relation();

    // Skip the topmost selector. It is handled quickly by the rule hashes.
    bool skipOverSubselectors = true;
    for (selector = selector->tagHistory(); selector; selector = selector->tagHistory()) {
        // Only collect identifiers that match ancestors.
        switch (relation) {
        case CSSSelector::Subselector:
            if (!skipOverSubselectors)
                collectSimpleSelectorHash(collectedHashes, *selector);
            break;
        case CSSSelector::DirectAdjacent:
        case CSSSelector::IndirectAdjacent:
        case CSSSelector::ShadowDescendant:
            skipOverSubselectors = true;
            break;
        case CSSSelector::DescendantSpace:
        case CSSSelector::Child:
            skipOverSubselectors = false;
            collectSimpleSelectorHash(collectedHashes, *selector);
            break;
        }
        relation = selector->relation();
    }
    return collectedHashes;
}

static SelectorFilter::Hashes chooseSelectorHashesForFilter(const CollectedSelectorHashes& collectedSelectorHashes)
{
    SelectorFilter::Hashes resultHashes;
    unsigned index = 0;

    auto addIfNew = [&] (unsigned hash) {
        for (unsigned i = 0; i < index; ++i) {
            if (resultHashes[i] == hash)
                return;
        }
        resultHashes[index++] = hash;
    };

    auto copyHashes = [&] (auto& hashes) {
        for (auto& hash : hashes) {
            addIfNew(hash);
            if (index == resultHashes.size())
                return true;
        }
        return false;
    };

    // There is a limited number of slots. Prefer more specific selector types.
    if (copyHashes(collectedSelectorHashes.ids))
        return resultHashes;
    if (copyHashes(collectedSelectorHashes.attributes))
        return resultHashes;
    if (copyHashes(collectedSelectorHashes.classes))
        return resultHashes;
    if (copyHashes(collectedSelectorHashes.tags))
        return resultHashes;

    // Null-terminate if not full.
    resultHashes[index] = 0;
    return resultHashes;
}

SelectorFilter::Hashes SelectorFilter::collectHashes(const CSSSelector& selector)
{
    auto hashes = collectSelectorHashes(selector);
    return chooseSelectorHashesForFilter(hashes);
}

}

