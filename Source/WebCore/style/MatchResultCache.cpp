/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "MatchResultCache.h"

#include "MatchResult.h"
#include "RenderStyle+SettersInlines.h"
#include "ResolvedStyle.h"
#include "StyleProperties.h"
#include "StyledElement.h"
#include <wtf/BitSet.h>

namespace WebCore {
namespace Style {

// Cache entries contain a MatchResult object that references element's mutable inline style.
// As a result the cache entry mutates when element's inline style mutates.
// We save the original properties and values so we can check which properties have changed.
// If a property changes we null the value and assume it will change in the future too.
//
// It would be nicer if the cache entries were immutable but doing that in a sufficiently
// performant way is tricky.

struct OriginalInlineProperty {
    CSSPropertyID propertyID;
    RefPtr<const CSSValue> valueIfUnchanged;
};

struct MatchResultCache::Entry : CanMakeCheckedPtr<MatchResultCache::Entry> {
    UnadjustedStyle unadjustedStyle;
    Ref<const MutableStyleProperties> inlineStyle;
    Vector<OriginalInlineProperty> originalInlineProperties;

    Entry(UnadjustedStyle&& unadjustedStyle, const MutableStyleProperties& inlineStyle)
        : unadjustedStyle(WTF::move(unadjustedStyle))
        , inlineStyle(inlineStyle)
    {
        originalInlineProperties.reserveInitialCapacity(inlineStyle.size());
        for (auto property : inlineStyle) {
            originalInlineProperties.append({
                .propertyID = property.id(),
                .valueIfUnchanged = property.value()
            });
        }
    }

    WTF_STRUCT_OVERRIDE_DELETE_FOR_CHECKED_PTR(Entry);
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(MatchResultCache);
};

MatchResultCache::MatchResultCache() = default;
MatchResultCache::~MatchResultCache() = default;

inline UnadjustedStyle copy(const UnadjustedStyle& other)
{
    return {
        .style = RenderStyle::clonePtr(*other.style),
        .relations = other.relations ? makeUnique<Relations>(*other.relations) : std::unique_ptr<Relations> { },
        .matchResult = other.matchResult
    };
}

bool MatchResultCache::isUsableAfterInlineStyleChange(const MatchResultCache::Entry& entry, const StyleProperties& currentInlineStyle)
{
    if (entry.inlineStyle.ptr() != &currentInlineStyle)
        return false;

    Ref inlineStyle = entry.inlineStyle;

    // Only allow the same exact properties after a change. This way the previous values in RenderStyle are guranteed to get overwritten.
    // Adding properties could be allowed without other changes. Removal would require resetting the removed property to initial
    // value in the style builder.

    auto size = entry.originalInlineProperties.size();
    if (size != inlineStyle->size())
        return false;

    for (size_t index = 0; index < size; ++index) {
        if (entry.originalInlineProperties[index].propertyID != inlineStyle->propertyAt(index).id())
            return false;
    }
    return true;
}

PropertyCascade::IncludedProperties MatchResultCache::computeAndUpdateChangedProperties(MatchResultCache::Entry& entry)
{
    auto& originalProperties = entry.originalInlineProperties;
    Ref inlineStyle = entry.inlineStyle.get();

    PropertyCascade::IncludedProperties result;

    auto size = originalProperties.size();
    for (size_t index = 0; index < size; ++index) {
        auto currentProperty = inlineStyle->propertyAt(index);
        auto propertyID = currentProperty.id();

        ASSERT(originalProperties[index].propertyID == propertyID);

        if (originalProperties[index].valueIfUnchanged == currentProperty.value())
            continue;

        // Assume that if a value changes ones then it will change more. Don't track changes anymore.
        originalProperties[index].valueIfUnchanged = nullptr;

        // FIXME: Support custom properties.
        if (propertyID == CSSPropertyCustom)
            return PropertyCascade::normalProperties();

        // Only use partial applying with low-priority properties since we know changes to them can't
        // affect values of other properties.
        // FIXME: CSSPropertyLineHeight shouldn't be a low-priority property as others can depend on it via `lh` unit.
        if (propertyID < firstLowPriorityProperty || propertyID == CSSPropertyLineHeight)
            return PropertyCascade::normalProperties();

        result.ids.append(cascadeAliasProperty(propertyID));
    }

    return result;
}

const std::optional<CachedMatchResult> MatchResultCache::resultWithCurrentInlineStyle(const Element& element)
{
    auto it = m_entries.find(element);
    if (it == m_entries.end())
        return { };

    auto& entry = it->value;

    auto* styledElement = dynamicDowncast<StyledElement>(element);
    RefPtr inlineStyle = styledElement ? styledElement->inlineStyle() : nullptr;

    if (!inlineStyle || !isUsableAfterInlineStyleChange(entry, *inlineStyle)) {
        m_entries.remove(it);
        return { };
    }

    auto changedProperties = computeAndUpdateChangedProperties(entry);

    return CachedMatchResult {
        .unadjustedStyle = copy(entry->unadjustedStyle),
        .changedProperties = WTF::move(changedProperties),
        .styleToUpdate = *entry->unadjustedStyle.style
    };
}

void MatchResultCache::update(CachedMatchResult& result, const RenderStyle& style)
{
    result.styleToUpdate.get() = RenderStyle::clone(style);
}

void MatchResultCache::updateForFastPathInherit(const Element& element, const RenderStyle& parentStyle)
{
    auto entry = m_entries.get(element);
    if (!entry)
        return;
    entry->unadjustedStyle.style->fastPathInheritFrom(parentStyle);
}

void MatchResultCache::set(const Element& element, const UnadjustedStyle& unadjustedStyle)
{
    // For now we cache match results if there is mutable inline style. This way we can avoid
    // selector matching when it gets mutated again.
    auto* styledElement = dynamicDowncast<StyledElement>(element);
    RefPtr inlineStyle = styledElement ? dynamicDowncast<MutableStyleProperties>(styledElement->inlineStyle()) : nullptr;

    if (inlineStyle)
        m_entries.set(element, makeUniqueRef<Entry>(copy(unadjustedStyle), *inlineStyle));
    else
        m_entries.remove(element);
}

}
}
