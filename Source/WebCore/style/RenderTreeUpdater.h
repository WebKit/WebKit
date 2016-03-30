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

#ifndef RenderTreeUpdater_h
#define RenderTreeUpdater_h

#include "RenderTreePosition.h"
#include "StyleChange.h"
#include "StyleUpdate.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefPtr.h>
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
    RenderTreeUpdater(Document&);

    void commit(std::unique_ptr<const Style::Update>);

private:
    void updateRenderTree(ContainerNode& root);
    void updateTextRenderer(Text&);
    void updateElementRenderer(Element&, const Style::ElementUpdate&);
    void createRenderer(Element&, RenderStyle&);
    void invalidateWhitespaceOnlyTextSiblingsAfterAttachIfNeeded(Node&);
    void updateBeforeOrAfterPseudoElement(Element&, PseudoId);

    struct Parent {
        Element* element { nullptr };
        Style::Change styleChange { Style::NoChange };
        Optional<RenderTreePosition> renderTreePosition;

        Parent(ContainerNode& root);
        Parent(Element&, Style::Change);
    };
    Parent& parent() { return m_parentStack.last(); }
    RenderTreePosition& renderTreePosition();

    void pushParent(Element&, Style::Change);
    void popParent();
    void popParentsToDepth(unsigned depth);

    Document& m_document;
    std::unique_ptr<const Style::Update> m_styleUpdate;

    Vector<Parent> m_parentStack;

    HashSet<Text*> m_invalidatedWhitespaceOnlyTextSiblings;
};

}
#endif
