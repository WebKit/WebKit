/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef StyleTreeResolver_h
#define StyleTreeResolver_h

#include "RenderStyleConstants.h"
#include "RenderTreePosition.h"
#include "SelectorChecker.h"
#include "SelectorFilter.h"
#include "StyleChange.h"
#include "StyleSharingResolver.h"
#include "StyleUpdate.h"
#include <functional>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class ContainerNode;
class Document;
class Element;
class HTMLSlotElement;
class Node;
class RenderStyle;
class Settings;
class ShadowRoot;
class StyleResolver;
class Text;
class TreeChange;

namespace Style {

class TreeResolver {
public:
    TreeResolver(Document&);
    ~TreeResolver();

    std::unique_ptr<const Update> resolve(Change);

private:
    Ref<RenderStyle> styleForElement(Element&, RenderStyle& inheritedStyle);

    void resolveComposedTree();
    ElementUpdate resolveElement(Element&);

    struct Scope : RefCounted<Scope> {
        StyleResolver& styleResolver;
        SelectorFilter selectorFilter;
        SharingResolver sharingResolver;
        ShadowRoot* shadowRoot { nullptr };
        Scope* enclosingScope { nullptr };

        Scope(Document&);
        Scope(ShadowRoot&, Scope& enclosingScope);
    };

    struct Parent {
        Element* element;
        Ref<RenderStyle> style;
        Change change;
        bool didPushScope { false };
        bool elementNeedingStyleRecalcAffectsNextSiblingElementStyle { false };

        Parent(Document&, Change);
        Parent(Element&, ElementUpdate&);
    };

    Scope& scope() { return m_scopeStack.last(); }
    Parent& parent() { return m_parentStack.last(); }

    void pushScope(ShadowRoot&);
    void pushEnclosingScope();
    void popScope();

    void pushParent(Element&, ElementUpdate&);
    void popParent();
    void popParentsToDepth(unsigned depth);

    Document& m_document;
    RefPtr<RenderStyle> m_documentElementStyle;

    Vector<Ref<Scope>, 4> m_scopeStack;
    Vector<Parent, 32> m_parentStack;

    std::unique_ptr<Update> m_update;
};

enum DetachType { NormalDetach, ReattachDetach };
void detachRenderTree(Element&, DetachType = NormalDetach);
void detachTextRenderer(Text&);

void queuePostResolutionCallback(std::function<void ()>);
bool postResolutionCallbacksAreSuspended();

bool isPlaceholderStyle(const RenderStyle&);

class PostResolutionCallbackDisabler {
public:
    explicit PostResolutionCallbackDisabler(Document&);
    ~PostResolutionCallbackDisabler();
};

}

}

#endif
