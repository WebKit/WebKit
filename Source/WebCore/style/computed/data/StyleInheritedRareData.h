/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleInheritedRareCSSData.h>
#include <WebCore/StyleTouchAction.h>
#include <wtf/DataRef.h>
#include <wtf/OptionSet.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

#if HAVE(CORE_MATERIAL)
#include <WebCore/AppleVisualEffect.h>
#endif

namespace WebCore {
namespace Style {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(InheritedRareData);
class InheritedRareData : public RefCounted<InheritedRareData> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(InheritedRareData, InheritedRareData);
public:
    static Ref<InheritedRareData> create() { return adoptRef(*new InheritedRareData); }
    Ref<InheritedRareData> copy() const;
    ~InheritedRareData();

    bool operator==(const InheritedRareData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const InheritedRareData&) const;
#endif

    float usedZoom;
    TouchAction usedTouchAction;
    OptionSet<EventListenerRegionType> eventListenerRegionTypes;

    DataRef<InheritedRareCSSData> cssData;

    PREFERRED_TYPE(bool) unsigned effectiveInert : 1;
    PREFERRED_TYPE(bool) unsigned effectivelyTransparent : 1;
    PREFERRED_TYPE(bool) unsigned isInSubtreeWithBlendMode : 1;
    PREFERRED_TYPE(bool) unsigned isForceHidden : 1;
    PREFERRED_TYPE(ContentVisibility) unsigned usedContentVisibility : 2;
    PREFERRED_TYPE(bool) unsigned autoRevealsWhenFound : 1;
    PREFERRED_TYPE(bool) unsigned insideDefaultButton : 1;
    PREFERRED_TYPE(bool) unsigned insideSubmitButton : 1;
    PREFERRED_TYPE(bool) unsigned evaluationTimeZoomEnabled : 1;
#if HAVE(CORE_MATERIAL)
    PREFERRED_TYPE(AppleVisualEffect) unsigned usedAppleVisualEffectForSubtree : 5;
#endif

private:
    InheritedRareData();
    InheritedRareData(const InheritedRareData&);
};

} // namespace Style
} // namespace WebCore
