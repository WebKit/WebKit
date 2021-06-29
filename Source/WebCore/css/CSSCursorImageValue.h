/*
 * Copyright (C) 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2008-2021 Apple Inc. All right reserved.
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
 */

#pragma once

#include "CSSValue.h"
#include "IntPoint.h"
#include "ResourceLoaderOptions.h"
#include <wtf/HashSet.h>

namespace WebCore {

class Document;
class SVGCursorElement;

struct ImageWithScale;

namespace Style {
class BuilderState;
}

class CSSCursorImageValue final : public CSSValue {
public:
    static Ref<CSSCursorImageValue> create(Ref<CSSValue>&& imageValue, const std::optional<IntPoint>& hotSpot, LoadedFromOpaqueSource);
    ~CSSCursorImageValue();

    std::optional<IntPoint> hotSpot() const { return m_hotSpot; }

    const URL& imageURL() const { return m_originalURL; }

    String customCSSText() const;

    ImageWithScale selectBestFitImage(const Document&);

    bool equals(const CSSCursorImageValue&) const;

    void cursorElementRemoved(SVGCursorElement&);
    void cursorElementChanged(SVGCursorElement&);

    Ref<CSSCursorImageValue> valueWithStylesResolved(Style::BuilderState&);

private:
    CSSCursorImageValue(Ref<CSSValue>&& imageValue, const std::optional<IntPoint>& hotSpot, LoadedFromOpaqueSource);

    SVGCursorElement* updateCursorElement(const Document&);

    URL m_originalURL;
    Ref<CSSValue> m_imageValue;
    std::optional<IntPoint> m_hotSpot;
    HashSet<SVGCursorElement*> m_cursorElements;
    LoadedFromOpaqueSource m_loadedFromOpaqueSource { LoadedFromOpaqueSource::No };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSCursorImageValue, isCursorImageValue())
