/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2011, 2012 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "HTMLCollection.h"

#include "ElementTraversal.h"
#include "HTMLDocument.h"
#include "HTMLNameCollection.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLOptionElement.h"
#include "NodeRareData.h"

namespace WebCore {

using namespace HTMLNames;

static bool shouldOnlyIncludeDirectChildren(CollectionType type)
{
    switch (type) {
    case DocAll:
    case DocAnchors:
    case DocApplets:
    case DocEmbeds:
    case DocForms:
    case DocImages:
    case DocLinks:
    case DocScripts:
    case DocumentNamedItems:
    case MapAreas:
    case TableRows:
    case SelectOptions:
    case SelectedOptions:
    case DataListOptions:
    case WindowNamedItems:
    case FormControls:
        return false;
    case NodeChildren:
    case TRCells:
    case TSectionRows:
    case TableTBodies:
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

inline auto HTMLCollection::rootTypeFromCollectionType(CollectionType type) -> RootType
{
    switch (type) {
    case DocImages:
    case DocApplets:
    case DocEmbeds:
    case DocForms:
    case DocLinks:
    case DocAnchors:
    case DocScripts:
    case DocAll:
    case WindowNamedItems:
    case DocumentNamedItems:
    case FormControls:
        return HTMLCollection::IsRootedAtDocument;
    case NodeChildren:
    case TableTBodies:
    case TSectionRows:
    case TableRows:
    case TRCells:
    case SelectOptions:
    case SelectedOptions:
    case DataListOptions:
    case MapAreas:
        return HTMLCollection::IsRootedAtNode;
    }
    ASSERT_NOT_REACHED();
    return HTMLCollection::IsRootedAtNode;
}

static NodeListInvalidationType invalidationTypeExcludingIdAndNameAttributes(CollectionType type)
{
    switch (type) {
    case DocImages:
    case DocEmbeds:
    case DocForms:
    case DocScripts:
    case DocAll:
    case NodeChildren:
    case TableTBodies:
    case TSectionRows:
    case TableRows:
    case TRCells:
    case SelectOptions:
    case MapAreas:
        return DoNotInvalidateOnAttributeChanges;
    case DocApplets:
    case SelectedOptions:
    case DataListOptions:
        // FIXME: We can do better some day.
        return InvalidateOnAnyAttrChange;
    case DocAnchors:
        return InvalidateOnNameAttrChange;
    case DocLinks:
        return InvalidateOnHRefAttrChange;
    case WindowNamedItems:
        return InvalidateOnIdNameAttrChange;
    case DocumentNamedItems:
        return InvalidateOnIdNameAttrChange;
    case FormControls:
        return InvalidateForFormControls;
    }
    ASSERT_NOT_REACHED();
    return DoNotInvalidateOnAttributeChanges;
}

HTMLCollection::HTMLCollection(ContainerNode& ownerNode, CollectionType type, ElementTraversalType traversalType)
    : m_ownerNode(ownerNode)
    , m_indexCache(*this)
    , m_collectionType(type)
    , m_invalidationType(invalidationTypeExcludingIdAndNameAttributes(type))
    , m_rootType(rootTypeFromCollectionType(type))
    , m_shouldOnlyIncludeDirectChildren(shouldOnlyIncludeDirectChildren(type))
    , m_usesCustomForwardOnlyTraversal(traversalType == CustomForwardOnlyTraversal)
{
    ASSERT(m_rootType == static_cast<unsigned>(rootTypeFromCollectionType(type)));
    ASSERT(m_invalidationType == static_cast<unsigned>(invalidationTypeExcludingIdAndNameAttributes(type)));
    ASSERT(m_collectionType == static_cast<unsigned>(type));
}

Ref<HTMLCollection> HTMLCollection::create(ContainerNode& base, CollectionType type)
{
    return adoptRef(*new HTMLCollection(base, type));
}

HTMLCollection::~HTMLCollection()
{
    if (m_indexCache.hasValidCache(*this))
        document().unregisterCollection(*this);
    if (hasNamedElementCache())
        document().collectionWillClearIdNameMap(*this);
    // HTMLNameCollection removes cache by itself.
    if (type() != WindowNamedItems && type() != DocumentNamedItems)
        ownerNode().nodeLists()->removeCachedCollection(this);
}

ContainerNode& HTMLCollection::rootNode() const
{
    if (isRootedAtDocument() && ownerNode().inDocument())
        return ownerNode().document();

    return ownerNode();
}

static inline bool isMatchingHTMLElement(const HTMLCollection& collection, HTMLElement& element)
{
    switch (collection.type()) {
    case DocImages:
        return element.hasTagName(imgTag);
    case DocScripts:
        return element.hasTagName(scriptTag);
    case DocForms:
        return element.hasTagName(formTag);
    case TableTBodies:
        return element.hasTagName(tbodyTag);
    case TRCells:
        return element.hasTagName(tdTag) || element.hasTagName(thTag);
    case TSectionRows:
        return element.hasTagName(trTag);
    case SelectOptions:
        return element.hasTagName(optionTag);
    case SelectedOptions:
        return is<HTMLOptionElement>(element) && downcast<HTMLOptionElement>(element).selected();
    case DataListOptions:
        if (is<HTMLOptionElement>(element)) {
            HTMLOptionElement& option = downcast<HTMLOptionElement>(element);
            if (!option.isDisabledFormControl() && !option.value().isEmpty())
                return true;
        }
        return false;
    case MapAreas:
        return element.hasTagName(areaTag);
    case DocApplets:
        return is<HTMLAppletElement>(element) || (is<HTMLObjectElement>(element) && downcast<HTMLObjectElement>(element).containsJavaApplet());
    case DocEmbeds:
        return element.hasTagName(embedTag);
    case DocLinks:
        return (element.hasTagName(aTag) || element.hasTagName(areaTag)) && element.fastHasAttribute(hrefAttr);
    case DocAnchors:
        return element.hasTagName(aTag) && element.fastHasAttribute(nameAttr);
    case DocumentNamedItems:
        return downcast<DocumentNameCollection>(collection).elementMatches(element);
    case DocAll:
    case NodeChildren:
    case WindowNamedItems:
    case FormControls:
    case TableRows:
        break;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static inline bool isMatchingElement(const HTMLCollection& collection, Element& element)
{
    // Collection types that deal with any type of Elements, not just HTMLElements.
    switch (collection.type()) {
    case DocAll:
    case NodeChildren:
        return true;
    case WindowNamedItems:
        return downcast<WindowNameCollection>(collection).elementMatches(element);
    default:
        // Collection types that only deal with HTMLElements.
        return is<HTMLElement>(element) && isMatchingHTMLElement(collection, downcast<HTMLElement>(element));
    }
}

static inline Element* previousElement(ContainerNode& base, Element& element, bool onlyIncludeDirectChildren)
{
    return onlyIncludeDirectChildren ? ElementTraversal::previousSibling(element) : ElementTraversal::previous(element, &base);
}

ALWAYS_INLINE Element* HTMLCollection::iterateForPreviousElement(Element* element) const
{
    bool onlyIncludeDirectChildren = m_shouldOnlyIncludeDirectChildren;
    ContainerNode& rootNode = this->rootNode();
    for (; element; element = previousElement(rootNode, *element, onlyIncludeDirectChildren)) {
        if (isMatchingElement(*this, *element))
            return element;
    }
    return nullptr;
}

static inline Element* firstMatchingElement(const HTMLCollection& collection, ContainerNode& root)
{
    Element* element = ElementTraversal::firstWithin(root);
    while (element && !isMatchingElement(collection, *element))
        element = ElementTraversal::next(*element, &root);
    return element;
}

static inline Element* nextMatchingElement(const HTMLCollection& collection, Element& element, ContainerNode& root)
{
    Element* next = &element;
    do {
        next = ElementTraversal::next(*next, &root);
    } while (next && !isMatchingElement(collection, *next));
    return next;
}

unsigned HTMLCollection::length() const
{
    return m_indexCache.nodeCount(*this);
}

Element* HTMLCollection::item(unsigned offset) const
{
    return m_indexCache.nodeAt(*this, offset);
}

static inline bool nameShouldBeVisibleInDocumentAll(HTMLElement& element)
{
    // The document.all collection returns only certain types of elements by name,
    // although it returns any type of element by id.
    return element.hasTagName(appletTag)
        || element.hasTagName(embedTag)
        || element.hasTagName(formTag)
        || element.hasTagName(imgTag)
        || element.hasTagName(inputTag)
        || element.hasTagName(objectTag)
        || element.hasTagName(selectTag);
}

static inline bool nameShouldBeVisibleInDocumentAll(Element& element)
{
    return is<HTMLElement>(element) && nameShouldBeVisibleInDocumentAll(downcast<HTMLElement>(element));
}

static inline Element* firstMatchingChildElement(const HTMLCollection& nodeList, ContainerNode& root)
{
    Element* element = ElementTraversal::firstWithin(root);
    while (element && !isMatchingElement(nodeList, *element))
        element = ElementTraversal::nextSibling(*element);
    return element;
}

static inline Element* nextMatchingSiblingElement(const HTMLCollection& nodeList, Element& element)
{
    Element* next = &element;
    do {
        next = ElementTraversal::nextSibling(*next);
    } while (next && !isMatchingElement(nodeList, *next));
    return next;
}

inline bool HTMLCollection::usesCustomForwardOnlyTraversal() const
{
    return m_usesCustomForwardOnlyTraversal;
}

inline Element* HTMLCollection::firstElement(ContainerNode& root) const
{
    if (usesCustomForwardOnlyTraversal())
        return customElementAfter(nullptr);
    if (m_shouldOnlyIncludeDirectChildren)
        return firstMatchingChildElement(*this, root);
    return firstMatchingElement(*this, root);
}

inline Element* HTMLCollection::traverseForward(Element& current, unsigned count, unsigned& traversedCount, ContainerNode& root) const
{
    Element* element = &current;
    if (usesCustomForwardOnlyTraversal()) {
        for (traversedCount = 0; traversedCount < count; ++traversedCount) {
            element = customElementAfter(element);
            if (!element)
                return nullptr;
        }
    } else if (m_shouldOnlyIncludeDirectChildren) {
        for (traversedCount = 0; traversedCount < count; ++traversedCount) {
            element = nextMatchingSiblingElement(*this, *element);
            if (!element)
                return nullptr;
        }
    } else {
        for (traversedCount = 0; traversedCount < count; ++traversedCount) {
            element = nextMatchingElement(*this, *element, root);
            if (!element)
                return nullptr;
        }
    }
    return element;
}

Element* HTMLCollection::collectionBegin() const
{
    return firstElement(rootNode());
}

Element* HTMLCollection::collectionLast() const
{
    // FIXME: This should be optimized similarly to the forward case.
    auto& root = rootNode();
    Element* last = m_shouldOnlyIncludeDirectChildren ? ElementTraversal::lastChild(root) : ElementTraversal::lastWithin(root);
    return iterateForPreviousElement(last);
}

void HTMLCollection::collectionTraverseForward(Element*& current, unsigned count, unsigned& traversedCount) const
{
    current = traverseForward(*current, count, traversedCount, rootNode());
}

void HTMLCollection::collectionTraverseBackward(Element*& current, unsigned count) const
{
    // FIXME: This should be optimized similarly to the forward case.
    if (m_shouldOnlyIncludeDirectChildren) {
        for (; count && current; --count)
            current = iterateForPreviousElement(ElementTraversal::previousSibling(*current));
        return;
    }
    auto& root = rootNode();
    for (; count && current; --count)
        current = iterateForPreviousElement(ElementTraversal::previous(*current, &root));
}

void HTMLCollection::invalidateCache(Document& document) const
{
    if (m_indexCache.hasValidCache(*this)) {
        document.unregisterCollection(const_cast<HTMLCollection&>(*this));
        m_indexCache.invalidate(*this);
    }
    if (hasNamedElementCache())
        invalidateNamedElementCache(document);
}

void HTMLCollection::invalidateNamedElementCache(Document& document) const
{
    ASSERT(hasNamedElementCache());
    document.collectionWillClearIdNameMap(*this);
    m_namedElementCache = nullptr;
}

Element* HTMLCollection::namedItem(const AtomicString& name) const
{
    // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
    // This method first searches for an object with a matching id
    // attribute. If a match is not found, the method then searches for an
    // object with a matching name attribute, but only on those elements
    // that are allowed a name attribute.

    if (name.isEmpty())
        return nullptr;

    ContainerNode& root = rootNode();
    if (!usesCustomForwardOnlyTraversal() && root.isInTreeScope()) {
        Element* candidate = nullptr;

        TreeScope& treeScope = root.treeScope();
        if (treeScope.hasElementWithId(*name.impl())) {
            if (!treeScope.containsMultipleElementsWithId(name))
                candidate = treeScope.getElementById(name);
        } else if (treeScope.hasElementWithName(*name.impl())) {
            if (!treeScope.containsMultipleElementsWithName(name)) {
                candidate = treeScope.getElementByName(name);
                if (candidate && type() == DocAll && !nameShouldBeVisibleInDocumentAll(*candidate))
                    candidate = nullptr;
            }
        } else
            return nullptr;

        if (candidate && isMatchingElement(*this, *candidate)) {
            if (m_shouldOnlyIncludeDirectChildren ? candidate->parentNode() == &root : candidate->isDescendantOf(&root))
                return candidate;
        }
    }

    // The pathological case. We need to walk the entire subtree.
    updateNamedElementCache();
    ASSERT(m_namedElementCache);

    if (const Vector<Element*>* idResults = m_namedElementCache->findElementsWithId(name)) {
        if (idResults->size())
            return idResults->at(0);
    }

    if (const Vector<Element*>* nameResults = m_namedElementCache->findElementsWithName(name)) {
        if (nameResults->size())
            return nameResults->at(0);
    }

    return nullptr;
}

void HTMLCollection::updateNamedElementCache() const
{
    if (hasNamedElementCache())
        return;

    auto cache = std::make_unique<CollectionNamedElementCache>();

    unsigned size = m_indexCache.nodeCount(*this);
    for (unsigned i = 0; i < size; ++i) {
        Element& element = *m_indexCache.nodeAt(*this, i);
        const AtomicString& id = element.getIdAttribute();
        if (!id.isEmpty())
            cache->appendToIdCache(id, element);
        if (!is<HTMLElement>(element))
            continue;
        const AtomicString& name = element.getNameAttribute();
        if (!name.isEmpty() && id != name && (type() != DocAll || nameShouldBeVisibleInDocumentAll(downcast<HTMLElement>(element))))
            cache->appendToNameCache(name, element);
    }

    setNamedItemCache(WTF::move(cache));
}

bool HTMLCollection::hasNamedItem(const AtomicString& name) const
{
    // FIXME: We can do better when there are multiple elements of the same name.
    return namedItem(name);
}

Vector<Ref<Element>> HTMLCollection::namedItems(const AtomicString& name) const
{
    // FIXME: This non-virtual function can't possibly be doing the correct thing for
    // any derived class that overrides the virtual namedItem function.

    Vector<Ref<Element>> elements;

    if (name.isEmpty())
        return elements;

    updateNamedElementCache();
    ASSERT(m_namedElementCache);

    auto* elementsWithId = m_namedElementCache->findElementsWithId(name);
    auto* elementsWithName = m_namedElementCache->findElementsWithName(name);

    elements.reserveInitialCapacity((elementsWithId ? elementsWithId->size() : 0) + (elementsWithName ? elementsWithName->size() : 0));

    if (elementsWithId) {
        for (auto& element : *elementsWithId)
            elements.uncheckedAppend(*element);
    }
    if (elementsWithName) {
        for (auto& element : *elementsWithName)
            elements.uncheckedAppend(*element);
    }

    return elements;
}

PassRefPtr<NodeList> HTMLCollection::tags(const String& name)
{
    return ownerNode().getElementsByTagName(name);
}

Element* HTMLCollection::customElementAfter(Element*) const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

} // namespace WebCore
