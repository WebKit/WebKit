/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CollectionType.h"
#include "ElementChildIterator.h"
#include "ElementDescendantIterator.h"

namespace WebCore {

template <CollectionTraversalType traversalType>
struct CollectionTraversal { };

template <>
struct CollectionTraversal<CollectionTraversalType::Descendants> {
    using Iterator = ElementDescendantIterator;

    static ElementDescendantIterator end(ContainerNode&) { return ElementDescendantIterator(); }

    template <typename CollectionClass>
    static ElementDescendantIterator begin(const CollectionClass&, ContainerNode& rootNode);

    template <typename CollectionClass>
    static ElementDescendantIterator last(const CollectionClass&, ContainerNode& rootNode);

    template <typename CollectionClass>
    static void traverseForward(const CollectionClass&, ElementDescendantIterator& current, unsigned count, unsigned& traversedCount);

    template <typename CollectionClass>
    static void traverseBackward(const CollectionClass&, ElementDescendantIterator& current, unsigned count);
};

template <typename CollectionClass>
inline ElementDescendantIterator CollectionTraversal<CollectionTraversalType::Descendants>::begin(const CollectionClass& collection, ContainerNode& rootNode)
{
    auto descendants = elementDescendants(rootNode);
    auto end = descendants.end();
    for (auto it = descendants.begin(); it != end; ++it) {
        if (collection.elementMatches(*it)) {
            // Drop iterator assertions because HTMLCollections / NodeList use a fine-grained invalidation scheme.
            it.dropAssertions();
            return it;
        }
    }
    return end;
}

template <typename CollectionClass>
inline ElementDescendantIterator CollectionTraversal<CollectionTraversalType::Descendants>::last(const CollectionClass& collection, ContainerNode& rootNode)
{
    auto descendants = elementDescendants(rootNode);
    ElementDescendantIterator invalid;
    for (auto it = descendants.last(); it != invalid; --it) {
        if (collection.elementMatches(*it)) {
            // Drop iterator assertions because HTMLCollections / NodeList use a fine-grained invalidation scheme.
            it.dropAssertions();
            return it;
        }
    }
    return invalid;
}

template <typename CollectionClass>
inline void CollectionTraversal<CollectionTraversalType::Descendants>::traverseForward(const CollectionClass& collection, ElementDescendantIterator& current, unsigned count, unsigned& traversedCount)
{
    ASSERT(collection.elementMatches(*current));
    ElementDescendantIterator invalid;
    for (traversedCount = 0; traversedCount < count; ++traversedCount) {
        do {
            ++current;
            if (current == invalid)
                return;
        } while (!collection.elementMatches(*current));
    }
}

template <typename CollectionClass>
inline void CollectionTraversal<CollectionTraversalType::Descendants>::traverseBackward(const CollectionClass& collection, ElementDescendantIterator& current, unsigned count)
{
    ASSERT(collection.elementMatches(*current));
    ElementDescendantIterator invalid;
    for (; count; --count) {
        do {
            --current;
            if (current == invalid)
                return;
        } while (!collection.elementMatches(*current));
    }
}

template <>
struct CollectionTraversal<CollectionTraversalType::ChildrenOnly> {
    using Iterator = ElementChildIterator<Element>;

    static ElementChildIterator<Element> end(ContainerNode& rootNode) { return ElementChildIterator<Element>(rootNode); }

    template <typename CollectionClass>
    static ElementChildIterator<Element> begin(const CollectionClass&, ContainerNode& rootNode);

    template <typename CollectionClass>
    static ElementChildIterator<Element> last(const CollectionClass&, ContainerNode& rootNode);

    template <typename CollectionClass>
    static void traverseForward(const CollectionClass&, ElementChildIterator<Element>& current, unsigned count, unsigned& traversedCount);

    template <typename CollectionClass>
    static void traverseBackward(const CollectionClass&, ElementChildIterator<Element>& current, unsigned count);
};

template <typename CollectionClass>
inline ElementChildIterator<Element> CollectionTraversal<CollectionTraversalType::ChildrenOnly>::begin(const CollectionClass& collection, ContainerNode& rootNode)
{
    auto children = childrenOfType<Element>(rootNode);
    auto end = children.end();
    for (auto it = children.begin(); it != end; ++it) {
        if (collection.elementMatches(*it)) {
            // Drop iterator assertions because HTMLCollections / NodeList use a fine-grained invalidation scheme.
            it.dropAssertions();
            return it;
        }
    }
    return end;
}

template <typename CollectionClass>
inline ElementChildIterator<Element> CollectionTraversal<CollectionTraversalType::ChildrenOnly>::last(const CollectionClass& collection, ContainerNode& rootNode)
{
    auto children = childrenOfType<Element>(rootNode);
    ElementChildIterator<Element> invalid(collection.rootNode());
    ElementChildIterator<Element> last(rootNode, children.last());
    for (auto it = last; it != invalid; --it) {
        if (collection.elementMatches(*it)) {
            // Drop iterator assertions because HTMLCollections / NodeList use a fine-grained invalidation scheme.
            it.dropAssertions();
            return it;
        }
    }
    return invalid;
}

template <typename CollectionClass>
inline void CollectionTraversal<CollectionTraversalType::ChildrenOnly>::traverseForward(const CollectionClass& collection, ElementChildIterator<Element>& current, unsigned count, unsigned& traversedCount)
{
    ASSERT(collection.elementMatches(*current));
    ElementChildIterator<Element> invalid(collection.rootNode());
    for (traversedCount = 0; traversedCount < count; ++traversedCount) {
        do {
            ++current;
            if (current == invalid)
                return;
        } while (!collection.elementMatches(*current));
    }
}

template <typename CollectionClass>
inline void CollectionTraversal<CollectionTraversalType::ChildrenOnly>::traverseBackward(const CollectionClass& collection, ElementChildIterator<Element>& current, unsigned count)
{
    ASSERT(collection.elementMatches(*current));
    ElementChildIterator<Element> invalid(collection.rootNode());
    for (; count; --count) {
        do {
            --current;
            if (current == invalid)
                return;
        } while (!collection.elementMatches(*current));
    }
}

template <>
struct CollectionTraversal<CollectionTraversalType::CustomForwardOnly> {
    using Iterator = Element*;

    static Element* end(ContainerNode&) { return nullptr; }

    template <typename CollectionClass>
    static Element* begin(const CollectionClass&, ContainerNode&);

    template <typename CollectionClass>
    static Element* last(const CollectionClass&, ContainerNode&);

    template <typename CollectionClass>
    static void traverseForward(const CollectionClass&, Element*& current, unsigned count, unsigned& traversedCount);

    template <typename CollectionClass>
    static void traverseBackward(const CollectionClass&, Element*&, unsigned count);
};

template <typename CollectionClass>
inline Element* CollectionTraversal<CollectionTraversalType::CustomForwardOnly>::begin(const CollectionClass& collection, ContainerNode&)
{
    return collection.customElementAfter(nullptr);
}

template <typename CollectionClass>
inline Element* CollectionTraversal<CollectionTraversalType::CustomForwardOnly>::last(const CollectionClass&, ContainerNode&)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

template <typename CollectionClass>
inline void CollectionTraversal<CollectionTraversalType::CustomForwardOnly>::traverseForward(const CollectionClass& collection, Element*& current, unsigned count, unsigned& traversedCount)
{
    Element* element = current;
    for (traversedCount = 0; traversedCount < count; ++traversedCount) {
        element = collection.customElementAfter(element);
        if (!element) {
            current = nullptr;
            return;
        }
    }
    current = element;
}

template <typename CollectionClass>
inline void CollectionTraversal<CollectionTraversalType::CustomForwardOnly>::traverseBackward(const CollectionClass&, Element*&, unsigned count)
{
    UNUSED_PARAM(count);
    ASSERT_NOT_REACHED();
}

} // namespace WebCore
