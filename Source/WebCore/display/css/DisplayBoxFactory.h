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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutSize.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

class RenderStyle;

namespace Layout {
class Box;
class BoxGeometry;
class ContainerBox;
class InlineLineGeometry;
class ReplacedBox;
struct LineRun;
}

namespace Display {

class Box;
class BoxDecorationData;
class BoxModelBox;
class Style;

enum class RootBackgroundPropagation : uint8_t {
    None,
    BodyToRoot,
};

class BoxFactory {
public:
    explicit BoxFactory(float pixelSnappingFactor);

    static RootBackgroundPropagation determineRootBackgroundPropagation(const Layout::ContainerBox& rootLayoutBox);

    std::unique_ptr<Box> displayBoxForRootBox(const Layout::ContainerBox&, const Layout::BoxGeometry&, RootBackgroundPropagation) const;
    std::unique_ptr<Box> displayBoxForBodyBox(const Layout::Box&, const Layout::BoxGeometry&, RootBackgroundPropagation, LayoutSize offsetFromRoot) const;
    std::unique_ptr<Box> displayBoxForLayoutBox(const Layout::Box&, const Layout::BoxGeometry&, LayoutSize offsetFromRoot) const;

    std::unique_ptr<Box> displayBoxForTextRun(const Layout::LineRun&, const Layout::InlineLineGeometry&, LayoutSize offsetFromRoot) const;

private:
    std::unique_ptr<Box> displayBoxForLayoutBox(const Layout::Box&, const RenderStyle* styleForBackground, const Layout::BoxGeometry&, LayoutSize offsetFromRoot, Style&&) const;

    void setupBoxGeometry(BoxModelBox&, const Layout::Box&, const Layout::BoxGeometry&, LayoutSize offsetFromRoot) const;
    void setupBoxModelBox(BoxModelBox&, const Layout::Box&, const RenderStyle* styleForBackground, const Layout::BoxGeometry&, LayoutSize offsetFromRoot) const;

    std::unique_ptr<BoxDecorationData> constructBoxDecorationData(const Layout::Box&, const RenderStyle* styleForBackground, const Layout::BoxGeometry&, LayoutSize offsetFromRoot) const;

    static const Layout::ContainerBox* documentElementBoxFromRootBox(const Layout::ContainerBox& rootLayoutBox);
    static const Layout::Box* bodyBoxFromRootBox(const Layout::ContainerBox& rootLayoutBox);

    float m_pixelSnappingFactor { 1 };
};

} // namespace Display
} // namespace WebCore


#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
