/*
 * Copyright (C) 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Apple Inc. All right reserved.
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

#ifndef CSSCursorImageValue_h
#define CSSCursorImageValue_h

#include "CSSImageValue.h"
#include "IntPoint.h"
#include <wtf/HashSet.h>

namespace WebCore {

class Document;
class Element;
class SVGElement;

class CSSCursorImageValue : public CSSValue {
public:
    static PassRef<CSSCursorImageValue> create(PassRef<CSSValue> imageValue, bool hasHotSpot, const IntPoint& hotSpot)
    {
        return adoptRef(*new CSSCursorImageValue(WTF::move(imageValue), hasHotSpot, hotSpot));
    }

    ~CSSCursorImageValue();

    bool hasHotSpot() const { return m_hasHotSpot; }

    IntPoint hotSpot() const
    {
        if (m_hasHotSpot)
            return m_hotSpot;
        return IntPoint(-1, -1);
    }

    String customCSSText() const;

    bool updateIfSVGCursorIsUsed(Element*);
    StyleImage* cachedImage(CachedResourceLoader*);
    StyleImage* cachedOrPendingImage(Document&);

    void removeReferencedElement(SVGElement*);

    bool equals(const CSSCursorImageValue&) const;

private:
    CSSCursorImageValue(PassRef<CSSValue> imageValue, bool hasHotSpot, const IntPoint& hotSpot);

    void detachPendingImage();

    bool isSVGCursor() const;
    String cachedImageURL();
    void clearCachedImage();

    Ref<CSSValue> m_imageValue;

    bool m_hasHotSpot;
    IntPoint m_hotSpot;
    RefPtr<StyleImage> m_image;
    bool m_accessedImage;

    HashSet<SVGElement*> m_referencedElements;
};

CSS_VALUE_TYPE_CASTS(CSSCursorImageValue, isCursorImageValue())

} // namespace WebCore

#endif // CSSCursorImageValue_h
