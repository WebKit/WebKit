/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2008, 2009, 2011 Apple Inc. All rights reserved.
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

#ifndef HTMLAreaElement_h
#define HTMLAreaElement_h

#include "HTMLAnchorElement.h"
#include "LayoutRect.h"
#include <memory>

namespace WebCore {

class HitTestResult;
class HTMLImageElement;
class Path;

class HTMLAreaElement FINAL : public HTMLAnchorElement {
public:
    static PassRefPtr<HTMLAreaElement> create(const QualifiedName&, Document&);

    bool isDefault() const { return m_shape == Default; }

    bool mapMouseEvent(LayoutPoint location, const LayoutSize&, HitTestResult&);

    // FIXME: Use RenderElement* instead of RenderObject* once we upstream iOS's DOMUIKitExtensions.{h, mm}.
    LayoutRect computeRect(RenderObject*) const;
    Path computePath(RenderObject*) const;

    // The parent map's image.
    HTMLImageElement* imageElement() const;
    
private:
    HTMLAreaElement(const QualifiedName&, Document&);

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;
    virtual bool supportsFocus() const override;
    virtual String target() const override;
    virtual bool isKeyboardFocusable(KeyboardEvent*) const override;
    virtual bool isMouseFocusable() const override;
    virtual bool isFocusable() const override;
    virtual void updateFocusAppearance(bool /*restorePreviousSelection*/) override;
    virtual void setFocus(bool) override;

    enum Shape { Default, Poly, Rect, Circle, Unknown };
    Path getRegion(const LayoutSize&) const;
    void invalidateCachedRegion();

    OwnPtr<Path> m_region;
    std::unique_ptr<Length[]> m_coords;
    int m_coordsLen;
    LayoutSize m_lastSize;
    Shape m_shape;
};

NODE_TYPE_CASTS(HTMLAreaElement)

} //namespace

#endif
