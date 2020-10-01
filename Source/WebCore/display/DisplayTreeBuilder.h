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

#include <wtf/IsoMalloc.h>

namespace WebCore {

class LayoutSize;

namespace Layout {
class Box;
class BoxGeometry;
class ContainerBox;
class LayoutState;
}

namespace Display {

class Box;
class ContainerBox;
class Tree;

class TreeBuilder {
public:
    explicit TreeBuilder(float pixelSnappingFactor);

    std::unique_ptr<Tree> build(const Layout::LayoutState&) const;

private:
    std::unique_ptr<Box> displayBoxForRootBox(const Layout::BoxGeometry&, const Layout::ContainerBox&) const;
    std::unique_ptr<Box> displayBoxForLayoutBox(const Layout::BoxGeometry&, const Layout::Box&, LayoutSize offsetFromRoot) const;

    Box* recursiveBuildDisplayTree(const Layout::LayoutState&, LayoutSize offsetFromRoot, const Layout::Box&, Display::ContainerBox& parentDisplayBox, Display::Box* previousSiblingBox = nullptr) const;

    void buildInlineDisplayTree(const Layout::LayoutState&, LayoutSize offsetFromRoot, const Layout::ContainerBox&, Display::ContainerBox& parentDisplayBox) const;

    float m_pixelSnappingFactor { 1 };
};

#if ENABLE(TREE_DEBUGGING)
void showDisplayTree(const Box&);
#endif

} // namespace Display
} // namespace WebCore

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
