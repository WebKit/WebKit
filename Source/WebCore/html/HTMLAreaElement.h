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

#pragma once

#include "HTMLAnchorElement.h"
#include "LayoutRect.h"
#include <memory>

namespace WebCore {

class HTMLImageElement;
class HitTestResult;
class Path;

class HTMLAreaElement final : public HTMLAnchorElement {
    WTF_MAKE_ISO_ALLOCATED(HTMLAreaElement);
public:
    static Ref<HTMLAreaElement> create(const QualifiedName&, Document&);

    bool isDefault() const { return m_shape == Default; }

    bool mapMouseEvent(LayoutPoint location, const LayoutSize&, HitTestResult&);

    // FIXME: Use RenderElement& instead of RenderObject*.
    WEBCORE_EXPORT LayoutRect computeRect(RenderObject*) const;
    Path computePath(RenderObject*) const;
    Path computePathForFocusRing(const LayoutSize& elementSize) const;

    // The parent map's image.
    WEBCORE_EXPORT HTMLImageElement* imageElement() const;
    
private:
    HTMLAreaElement(const QualifiedName&, Document&);

    void parseAttribute(const QualifiedName&, const AtomString&) final;
    bool supportsFocus() const final;
    String target() const final;
    bool isKeyboardFocusable(KeyboardEvent*) const final;
    bool isMouseFocusable() const final;
    bool isFocusable() const final;
    RefPtr<Element> focusAppearanceUpdateTarget() final;
    void setFocus(bool, FocusVisibility = FocusVisibility::Invisible) final;

    enum Shape { Default, Poly, Rect, Circle, Unknown };
    Path getRegion(const LayoutSize&) const;
    void invalidateCachedRegion();

    std::unique_ptr<Path> m_region;
    Vector<double> m_coords;
    LayoutSize m_lastSize;
    Shape m_shape;
};

} //namespace
