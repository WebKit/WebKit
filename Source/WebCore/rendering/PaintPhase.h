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

#pragma once

namespace WebCore {

/*
 *  The painting of a layer occurs in three distinct phases.  Each phase involves
 *  a recursive descent into the layer's render objects. The first phase is the background phase.
 *  The backgrounds and borders of all blocks are painted.  Inlines are not painted at all.
 *  Floats must paint above block backgrounds but entirely below inline content that can overlap them.
 *  In the foreground phase, all inlines are fully painted.  Inline replaced elements will get all
 *  three phases invoked on them during this phase.
 */

enum class PaintPhase : uint16_t {
    BlockBackground          = 0,
    ChildBlockBackground     = 1 << 0,
    ChildBlockBackgrounds    = 1 << 1,
    Float                    = 1 << 2,
    Foreground               = 1 << 3,
    Outline                  = 1 << 4,
    ChildOutlines            = 1 << 5,
    SelfOutline              = 1 << 6,
    Selection                = 1 << 7,
    CollapsedTableBorders    = 1 << 8,
    TextClip                 = 1 << 9,
    Mask                     = 1 << 10,
    ClippingMask             = 1 << 11,
    EventRegion              = 1 << 12,
};

enum class PaintBehavior : uint16_t {
    Normal                              = 0,
    SelectionOnly                       = 1 << 0,
    SkipSelectionHighlight              = 1 << 1,
    ForceBlackText                      = 1 << 2,
    ForceWhiteText                      = 1 << 3,
    RenderingSVGMask                    = 1 << 4,
    SkipRootBackground                  = 1 << 5,
    RootBackgroundOnly                  = 1 << 6,
    SelectionAndBackgroundsOnly         = 1 << 7,
    ExcludeSelection                    = 1 << 8,
    FlattenCompositingLayers            = 1 << 9, // Paint doesn't stop at compositing layer boundaries.
    Snapshotting                        = 1 << 10,
    TileFirstPaint                      = 1 << 11,
    CompositedOverflowScrollContent     = 1 << 12,
    AnnotateLinks                       = 1 << 13, // Collect all renderers with links to annotate their URLs (e.g. PDFs)
    EventRegionIncludeForeground        = 1 << 14, // FIXME: Event region painting should use paint phases.
    EventRegionIncludeBackground        = 1 << 15,
};

} // namespace WebCore
