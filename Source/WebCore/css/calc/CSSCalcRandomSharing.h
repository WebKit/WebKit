/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/CSSCustomIdent.h>
#include <WebCore/CSSValueKeywords.h>
#include <optional>
#include <wtf/Hasher.h>
#include <wtf/Variant.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

enum CSSPropertyID : uint16_t;

namespace CSSCalc {

// Which function a property scope was derived from. random() and random-item() each get their own
// bucket, so an index derived for one cannot collide with the same index derived for the other. Only
// derived scopes carry this, so an author's <dashed-ident> still shares across both functions.
// https://github.com/w3c/csswg-drafts/issues/14330
enum class RandomFunction : bool { Random, RandomItem };

// The property a random key is scoped to. Every custom property shares CSSPropertyCustom, so the name
// is what tells them apart and is empty for everything else.
// FIXME: Same concept as AssociatedProperty and AnimatableCSSProperty.
struct RandomScopedProperty {
    CSSPropertyID property { CSSPropertyInvalid };
    AtomString customPropertyName { };
    RandomFunction function { RandomFunction::Random };

    bool operator==(const RandomScopedProperty&) const = default;
};

inline void add(Hasher& hasher, const RandomScopedProperty& scopedProperty)
{
    add(hasher, scopedProperty.property, scopedProperty.customPropertyName, scopedProperty.function);
}

// `auto` is a top-level <random-key> alternative; its scoping is chosen per-usage by the caller.
// The (property, index) pair is the implementation-derived caching identity.
//
// `index` is 0-based. § 9.4.1 spells the simplified form ua-PROPERTY-INDEX with a 1-indexed INDEX, so
// the <random-ua-ident> serialization follow-up has to add one when serializing.
struct RandomSharingAuto {
    RandomScopedProperty property;
    unsigned index;
    std::optional<CSS::Keyword::ElementScoped> elementScoped;

    bool operator==(const RandomSharingAuto&) const = default;
};

// <random-cache-key> = <dashed-ident> || element-scoped || [ property-scoped | property-index-scoped ]
// NOTE: <random-ua-ident> is intentionally not yet supported (follow-up).
struct RandomSharingKey {
    struct PropertyScoped {
        RandomScopedProperty property;

        bool operator==(const PropertyScoped&) const = default;
    };
    // `index` is 0-based; see the note on RandomSharingAuto.
    struct PropertyIndexScoped {
        RandomScopedProperty property;
        unsigned index;

        bool operator==(const PropertyIndexScoped&) const = default;
    };
    std::optional<CSS::CustomIdent> name;
    std::optional<CSS::Keyword::ElementScoped> elementScoped;
    std::optional<Variant<PropertyScoped, PropertyIndexScoped>> propertyScoped;

    bool operator==(const RandomSharingKey&) const = default;
};

} // namespace CSSCalc
} // namespace WebCore
