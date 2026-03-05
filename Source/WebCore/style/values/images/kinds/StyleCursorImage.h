/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005-2008, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Noam Rosenthal (noam@webkit.org)
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class CSSValue;
class Document;
class WeakPtrImplWithEventTargetData;
class SVGCursorElement;

namespace Style {

class CursorImage final : public MultiImage {
    WTF_MAKE_TZONE_ALLOCATED(CursorImage);
public:
    static Ref<CursorImage> create(const Ref<Image>&, std::optional<IntPoint>, const URL&);
    static Ref<CursorImage> create(Ref<Image>&&, std::optional<IntPoint>, URL&&);
    virtual ~CursorImage();

    bool operator==(const Image&) const final;
    bool equals(const CursorImage&) const;
    bool equalInputImages(const CursorImage&) const;

    bool usesDataProtocol() const final;

    void cursorElementRemoved(SVGCursorElement&);
    void cursorElementChanged(SVGCursorElement&);

    std::optional<IntPoint> hotSpot() const { return m_hotSpot; }

private:
    explicit CursorImage(const Ref<Image>&, std::optional<IntPoint> hotSpot, const URL&);
    explicit CursorImage(Ref<Image>&&, std::optional<IntPoint> hotSpot, URL&&);

    void setContainerContextForRenderer(const RenderElement& renderer, const FloatSize& containerSize, float containerZoom, const WTF::URL& = WTF::URL()) final;
    Ref<CSSValue> computedStyleValue(const RenderStyle&) const final;
    ImageWithScale selectBestFitImage(const Document&) final;

    RefPtr<SVGCursorElement> updateCursorElement(const Document&);

    Ref<Image> m_image;
    std::optional<IntPoint> m_hotSpot;
    URL m_originalURL;
    WeakHashSet<SVGCursorElement, WeakPtrImplWithEventTargetData> m_cursorElements;
};

} // namespace Style
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(CursorImage, isCursorImage)
