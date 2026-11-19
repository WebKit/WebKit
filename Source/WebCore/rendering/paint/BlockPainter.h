/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2016 Google Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
 */

#pragma once

#include "LayoutPoint.h"
#include "RenderBlock.h"
#include <wtf/CheckedRef.h>

namespace WebCore {

class RenderBox;
struct PaintInfo;

namespace Paint {

class BlockPainter {
public:
    BlockPainter(RenderBlock&, PaintInfo&, const LayoutPoint& paintOffset);

    void paint();
    void paintObject();

    void paintChildren(PaintInfo& paintInfoForChild, bool usePrintRect);

    enum PaintBlockType { PaintAsBlock, PaintAsInlineBlock };
    bool paintChild(RenderBox&, PaintInfo& paintInfoForChild, bool usePrintRect, PaintBlockType = PaintAsBlock);

    void paintBlockLevelContentInInline();

    static PaintInfo paintInfoForBlockChildren(const RenderBlock&, const PaintInfo&);

private:
    void paintContents(const LayoutPoint&);
    void paintInlineChildren(const LayoutPoint&);
    void paintFloats(const LayoutPoint&, bool preservePhase);
    void paintColumnRules(const LayoutPoint&);
    void paintExcludedChildrenInBorder();
    void paintCarets();
    void paintCaret(CaretType);

    const CheckedRef<RenderBlock> m_block;
    PaintInfo& m_paintInfo;
    const LayoutPoint m_paintOffset;
};

} // namespace Paint
} // namespace WebCore
