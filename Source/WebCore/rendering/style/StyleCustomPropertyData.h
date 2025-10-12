/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#pragma once

#include <WebCore/StyleCustomProperty.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/IterationStatus.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {
namespace Style {

class CustomPropertyData : public RefCounted<CustomPropertyData> {
private:
    using CustomPropertyValueMap = HashMap<AtomString, RefPtr<const CustomProperty>>;

public:
    static Ref<CustomPropertyData> create() { return adoptRef(*new CustomPropertyData); }
    Ref<CustomPropertyData> copy() const { return adoptRef(*new CustomPropertyData(*this)); }

    bool operator==(const CustomPropertyData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const CustomPropertyData&) const;
#endif

    const CustomProperty* get(const AtomString&) const;
    void set(const AtomString&, Ref<const CustomProperty>&&);

    unsigned size() const { return m_size; }
    bool mayHaveAnimatableProperties() const { return m_mayHaveAnimatableProperties; }

    void forEach(NOESCAPE const Function<IterationStatus(const KeyValuePair<AtomString, RefPtr<const CustomProperty>>&)>&) const;
    AtomString findKeyAtIndex(unsigned) const;

private:
    CustomPropertyData() = default;
    CustomPropertyData(const CustomPropertyData&);

    template<typename Callback> void forEachInternal(Callback&&) const;

    RefPtr<const CustomPropertyData> m_parentValues;
    CustomPropertyValueMap m_ownValues;
    unsigned m_size { 0 };
    unsigned m_ancestorCount { 0 };
    bool m_mayHaveAnimatableProperties { false };
#if ASSERT_ENABLED
    mutable bool m_hasChildren { false };
#endif
};

} // namespace Style
} // namespace WebCore
