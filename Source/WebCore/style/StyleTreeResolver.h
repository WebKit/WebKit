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
#include "SelectorFilter.h"
#include "StyleChange.h"
#include "StyleSharingResolver.h"
#include <functional>
#include <wtf/RefPtr.h>

namespace WebCore {

class ContainerNode;
class Document;
class Element;
class HTMLSlotElement;
class Node;
class RenderStyle;
class RenderTreePosition;
class Settings;
class ShadowRoot;
class StyleResolver;
class Text;

namespace Style {

class TreeResolver {
public:
    TreeResolver(Document&);

    void resolve(Change);

private:
    void resolveShadowTree(Change, RenderStyle& inheritedStyle);

    Ref<RenderStyle> styleForElement(Element&, RenderStyle& inheritedStyle);

    void resolveRecursively(Element&, RenderStyle& inheritedStyle, RenderTreePosition&, Change);
    Change resolveLocally(Element&, RenderStyle& inheritedStyle, RenderTreePosition&, Change inheritedChange);
    void resolveChildren(Element&, RenderStyle&, Change, RenderTreePosition&);
    void resolveChildAtShadowBoundary(Node&, RenderStyle& inheritedStyle, RenderTreePosition&, Change);
    void resolveBeforeOrAfterPseudoElement(Element&, Change, PseudoId, RenderTreePosition&);

    void createRenderTreeRecursively(Element&, RenderStyle&, RenderTreePosition&, RefPtr<RenderStyle>&& resolvedStyle);
    void createRenderer(Element&, RenderStyle& inheritedStyle, RenderTreePosition&, RefPtr<RenderStyle>&& resolvedStyle);
    void createRenderTreeForBeforeOrAfterPseudoElement(Element&, PseudoId, RenderTreePosition&);
    void createRenderTreeForChildren(ContainerNode&, RenderStyle&, RenderTreePosition&);
    void createRenderTreeForShadowRoot(ShadowRoot&);

#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
    void resolveSlotAssignees(HTMLSlotElement&, RenderStyle& inheritedStyle, RenderTreePosition&, Change);
    void createRenderTreeForSlotAssignees(HTMLSlotElement&, RenderStyle& inheritedStyle, RenderTreePosition&);
#endif

    struct Scope : RefCounted<Scope> {
        StyleResolver& styleResolver;
        SelectorFilter selectorFilter;
        SharingResolver sharingResolver;
        ShadowRoot* shadowRoot { nullptr };
        Scope* enclosingScope { nullptr };

        Scope(Document&);
        Scope(ShadowRoot&, Scope& enclosingScope);
    };
    Scope& scope() { return m_scopeStack.last(); }
    void pushScope(ShadowRoot&);
    void pushEnclosingScope();
    void popScope();

    Document& m_document;
    Vector<Ref<Scope>, 4> m_scopeStack;
};

void detachRenderTree(Element&);
void detachTextRenderer(Text&);

void updateTextRendererAfterContentChange(Text&, unsigned offsetOfReplacedData, unsigned lengthOfReplacedData);

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
