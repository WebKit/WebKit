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

static NodeListRootType rootTypeFromCollectionType(CollectionType type)
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
        return NodeListIsRootedAtDocument;
    case NodeChildren:
    case TableTBodies:
    case TSectionRows:
    case TableRows:
    case TRCells:
    case SelectOptions:
    case SelectedOptions:
    case DataListOptions:
    case MapAreas:
        return NodeListIsRootedAtNode;
    }
    ASSERT_NOT_REACHED();
    return NodeListIsRootedAtNode;
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

PassRefPtr<HTMLCollection> HTMLCollection::create(ContainerNode& base, CollectionType type)
{
    return adoptRef(new HTMLCollection(base, type));
}

HTMLCollection::~HTMLCollection()
{
    if (m_indexCache.hasValidCache())
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

inline bool isMatchingElement(const HTMLCollection& htmlCollection, Element& element)
{
    CollectionType type = htmlCollection.type();
    if (!element.isHTMLElement() && !(type == DocAll || type == NodeChildren || type == WindowNamedItems))
        return false;

    switch (type) {
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
        return element.hasTagName(optionTag) && toHTMLOptionElement(element).selected();
    case DataListOptions:
        if (element.hasTagName(optionTag)) {
            HTMLOptionElement& option = toHTMLOptionElement(element);
            if (!option.isDisabledFormControl() && !option.value().isEmpty())
                return true;
        }
        return false;
    case MapAreas:
        return element.hasTagName(areaTag);
    case DocApplets:
        return element.hasTagName(appletTag) || (element.hasTagName(objectTag) && toHTMLObjectElement(element).containsJavaApplet());
    case DocEmbeds:
        return element.hasTagName(embedTag);
    case DocLinks:
        return (element.hasTagName(aTag) || element.hasTagName(areaTag)) && element.fastHasAttribute(hrefAttr);
    case DocAnchors:
        return element.hasTagName(aTag) && element.fastHasAttribute(nameAttr);
    case DocAll:
    case NodeChildren:
        return true;
    case DocumentNamedItems:
        return static_cast<const DocumentNameCollection&>(htmlCollection).nodeMatches(&element);
    case WindowNamedItems:
        return static_cast<const WindowNameCollection&>(htmlCollection).nodeMatches(&element);
    case FormControls:
    case TableRows:
        break;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static Element* previousElement(ContainerNode& base, Element* previous, bool onlyIncludeDirectChildren)
{
    return onlyIncludeDirectChildren ? ElementTraversal::previousSibling(previous) : ElementTraversal::previous(previous, &base);
}

ALWAYS_INLINE Element* HTMLCollection::iterateForPreviousElement(Element* current) const
{
    bool onlyIncludeDirectChildren = m_shouldOnlyIncludeDirectChildren;
    ContainerNode& rootNode = this->rootNode();
    for (; current; current = previousElement(rootNode, current, onlyIncludeDirectChildren)) {
        if (isMatchingElement(*this, *current))
            return current;
    }
    return nullptr;
}

inline Element* firstMatchingElement(const HTMLCollection& collection, ContainerNode& root)
{
    Element* element = ElementTraversal::firstWithin(&root);
    while (element && !isMatchingElement(collection, *element))
        element = ElementTraversal::next(element, &root);
    return element;
}

inline Element* nextMatchingElement(const HTMLCollection& collection, Element* current, ContainerNode& root)
{
    do {
        current = ElementTraversal::next(current, &root);
    } while (current && !isMatchingElement(collection, *current));
    return current;
}

unsigned HTMLCollection::length() const
{
    return m_indexCache.nodeCount(*this);
}

Node* HTMLCollection::item(unsigned offset) const
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

inline Element* firstMatchingChildElement(const HTMLCollection& nodeList, ContainerNode& root)
{
    Element* element = ElementTraversal::firstWithin(&root);
    while (element && !isMatchingElement(nodeList, *element))
        element = ElementTraversal::nextSibling(element);
    return element;
}

inline Element* nextMatchingSiblingElement(const HTMLCollection& nodeList, Element* current)
{
    do {
        current = ElementTraversal::nextSibling(current);
    } while (current && !isMatchingElement(nodeList, *current));
    return current;
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
        return element;
    }
    if (m_shouldOnlyIncludeDirectChildren) {
        for (traversedCount = 0; traversedCount < count; ++traversedCount) {
            element = nextMatchingSiblingElement(*this, element);
            if (!element)
                return nullptr;
        }
        return element;
    }
    for (traversedCount = 0; traversedCount < count; ++traversedCount) {
        element = nextMatchingElement(*this, element, root);
        if (!element)
            return nullptr;
    }
    return element;
}

Element* HTMLCollection::collectionFirst() const
{
    return firstElement(rootNode());
}

Element* HTMLCollection::collectionLast() const
{
    // FIXME: This should be optimized similarly to the forward case.
    auto& root = rootNode();
    Element* last = m_shouldOnlyIncludeDirectChildren ? ElementTraversal::lastChild(&root) : ElementTraversal::lastWithin(&root);
    return iterateForPreviousElement(last);
}

Element* HTMLCollection::collectionTraverseForward(Element& current, unsigned count, unsigned& traversedCount) const
{
    return traverseForward(current, count, traversedCount, rootNode());
}

Element* HTMLCollection::collectionTraverseBackward(Element& current, unsigned count) const
{
    // FIXME: This should be optimized similarly to the forward case.
    auto& root = rootNode();
    Element* element = &current;
    if (m_shouldOnlyIncludeDirectChildren) {
        for (; count && element ; --count)
            element = iterateForPreviousElement(ElementTraversal::previousSibling(element));
        return element;
    }
    for (; count && element ; --count)
        element = iterateForPreviousElement(ElementTraversal::previous(element, &root));
    return element;
}

void HTMLCollection::invalidateCache(Document& document) const
{
    if (m_indexCache.hasValidCache()) {
        document.unregisterCollection(const_cast<HTMLCollection&>(*this));
        m_indexCache.invalidate();
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

Node* HTMLCollection::namedItem(const AtomicString& name) const
{
    // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
    // This method first searches for an object with a matching id
    // attribute. If a match is not found, the method then searches for an
    // object with a matching name attribute, but only on those elements
    // that are allowed a name attribute.

    if (name.isEmpty())
        return 0;

    ContainerNode& root = rootNode();
    if (!usesCustomForwardOnlyTraversal() && root.isInTreeScope()) {
        TreeScope& treeScope = root.treeScope();
        Element* candidate = 0;
        if (treeScope.hasElementWithId(*name.impl())) {
            if (!treeScope.containsMultipleElementsWithId(name))
                candidate = treeScope.getElementById(name);
        } else if (treeScope.hasElementWithName(*name.impl())) {
            if (!treeScope.containsMultipleElementsWithName(name)) {
                candidate = treeScope.getElementByName(name);
                if (candidate && type() == DocAll && (!candidate->isHTMLElement() || !nameShouldBeVisibleInDocumentAll(toHTMLElement(*candidate))))
                    candidate = 0;
            }
        } else
            return 0;

        if (candidate && isMatchingElement(*this, *candidate)
            && (m_shouldOnlyIncludeDirectChildren ? candidate->parentNode() == &root : candidate->isDescendantOf(&root)))
            return candidate;
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

    return 0;
}

void HTMLCollection::updateNamedElementCache() const
{
    if (hasNamedElementCache())
        return;

    ContainerNode& root = rootNode();
    CollectionNamedElementCache& cache = createNameItemCache();

    unsigned count;
    for (Element* element = firstElement(root); element; element = traverseForward(*element, 1, count, root)) {
        const AtomicString& idAttrVal = element->getIdAttribute();
        if (!idAttrVal.isEmpty())
            cache.appendIdCache(idAttrVal, element);
        if (!element->isHTMLElement())
            continue;
        const AtomicString& nameAttrVal = element->getNameAttribute();
        if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal && (type() != DocAll || nameShouldBeVisibleInDocumentAll(toHTMLElement(*element))))
            cache.appendNameCache(nameAttrVal, element);
    }

    m_indexCache.nodeCount(*this);

    cache.didPopulate();
}

bool HTMLCollection::hasNamedItem(const AtomicString& name) const
{
    // FIXME: We can do better when there are multiple elements of the same name.
    return namedItem(name);
}

void HTMLCollection::namedItems(const AtomicString& name, Vector<Ref<Element>>& result) const
{
    ASSERT(result.isEmpty());
    if (name.isEmpty())
        return;

    updateNamedElementCache();
    ASSERT(m_namedElementCache);

    const Vector<Element*>* idResults = m_namedElementCache->findElementsWithId(name);
    const Vector<Element*>* nameResults = m_namedElementCache->findElementsWithName(name);

    for (unsigned i = 0; idResults && i < idResults->size(); ++i)
        result.append(*idResults->at(i));

    for (unsigned i = 0; nameResults && i < nameResults->size(); ++i)
        result.append(*nameResults->at(i));
}

PassRefPtr<NodeList> HTMLCollection::tags(const String& name)
{
    return ownerNode().getElementsByTagName(name);
}

} // namespace WebCore
