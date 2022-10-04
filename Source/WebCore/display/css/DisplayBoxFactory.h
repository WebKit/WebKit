/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "LayoutSize.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

class FloatPoint3D;
class RenderStyle;
class TransformationMatrix;

namespace Layout {
class Box;
class BoxGeometry;
class ElementBox;
struct Run;
}

namespace InlineDisplay {
struct Box;
class Line;
}

namespace Display {

class Box;
class BoxDecorationData;
class BoxModelBox;
class BoxRareGeometry;
class ContainerBox;
class Style;
class TreeBuilder;

enum class RootBackgroundPropagation : uint8_t {
    None,
    BodyToRoot,
};

struct ContainingBlockContext {
    const Display::ContainerBox& box;
    LayoutSize offsetFromRoot;
};

class BoxFactory {
public:
    BoxFactory(TreeBuilder&, float pixelSnappingFactor);
    
    static RootBackgroundPropagation determineRootBackgroundPropagation(const Layout::ElementBox& rootLayoutBox);

    std::unique_ptr<ContainerBox> displayBoxForRootBox(const Layout::ElementBox&, const Layout::BoxGeometry&, RootBackgroundPropagation) const;
    std::unique_ptr<Box> displayBoxForBodyBox(const Layout::Box&, const Layout::BoxGeometry&, const ContainingBlockContext&, RootBackgroundPropagation) const;
    std::unique_ptr<Box> displayBoxForLayoutBox(const Layout::Box&, const Layout::BoxGeometry&, const ContainingBlockContext&) const;

    std::unique_ptr<Box> displayBoxForTextRun(const InlineDisplay::Box&, const InlineDisplay::Line&, const ContainingBlockContext&) const;

private:
    std::unique_ptr<Box> displayBoxForLayoutBox(const Layout::Box&, const Layout::BoxGeometry&, const ContainingBlockContext&, const RenderStyle* styleForBackground, Style&&) const;

    void setupBoxGeometry(BoxModelBox&, const Layout::Box&, const Layout::BoxGeometry&, const ContainingBlockContext&) const;
    void setupBoxModelBox(BoxModelBox&, const Layout::Box&, const Layout::BoxGeometry&, const ContainingBlockContext&, const RenderStyle* styleForBackground) const;

    std::unique_ptr<BoxDecorationData> constructBoxDecorationData(const Layout::Box&, const Layout::BoxGeometry&, const RenderStyle* styleForBackground, LayoutSize offsetFromRoot) const;

    std::unique_ptr<BoxRareGeometry> constructBoxRareGeometry(const BoxModelBox& box, const Layout::Box&, const Layout::BoxGeometry&, LayoutSize offsetFromRoot) const;

    FloatPoint3D computeTransformOrigin(const BoxModelBox&, const Layout::BoxGeometry&, const RenderStyle&, LayoutSize offsetFromRoot) const;
    TransformationMatrix computeTransformationMatrix(const BoxModelBox&, const Layout::BoxGeometry&, const RenderStyle&, LayoutSize offsetFromRoot) const;

    static const Layout::ElementBox* documentElementBoxFromRootBox(const Layout::ElementBox& rootLayoutBox);
    static const Layout::Box* bodyBoxFromRootBox(const Layout::ElementBox& rootLayoutBox);

    TreeBuilder& m_treeBuilder;
    float m_pixelSnappingFactor { 1 };
};

} // namespace Display
} // namespace WebCore


