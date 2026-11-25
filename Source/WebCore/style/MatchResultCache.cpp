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

#include "ImmutableStyleProperties.h"
#include "MatchResult.h"
#include "MutableStyleProperties.h"
#include "ResolvedStyle.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "StyleProperties.h"
#include "StyledElement.h"
#include <wtf/BitSet.h>
#include <wtf/ZippedRange.h>

namespace WebCore::Style {

// Cache entries contain a MatchResult object that references the element's mutable inline style
// and mutable presentational hint style. As a result the cache entry mutates when either mutates.
// We save the original properties and values so we can check which properties have changed.
// If a property changes we null the value and assume it will change in the future too.
//
// It would be nicer if the cache entries were immutable but doing that in a sufficiently
// performant way is tricky.

struct OriginalProperty {
    CSSPropertyID propertyID;
    bool wasImportant { false };
    RefPtr<const CSSValue> valueIfUnchanged;
};

static Vector<OriginalProperty> originalPropertiesIfMutable(const StyleProperties* style)
{
    RefPtr mutableStyle = dynamicDowncast<MutableStyleProperties>(style);
    if (!mutableStyle)
        return { };

    return WTF::map(*mutableStyle, [](auto& property) {
        return OriginalProperty {
            .propertyID = property.id(),
            .wasImportant = property.isImportant(),
            .valueIfUnchanged = property.value(),
        };
    });
}

struct MatchResultCache::Entry : CanMakeCheckedPtr<MatchResultCache::Entry> {
    UnadjustedStyle unadjustedStyle;
    RefPtr<const StyleProperties> inlineStyle;
    Vector<OriginalProperty> originalInlineProperties;
    RefPtr<const StyleProperties> presentationalHintStyle;
    Vector<OriginalProperty> originalPresentationalHintProperties;
    RefPtr<const StyleProperties> additionalPresentationalHintStyle;

    Entry(UnadjustedStyle&& unadjustedStyle, const StyledElement& element)
        : unadjustedStyle(WTF::move(unadjustedStyle))
        , inlineStyle(element.inlineStyle())
        , originalInlineProperties(originalPropertiesIfMutable(inlineStyle.get()))
        , presentationalHintStyle(element.presentationalHintStyle())
        , originalPresentationalHintProperties(originalPropertiesIfMutable(presentationalHintStyle.get()))
        , additionalPresentationalHintStyle(element.additionalPresentationalHintStyle())
    {
    }

    WTF_STRUCT_OVERRIDE_DELETE_FOR_CHECKED_PTR(Entry);
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(MatchResultCache);
};

MatchResultCache::MatchResultCache() = default;
MatchResultCache::~MatchResultCache() = default;

inline UnadjustedStyle copy(const UnadjustedStyle& other)
{
    // Relations hold raw Element pointers valid only for a single style resolution. They should not be cached.
    return {
        .style = ComputedStyle::clonePtr(protect(*other.style)),
        .relations = { },
        .matchResult = other.matchResult
    };
}

static bool isUsableAfterChange(const StyleProperties* cachedStyle, const Vector<OriginalProperty>& originalProperties, const StyleProperties* currentStyle)
{
    if (cachedStyle != currentStyle)
        return false;

    RefPtr mutableStyle = dynamicDowncast<MutableStyleProperties>(currentStyle);
    if (!mutableStyle)
        return true;

    // Only allow the same exact properties after a change. This way the previous values in ComputedStyle are guaranteed to get overwritten.
    // Adding properties could be allowed without other changes. Removal would require resetting the removed property to initial
    // value in the style builder.

    if (originalProperties.size() != mutableStyle->size())
        return false;

    for (auto [originalProperty, currentProperty] : zippedRange(originalProperties, *mutableStyle)) {
        if (originalProperty.propertyID != currentProperty.id())
            return false;
    }
    return true;
}

bool MatchResultCache::canApplyPartially(CSSPropertyID propertyID)
{
    // FIXME: Support custom properties.
    if (propertyID == CSSPropertyID::Custom)
        return false;

    // Only use partial applying with low-priority properties since we know changes to them can't
    // affect values of other properties.
    // FIXME: CSSPropertyID::LineHeight shouldn't be a low-priority property as others can depend on it via `lh` unit.
    return propertyID >= firstLowPriorityProperty && propertyID != CSSPropertyID::LineHeight;
}

enum class ValueComparison : bool { Identity, Equality };

static PropertyCascade::IncludedProperties computeAndUpdateChangedProperties(Vector<OriginalProperty>& originalProperties, const StyleProperties* style, ValueComparison comparison)
{
    RefPtr mutableStyle = dynamicDowncast<MutableStyleProperties>(style);
    if (!mutableStyle)
        return { };

    PropertyCascade::IncludedProperties result;

    ASSERT(originalProperties.size() == mutableStyle->size());
    for (auto [originalProperty, currentProperty] : zippedRange(originalProperties, *mutableStyle)) {
        auto propertyID = currentProperty.id();

        ASSERT(originalProperty.propertyID == propertyID);

        if (originalProperty.valueIfUnchanged && originalProperty.wasImportant == currentProperty.isImportant()) {
            if (originalProperty.valueIfUnchanged == currentProperty.value())
                continue;
            if (comparison == ValueComparison::Equality && protect(*originalProperty.valueIfUnchanged)->equals(protect(*currentProperty.value()))) {
                originalProperty.valueIfUnchanged = currentProperty.value();
                continue;
            }
        }

        // Assume that if a value or priority changes once then it will change more. Don't track changes anymore.
        originalProperty.valueIfUnchanged = nullptr;

        if (!MatchResultCache::canApplyPartially(propertyID))
            return PropertyCascade::normalProperties();

        result.ids.append(cascadeAliasProperty(propertyID));
    }

    return result;
}

const std::optional<CachedMatchResult> MatchResultCache::resultWithCurrentDeclarations(const Element& element)
{
    auto it = m_entries.find(element);
    if (it == m_entries.end())
        return { };

    auto& entry = it->value;

    bool isUsable = [&] {
        auto* styledElement = dynamicDowncast<StyledElement>(element);
        if (!styledElement)
            return false;
        if (entry->additionalPresentationalHintStyle != styledElement->additionalPresentationalHintStyle())
            return false;
        return isUsableAfterChange(entry->inlineStyle.get(), entry->originalInlineProperties, protect(styledElement->inlineStyle()).get())
            && isUsableAfterChange(entry->presentationalHintStyle.get(), entry->originalPresentationalHintProperties, protect(styledElement->presentationalHintStyle()).get());
    }();

    if (!isUsable) {
        m_entries.remove(it);
        return { };
    }

    auto changedProperties = computeAndUpdateChangedProperties(entry->originalInlineProperties, entry->inlineStyle.get(), ValueComparison::Identity);
    auto changedHintProperties = computeAndUpdateChangedProperties(entry->originalPresentationalHintProperties, entry->presentationalHintStyle.get(), ValueComparison::Equality);

    if (changedProperties.types || changedHintProperties.types)
        changedProperties = PropertyCascade::normalProperties();
    else {
        for (auto propertyID : changedHintProperties.ids) {
            if (!changedProperties.ids.contains(propertyID))
                changedProperties.ids.append(propertyID);
        }
    }

    return CachedMatchResult {
        .unadjustedStyle = copy(entry->unadjustedStyle),
        .changedProperties = WTF::move(changedProperties),
        .styleToUpdate = *entry->unadjustedStyle.style
    };
}

void MatchResultCache::update(CachedMatchResult& result, const Style::ComputedStyle& style)
{
    protect(result.styleToUpdate)->cloneFrom(style);
}

void MatchResultCache::updateForFastPathInherit(const Element& element, const Style::ComputedStyle& parentStyle)
{
    CheckedPtr entry = m_entries.get(element);
    if (!entry)
        return;
    entry->unadjustedStyle.style->fastPathInheritFrom(parentStyle);
}

void MatchResultCache::set(const Element& element, const UnadjustedStyle& unadjustedStyle)
{
    // For now we cache match results if there is mutable inline style or a mutable presentational hint style.
    // This way we can avoid selector matching when either gets mutated again.
    auto* styledElement = dynamicDowncast<StyledElement>(element);
    if (!styledElement) {
        m_entries.remove(element);
        return;
    }

    if (is<MutableStyleProperties>(styledElement->inlineStyle()) || is<MutableStyleProperties>(styledElement->presentationalHintStyle()))
        m_entries.set(element, makeUniqueRef<Entry>(copy(unadjustedStyle), *styledElement));
    else
        m_entries.remove(element);
}

}
