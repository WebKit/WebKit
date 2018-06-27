/*
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "SelectorChecker.h"
#include "SelectorFilter.h"
#include "StyleChange.h"
#include "StyleSharingResolver.h"
#include "StyleUpdate.h"
#include <wtf/Function.h>
#include <wtf/Ref.h>

namespace WebCore {

class Document;
class Element;
class Node;
class RenderStyle;
class ShadowRoot;
class StyleResolver;

namespace Style {

class TreeResolver {
public:
    TreeResolver(Document&);
    ~TreeResolver();

    std::unique_ptr<Update> resolve();

private:
    std::unique_ptr<RenderStyle> styleForElement(Element&, const RenderStyle& inheritedStyle);

    void resolveComposedTree();

    ElementUpdates resolveElement(Element&);

    ElementUpdate createAnimatedElementUpdate(std::unique_ptr<RenderStyle>, Element&, Change);
    ElementUpdate resolvePseudoStyle(Element&, const ElementUpdate&, PseudoId);

    struct Scope : RefCounted<Scope> {
        StyleResolver& styleResolver;
        SelectorFilter selectorFilter;
        SharingResolver sharingResolver;
        ShadowRoot* shadowRoot { nullptr };
        Scope* enclosingScope { nullptr };

        Scope(Document&);
        Scope(ShadowRoot&, Scope& enclosingScope);
        ~Scope();
    };

    struct Parent {
        Element* element;
        const RenderStyle& style;
        Change change { NoChange };
        DescendantsToResolve descendantsToResolve { DescendantsToResolve::None };
        bool didPushScope { false };

        Parent(Document&);
        Parent(Element&, const RenderStyle&, Change, DescendantsToResolve);
    };

    Scope& scope() { return m_scopeStack.last(); }
    Parent& parent() { return m_parentStack.last(); }

    void pushScope(ShadowRoot&);
    void pushEnclosingScope();
    void popScope();

    void pushParent(Element&, const RenderStyle&, Change, DescendantsToResolve);
    void popParent();
    void popParentsToDepth(unsigned depth);

    const RenderStyle* parentBoxStyle() const;

    Document& m_document;
    std::unique_ptr<RenderStyle> m_documentElementStyle;

    Vector<Ref<Scope>, 4> m_scopeStack;
    Vector<Parent, 32> m_parentStack;
    bool m_didSeePendingStylesheet { false };

    std::unique_ptr<Update> m_update;
};

void queuePostResolutionCallback(Function<void ()>&&);
bool postResolutionCallbacksAreSuspended();

class PostResolutionCallbackDisabler {
public:
    enum class DrainCallbacks { Yes, No };
    explicit PostResolutionCallbackDisabler(Document&, DrainCallbacks = DrainCallbacks::Yes);
    ~PostResolutionCallbackDisabler();
private:
    DrainCallbacks m_drainCallbacks;
};

}

}
