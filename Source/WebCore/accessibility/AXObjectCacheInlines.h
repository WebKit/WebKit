/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AXGeometryManager.h"
#include "AXIsolatedTree.h"
#include "AXObjectCache.h"

namespace WebCore {

inline String CharacterOffset::debugDescription()
{
    auto nodeDescription = node ? CheckedPtr { node.get() }->debugDescription() : "null"_s;
    return makeString("CharacterOffset {node: "_s, nodeDescription, ", startIndex: "_s, startIndex, ", offset: "_s, offset, ", remainingOffset: "_s, remainingOffset, '}');
}

inline bool CharacterOffset::isEqual(const CharacterOffset& other) const
{
    if (isNull() || other.isNull())
        return false;
    return node == other.node && startIndex == other.startIndex && offset == other.offset;
}

template<typename U>
inline Vector<Ref<AXCoreObject>> AXObjectCache::objectsForIDs(const U& axIDs) const
{
    AX_ASSERT(isMainThread());

    CheckedPtr cache = this;
    return WTF::compactMap(axIDs, [cache](auto& axID) -> std::optional<Ref<AXCoreObject>> {
        if (auto* object = cache->objectForID(axID))
            return Ref { *object };
        return std::nullopt;
    });
}

inline Node* AXObjectCache::nodeForID(std::optional<AXID> axID) const
{
    if (!axID)
        return nullptr;

    RefPtr object = m_objects.get(*axID);
    return object ? object->node() : nullptr;
}

inline AccessibilityObject* AXObjectCache::getOrCreate(Node& node, IsPartOfRelation isPartOfRelation)
{
    if (auto* object = get(node))
        return object;
    return getOrCreateSlow(node, isPartOfRelation);
}

inline AccessibilityObject* AXObjectCache::getOrCreate(Element& element, IsPartOfRelation isPartOfRelation)
{
    if (auto* object = get(element))
        return object;
    return getOrCreateSlow(element, isPartOfRelation);
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)

inline void AXObjectCache::scheduleObjectRegionsUpdate(bool scheduleImmediately)
{
    m_geometryManager->scheduleObjectRegionsUpdate(scheduleImmediately);
}

inline void AXObjectCache::willUpdateObjectRegions()
{
    m_geometryManager->willUpdateObjectRegions();
}

#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

} // WebCore
