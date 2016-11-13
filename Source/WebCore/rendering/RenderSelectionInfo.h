/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "GapRects.h"
#include "RenderBlock.h"
#include "RenderObject.h"

namespace WebCore {

class RenderSelectionInfoBase {
    WTF_MAKE_NONCOPYABLE(RenderSelectionInfoBase); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit RenderSelectionInfoBase(RenderObject& renderer);
    RenderLayerModelObject* repaintContainer() const { return m_repaintContainer; }
    RenderObject::SelectionState state() const { return m_state; }

protected:
    void repaintRectangle(const LayoutRect& repaintRect);

    RenderObject& m_renderer;
    RenderLayerModelObject* m_repaintContainer;

private:
    RenderObject::SelectionState m_state;
};

// This struct is used when the selection changes to cache the old and new state of the selection for each RenderObject.
class RenderSelectionInfo : public RenderSelectionInfoBase {
public:
    RenderSelectionInfo(RenderObject& renderer, bool clipToVisibleContent);

    void repaint();
    const Vector<LayoutRect>& collectedSelectionRects() const { return m_collectedSelectionRects; }
    LayoutRect rect() const { return m_rect; }

private:
    Vector<LayoutRect> m_collectedSelectionRects; // relative to repaint container
    LayoutRect m_rect; // relative to repaint container
};


// This struct is used when the selection changes to cache the old and new state of the selection for each RenderBlock.
class RenderBlockSelectionInfo : public RenderSelectionInfoBase {
public:
    explicit RenderBlockSelectionInfo(RenderBlock& renderer);

    void repaint();
    GapRects rects() const { return m_rects; }

private:
    GapRects m_rects; // relative to repaint container
};

} // namespace WebCore
