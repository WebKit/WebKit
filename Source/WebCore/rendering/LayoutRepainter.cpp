/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "config.h"
#include "LayoutRepainter.h"

#include "RenderElement.h"

namespace WebCore {

LayoutRepainter::LayoutRepainter(RenderElement& renderer, bool checkForRepaint, RepaintOutlineBounds repaintOutlineBounds)
    : m_renderer(renderer)
    , m_checkForRepaint(checkForRepaint)
    , m_repaintOutlineBounds(repaintOutlineBounds == RepaintOutlineBounds::Yes)
{
    if (!m_checkForRepaint)
        return;

    m_repaintContainer = m_renderer.containerForRepaint().renderer;
    m_oldBounds = m_renderer.clippedOverflowRectForRepaint(m_repaintContainer);

    if (m_repaintOutlineBounds)
        m_oldOutlineBounds = m_renderer.outlineBoundsForRepaint(m_repaintContainer);
}

bool LayoutRepainter::repaintAfterLayout()
{
    if (!m_checkForRepaint)
        return false;

    auto newBounds = m_renderer.clippedOverflowRectForRepaint(m_repaintContainer);
    auto newOutlineBounds = m_repaintOutlineBounds ? nullptr : &m_oldOutlineBounds;

    return m_renderer.repaintAfterLayoutIfNeeded(m_repaintContainer, m_oldBounds, m_oldOutlineBounds, &newBounds, newOutlineBounds);
}

} // namespace WebCore
