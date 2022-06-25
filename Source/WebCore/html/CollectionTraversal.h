/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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
#include "ElementIterator.h"

namespace WebCore {

template <CollectionTraversalType traversalType>
struct CollectionTraversal { };

template <>
struct CollectionTraversal<CollectionTraversalType::Descendants> {
    using Iterator = ElementDescendantIterator<Element>;

    template <typename CollectionClass>
    static Iterator begin(const CollectionClass&, ContainerNode& rootNode);

    template <typename CollectionClass>
    static Iterator last(const CollectionClass&, ContainerNode& rootNode);

    template <typename CollectionClass>
    static void traverseForward(const CollectionClass&, Iterator& current, unsigned count, unsigned& traversedCount);

    template <typename CollectionClass>
    static void traverseBackward(const CollectionClass&, Iterator& current, unsigned count);
};

template <typename CollectionClass>
inline auto CollectionTraversal<CollectionTraversalType::Descendants>::begin(const CollectionClass& collection, ContainerNode& rootNode) -> Iterator
{
    auto it = descendantsOfType<Element>(rootNode).begin();
    while (it && !collection.elementMatches(*it))
        ++it;
    // Drop iterator assertions because HTMLCollections / NodeList use a fine-grained invalidation scheme.
    it.dropAssertions();
    return it;
}

template <typename CollectionClass>
inline auto CollectionTraversal<CollectionTraversalType::Descendants>::last(const CollectionClass& collection, ContainerNode& rootNode) -> Iterator
{
    Iterator it { rootNode, ElementTraversal::lastWithin(rootNode) };
    while (it && !collection.elementMatches(*it))
        --it;
    // Drop iterator assertions because HTMLCollections / NodeList use a fine-grained invalidation scheme.
    it.dropAssertions();
    return it;
}

template <typename CollectionClass>
inline void CollectionTraversal<CollectionTraversalType::Descendants>::traverseForward(const CollectionClass& collection, Iterator& current, unsigned count, unsigned& traversedCount)
{
    ASSERT(collection.elementMatches(*current));
    for (traversedCount = 0; traversedCount < count; ++traversedCount) {
        do {
            ++current;
            if (!current)
                return;
        } while (!collection.elementMatches(*current));
    }
}

template <typename CollectionClass>
inline void CollectionTraversal<CollectionTraversalType::Descendants>::traverseBackward(const CollectionClass& collection, Iterator& current, unsigned count)
{
    ASSERT(collection.elementMatches(*current));
    for (; count; --count) {
        do {
            --current;
            if (!current)
                return;
        } while (!collection.elementMatches(*current));
    }
}

template <>
struct CollectionTraversal<CollectionTraversalType::ChildrenOnly> {
    using Iterator = ElementChildIterator<Element>;

    template <typename CollectionClass>
    static Iterator begin(const CollectionClass&, ContainerNode& rootNode);

    template <typename CollectionClass>
    static Iterator last(const CollectionClass&, ContainerNode& rootNode);

    template <typename CollectionClass>
    static void traverseForward(const CollectionClass&, Iterator& current, unsigned count, unsigned& traversedCount);

    template <typename CollectionClass>
    static void traverseBackward(const CollectionClass&, Iterator& current, unsigned count);
};

template <typename CollectionClass>
inline auto CollectionTraversal<CollectionTraversalType::ChildrenOnly>::begin(const CollectionClass& collection, ContainerNode& rootNode) -> Iterator
{
    auto it = childrenOfType<Element>(rootNode).begin();
    while (it && !collection.elementMatches(*it))
        ++it;
    // Drop iterator assertions because HTMLCollections / NodeList use a fine-grained invalidation scheme.
    it.dropAssertions();
    return it;
}

template <typename CollectionClass>
inline auto CollectionTraversal<CollectionTraversalType::ChildrenOnly>::last(const CollectionClass& collection, ContainerNode& rootNode) -> Iterator
{
    auto it = childrenOfType<Element>(rootNode).begin();
    while (it && !collection.elementMatches(*it))
        --it;
    // Drop iterator assertions because HTMLCollections / NodeList use a fine-grained invalidation scheme.
    it.dropAssertions();
    return it;
}

template <typename CollectionClass>
inline void CollectionTraversal<CollectionTraversalType::ChildrenOnly>::traverseForward(const CollectionClass& collection, Iterator& current, unsigned count, unsigned& traversedCount)
{
    ASSERT(collection.elementMatches(*current));
    for (traversedCount = 0; traversedCount < count; ++traversedCount) {
        do {
            ++current;
            if (!current)
                return;
        } while (!collection.elementMatches(*current));
    }
}

template <typename CollectionClass>
inline void CollectionTraversal<CollectionTraversalType::ChildrenOnly>::traverseBackward(const CollectionClass& collection, Iterator& current, unsigned count)
{
    ASSERT(collection.elementMatches(*current));
    for (; count; --count) {
        do {
            --current;
            if (!current)
                return;
        } while (!collection.elementMatches(*current));
    }
}

template <>
struct CollectionTraversal<CollectionTraversalType::CustomForwardOnly> {
    using Iterator = Element*;

    static constexpr Element* end(ContainerNode&) { return nullptr; }

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
