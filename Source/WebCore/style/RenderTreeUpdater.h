/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "RenderTreePosition.h"
#include "StyleChange.h"
#include "StyleUpdate.h"
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace WebCore {

class ContainerNode;
class Document;
class Element;
class Node;
class RenderQuote;
class RenderStyle;
class Text;

class RenderTreeUpdater {
public:
    RenderTreeUpdater(Document&);

    void commit(std::unique_ptr<const Style::Update>);

    enum class TeardownType { Normal, KeepHoverAndActive };
    static void tearDownRenderers(Element&, TeardownType = TeardownType::Normal);
    static void tearDownRenderer(Text&);

    class FirstLetter;
    class ListItem;

private:
    void updateRenderTree(ContainerNode& root);
    void updateTextRenderer(Text&, const Style::TextUpdate*);
    void updateElementRenderer(Element&, const Style::ElementUpdate&);
    void createRenderer(Element&, RenderStyle&&);
    void invalidateWhitespaceOnlyTextSiblingsAfterAttachIfNeeded(Node&);
    void updateBeforeDescendants(Element&);
    void updateAfterDescendants(Element&, Style::Change);
    void updateBeforeOrAfterPseudoElement(Element&, PseudoId);

    struct Parent {
        Element* element { nullptr };
        Style::Change styleChange { Style::NoChange };
        std::optional<RenderTreePosition> renderTreePosition;

        Parent(ContainerNode& root);
        Parent(Element&, Style::Change);
    };
    Parent& parent() { return m_parentStack.last(); }
    RenderTreePosition& renderTreePosition();

    void pushParent(Element&, Style::Change);
    void popParent();
    void popParentsToDepth(unsigned depth);

    void updateQuotesUpTo(RenderQuote*);

    Document& m_document;
    std::unique_ptr<const Style::Update> m_styleUpdate;

    Vector<Parent> m_parentStack;

    HashSet<Text*> m_invalidatedWhitespaceOnlyTextSiblings;
    RenderQuote* m_previousUpdatedQuote { nullptr };
};

} // namespace WebCore
