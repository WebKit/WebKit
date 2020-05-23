/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#include "RenderTreeBuilder.h"
#include "RenderTreePosition.h"
#include "StyleChange.h"
#include "StyleTreeResolver.h"
#include "StyleUpdate.h"
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace WebCore {

class ContainerNode;
class Document;
class Element;
class Node;
class RenderStyle;
class Text;

class RenderTreeUpdater {
public:
    RenderTreeUpdater(Document&, Style::PostResolutionCallbackDisabler&);
    ~RenderTreeUpdater();

    void commit(std::unique_ptr<const Style::Update>);

    static void tearDownRenderers(Element&);
    static void tearDownRenderer(Text&);

private:
    class GeneratedContent;

    void updateRenderTree(ContainerNode& root);
    void updateTextRenderer(Text&, const Style::TextUpdate*);
    void createTextRenderer(Text&, const Style::TextUpdate*);
    void updateElementRenderer(Element&, const Style::ElementUpdate&);
    void updateRendererStyle(RenderElement&, RenderStyle&&, StyleDifference);
    void createRenderer(Element&, RenderStyle&&);
    void updateBeforeDescendants(Element&, const Style::ElementUpdates*);
    void updateAfterDescendants(Element&, const Style::ElementUpdates*);
    bool textRendererIsNeeded(const Text& textNode);
    void storePreviousRenderer(Node&);

    struct Parent {
        Element* element { nullptr };
        const Style::ElementUpdates* updates { nullptr };
        Optional<RenderTreePosition> renderTreePosition;

        bool didCreateOrDestroyChildRenderer { false };
        RenderObject* previousChildRenderer { nullptr };

        Parent(ContainerNode& root);
        Parent(Element&, const Style::ElementUpdates*);
    };
    Parent& parent() { return m_parentStack.last(); }
    Parent& renderingParent();
    RenderTreePosition& renderTreePosition();

    GeneratedContent& generatedContent() { return *m_generatedContent; }

    void pushParent(Element&, const Style::ElementUpdates*);
    void popParent();
    void popParentsToDepth(unsigned depth);

    enum class TeardownType { Full, RendererUpdate, RendererUpdateCancelingAnimations };
    static void tearDownRenderers(Element&, TeardownType, RenderTreeBuilder&);
    static void tearDownTextRenderer(Text&, RenderTreeBuilder&);
    static void tearDownLeftoverShadowHostChildren(Element&, RenderTreeBuilder&);
    static void tearDownLeftoverPaginationRenderersIfNeeded(Element&, RenderTreeBuilder&);

    RenderView& renderView();

    Document& m_document;
    std::unique_ptr<const Style::Update> m_styleUpdate;

    Vector<Parent> m_parentStack;

    std::unique_ptr<GeneratedContent> m_generatedContent;

    RenderTreeBuilder m_builder;
};

} // namespace WebCore
