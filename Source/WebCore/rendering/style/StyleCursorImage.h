/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005-2008, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Noam Rosenthal (noam@webkit.org)
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

#include "StyleMultiImage.h"
#include <wtf/WeakHashSet.h>

namespace WebCore {

class CSSValue;
class Document;
class WeakPtrImplWithEventTargetData;
class SVGCursorElement;

enum class LoadedFromOpaqueSource : bool;

class StyleCursorImage final : public StyleMultiImage {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<StyleCursorImage> create(Ref<StyleImage>&&, const std::optional<IntPoint>&, const URL&, LoadedFromOpaqueSource);
    virtual ~StyleCursorImage();

    bool operator==(const StyleImage&) const final;
    bool equals(const StyleCursorImage&) const;
    bool equalInputImages(const StyleCursorImage&) const;

    bool usesDataProtocol() const final;

    void cursorElementRemoved(SVGCursorElement&);
    void cursorElementChanged(SVGCursorElement&);

private:
    explicit StyleCursorImage(Ref<StyleImage>&&, const std::optional<IntPoint>& hotSpot, const URL&, LoadedFromOpaqueSource);

    void setContainerContextForRenderer(const RenderElement& renderer, const FloatSize& containerSize, float containerZoom) final;
    Ref<CSSValue> computedStyleValue(const RenderStyle&) const final;
    ImageWithScale selectBestFitImage(const Document&) final;

    RefPtr<SVGCursorElement> updateCursorElement(const Document&);

    Ref<StyleImage> m_image;
    std::optional<IntPoint> m_hotSpot;
    URL m_originalURL;
    LoadedFromOpaqueSource m_loadedFromOpaqueSource;
    WeakHashSet<SVGCursorElement, WeakPtrImplWithEventTargetData> m_cursorElements;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(StyleCursorImage, isCursorImage)
