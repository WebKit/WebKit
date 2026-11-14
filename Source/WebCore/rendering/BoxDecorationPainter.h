/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2005-2025 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2003-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2019 Google Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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

#include "RenderBoxModelObject.h"

namespace WebCore {

class GraphicsContext;
class RenderBox;
class RenderTable;
class RenderTableCell;
class RenderView;
struct PaintInfo;

class BoxDecorationPainter {
public:
    BoxDecorationPainter(RenderBox&, PaintInfo&, const LayoutPoint& paintOffset);

    void paintBoxDecorations();

private:
    void paintTableDecorations(RenderTable&);
    void paintTableCellDecorations(RenderTableCell&);
    void paintViewBackground(RenderView&);

    BleedAvoidance determineBleedAvoidance() const;
    bool backgroundHasOpaqueTopLayer() const;

    const CheckedRef<RenderBox> m_renderer;
    PaintInfo& m_paintInfo;
    const LayoutPoint m_paintOffset;
};

}
