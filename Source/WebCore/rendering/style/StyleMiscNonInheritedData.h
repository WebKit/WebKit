/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleAlignContent.h>
#include <WebCore/StyleAlignItems.h>
#include <WebCore/StyleAlignSelf.h>
#include <WebCore/StyleAnimations.h>
#include <WebCore/StyleAppearance.h>
#include <WebCore/StyleAspectRatio.h>
#include <WebCore/StyleBoxShadow.h>
#include <WebCore/StyleContent.h>
#include <WebCore/StyleJustifyContent.h>
#include <WebCore/StyleJustifyItems.h>
#include <WebCore/StyleJustifySelf.h>
#include <WebCore/StyleMaskLayers.h>
#include <WebCore/StyleObjectPosition.h>
#include <WebCore/StyleOpacity.h>
#include <WebCore/StyleOrder.h>
#include <WebCore/StyleResize.h>
#include <WebCore/StyleTransitions.h>
#include <memory>
#include <wtf/DataRef.h>
#include <wtf/FixedVector.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class StyleDeprecatedFlexibleBoxData;
class StyleFilterData;
class StyleFlexibleBoxData;
class StyleMultiColData;
class StyleTransformData;
class StyleVisitedLinkColorData;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleMiscNonInheritedData);
class StyleMiscNonInheritedData : public RefCounted<StyleMiscNonInheritedData> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StyleMiscNonInheritedData, StyleMiscNonInheritedData);
public:
    static Ref<StyleMiscNonInheritedData> create() { return adoptRef(*new StyleMiscNonInheritedData); }
    Ref<StyleMiscNonInheritedData> copy() const;
    ~StyleMiscNonInheritedData();

    bool operator==(const StyleMiscNonInheritedData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const StyleMiscNonInheritedData&) const;
#endif

    bool hasFilters() const;

    // This is here to pack in with m_refCount.
    Style::Opacity opacity;

    DataRef<StyleDeprecatedFlexibleBoxData> deprecatedFlexibleBox; // Flexible box properties
    DataRef<StyleFlexibleBoxData> flexibleBox;
    DataRef<StyleMultiColData> multiCol; //  CSS3 multicol properties
    DataRef<StyleFilterData> filter; // Filter operations (url, sepia, blur, etc.)
    DataRef<StyleTransformData> transform; // Transform properties (rotate, scale, skew, etc.)
    DataRef<StyleVisitedLinkColorData> visitedLinkColor;

    Style::MaskLayers mask;
    Style::Animations animations;
    Style::Transitions transitions;
    Style::Content content;
    Style::BoxShadows boxShadow;
    Style::AspectRatio aspectRatio;

    Style::AlignContent alignContent;
    Style::AlignItems alignItems;
    Style::AlignSelf alignSelf;
    Style::JustifyContent justifyContent;
    Style::JustifyItems justifyItems;
    Style::JustifySelf justifySelf;

    Style::ObjectPosition objectPosition;
    Style::Order order;

    PREFERRED_TYPE(bool) unsigned hasAttrContent : 1 { false };
    PREFERRED_TYPE(bool) unsigned hasDisplayAffectedByAnimations : 1 { false };
#if ENABLE(DARK_MODE_CSS)
    PREFERRED_TYPE(bool) unsigned hasExplicitlySetColorScheme : 1 { false };
#endif
    PREFERRED_TYPE(bool) unsigned hasExplicitlySetDirection : 1 { false };
    PREFERRED_TYPE(bool) unsigned hasExplicitlySetWritingMode : 1 { false };
    PREFERRED_TYPE(TableLayoutType) unsigned tableLayout : 1;
    PREFERRED_TYPE(StyleAppearance) unsigned appearance : appearanceBitWidth;
    PREFERRED_TYPE(StyleAppearance) unsigned usedAppearance : appearanceBitWidth;
    PREFERRED_TYPE(bool) unsigned textOverflow : 1; // Whether or not lines that spill out should be truncated with "..."
    PREFERRED_TYPE(UserDrag) unsigned userDrag : 2;
    PREFERRED_TYPE(ObjectFit) unsigned objectFit : 3;
    PREFERRED_TYPE(Style::Resize) unsigned resize : 3;

private:
    StyleMiscNonInheritedData();
    StyleMiscNonInheritedData(const StyleMiscNonInheritedData&);
};

} // namespace WebCore
