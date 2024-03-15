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
#include "SelectorMatchingState.h"
#include "StyleChange.h"
#include "StyleSharingResolver.h"
#include "StyleUpdate.h"
#include "Styleable.h"
#include <wtf/Function.h>
#include <wtf/Ref.h>

namespace WebCore {

class Document;
class Element;
class Node;
class RenderStyle;
class ShadowRoot;

namespace Style {

class Resolver;
struct MatchResult;
struct ResolutionContext;
struct ResolvedStyle;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(TreeResolverScope);
class TreeResolver {
public:
    TreeResolver(Document&, std::unique_ptr<Update> = { });
    ~TreeResolver();

    std::unique_ptr<Update> resolve();

    bool hasUnresolvedQueryContainers() const { return m_hasUnresolvedQueryContainers; }

private:
    enum class ResolutionType : uint8_t { RebuildUsingExisting, AnimationOnly, FastPathInherit, Full };
    ResolvedStyle styleForStyleable(const Styleable&, ResolutionType, const ResolutionContext&);

    void resolveComposedTree();

    const RenderStyle* existingStyle(const Element&);

    enum class QueryContainerAction : uint8_t { None, Resolve, Continue };
    enum class DescendantsToResolve : uint8_t { None, RebuildAllUsingExisting, ChildrenWithExplicitInherit, Children, All };

    QueryContainerAction updateStateForQueryContainer(Element&, const RenderStyle*, Change&, DescendantsToResolve&);

    std::pair<ElementUpdate, DescendantsToResolve> resolveElement(Element&, const RenderStyle* existingStyle, ResolutionType);

    ElementUpdate createAnimatedElementUpdate(ResolvedStyle&&, const Styleable&, Change, const ResolutionContext&);
    std::unique_ptr<RenderStyle> resolveStartingStyle(const ResolvedStyle&, const Styleable&, const ResolutionContext&) const;
    HashSet<AnimatableCSSProperty> applyCascadeAfterAnimation(RenderStyle&, const HashSet<AnimatableCSSProperty>&, bool isTransition, const MatchResult&, const Element&, const ResolutionContext&);

    std::optional<ElementUpdate> resolvePseudoElement(Element&, PseudoId, const ElementUpdate&);
    std::optional<ElementUpdate> resolveAncestorPseudoElement(Element&, PseudoId, const ElementUpdate&);
    std::optional<ResolvedStyle> resolveAncestorFirstLinePseudoElement(Element&, const ElementUpdate&);
    std::optional<ResolvedStyle> resolveAncestorFirstLetterPseudoElement(Element&, const ElementUpdate&, ResolutionContext&);

    struct Scope : RefCounted<Scope> {
        WTF_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(TreeResolverScope);
        Ref<Resolver> resolver;
        SelectorMatchingState selectorMatchingState;
        SharingResolver sharingResolver;
        RefPtr<ShadowRoot> shadowRoot;
        RefPtr<Scope> enclosingScope;

        Scope(Document&);
        Scope(ShadowRoot&, Scope& enclosingScope);
        ~Scope();
    };

    struct Parent {
        Element* element;
        const RenderStyle& style;
        Change change { Change::None };
        DescendantsToResolve descendantsToResolve { DescendantsToResolve::None };
        bool didPushScope { false };
        bool resolvedFirstLineAndLetterChild { false };
        bool needsUpdateQueryContainerDependentStyle { false };

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

    static DescendantsToResolve computeDescendantsToResolve(Change, Validity, DescendantsToResolve);
    static std::optional<ResolutionType> determineResolutionType(const Element&, const RenderStyle*, DescendantsToResolve, Change parentChange);
    static void resetDescendantStyleRelations(Element&, DescendantsToResolve);

    ResolutionContext makeResolutionContext();
    ResolutionContext makeResolutionContextForPseudoElement(const ElementUpdate&, PseudoId);
    std::optional<ResolutionContext> makeResolutionContextForInheritedFirstLine(const ElementUpdate&, const RenderStyle& inheritStyle);
    const Parent* boxGeneratingParent() const;
    const RenderStyle* parentBoxStyle() const;
    const RenderStyle* parentBoxStyleForPseudoElement(const ElementUpdate&) const;

    struct QueryContainerState {
        Change change { Change::None };
        DescendantsToResolve descendantsToResolve { DescendantsToResolve::None };
    };

    Document& m_document;
    std::unique_ptr<RenderStyle> m_documentElementStyle;

    Vector<Ref<Scope>, 4> m_scopeStack;
    Vector<Parent, 32> m_parentStack;
    bool m_didSeePendingStylesheet { false };

    HashMap<Ref<Element>, std::optional<QueryContainerState>> m_queryContainerStates;
    bool m_hasUnresolvedQueryContainers { false };

    std::unique_ptr<Update> m_update;
};

// Integrate with the HTML5 event loop instead, see EventLoop.cpp and consumers.
void deprecatedQueuePostResolutionCallback(Function<void()>&&);
bool postResolutionCallbacksAreSuspended();

class PostResolutionCallbackDisabler {
public:
    enum class DrainCallbacks : bool { No, Yes };
    explicit PostResolutionCallbackDisabler(Document&, DrainCallbacks = DrainCallbacks::Yes);
    ~PostResolutionCallbackDisabler();
private:
    DrainCallbacks m_drainCallbacks;
};

}

}
