/*
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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

#include "AnchorPositionEvaluator.h"
#include "PropertyCascade.h"
#include "RenderStyle.h"
#include "ResolvedStyle.h"
#include "SelectorChecker.h"
#include "SelectorMatchingState.h"
#include "StyleChange.h"
#include "StylePositionTryFallback.h"
#include "StyleUpdate.h"
#include "Styleable.h"
#include "TreeResolutionState.h"
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
struct BuilderPositionTryFallback;
struct MatchResult;
struct PositionTryFallback;
struct PseudoElementIdentifier;
struct ResolutionContext;
struct ResolvedStyle;

enum class IsInDisplayNoneTree : bool { No, Yes };

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(TreeResolverScope);
class TreeResolver {
public:
    TreeResolver(Document&, std::unique_ptr<Update> = { });
    ~TreeResolver();

    std::unique_ptr<Update> resolve();

    bool needsInterleavedLayout() const { return m_needsInterleavedLayout; }

private:
    enum class ResolutionType : uint8_t { RebuildUsingExisting, AnimationOnly, FastPathInherit, FullWithMatchResultCache, Full };
    ResolvedStyle styleForStyleable(const Styleable&, ResolutionType, const ResolutionContext&, const RenderStyle* existingStyle);

    void resolveComposedTree();

    const RenderStyle* existingStyle(const Element&);

    enum class LayoutInterleavingAction : uint8_t { None, SkipDescendants };
    enum class DescendantsToResolve : uint8_t { None, RebuildAllUsingExisting, ChildrenWithExplicitInherit, Children, All };

    LayoutInterleavingAction updateStateForQueryContainer(Element&, const RenderStyle*, DescendantsToResolve&);

    // For elements requiring style/layout interleaving (anchor-positioned and query
    // containers), descendant resolution is deferred on the first style resolution
    // pass and resumed in subsequent passes.
    // When deferral is needed, deferDescendantResolution saves the internal state
    // relevant to which descendants should be resolved.
    void deferDescendantResolution(Element&, OptionSet<Change>, DescendantsToResolve);
    // When descendant resolution can be resumed, resumeDescendantResolutionIfNeeded
    // restores the previously saved state (if there is)
    void resumeDescendantResolutionIfNeeded(Element&, OptionSet<Change>&, DescendantsToResolve&);

    std::pair<ElementUpdate, DescendantsToResolve> resolveElement(Element&, const RenderStyle* existingStyle, ResolutionType);

    ElementUpdate createAnimatedElementUpdate(ResolvedStyle&&, const Styleable&, OptionSet<Change>, const ResolutionContext&, IsInDisplayNoneTree = IsInDisplayNoneTree::No);
    std::unique_ptr<RenderStyle> resolveStartingStyle(const ResolvedStyle&, const Styleable&, const ResolutionContext&);
    std::unique_ptr<RenderStyle> resolveAfterChangeStyleForNonAnimated(const ResolvedStyle&, const Styleable&, const ResolutionContext&);
    std::unique_ptr<RenderStyle> resolveAgainInDifferentContext(const ResolvedStyle&, const Styleable&, const RenderStyle& parentStyle,  OptionSet<PropertyCascade::PropertyType>, std::optional<BuilderPositionTryFallback>&&, const ResolutionContext&);
    const RenderStyle& parentAfterChangeStyle(const Styleable&, const ResolutionContext&) const;

    HashSet<AnimatableCSSProperty> applyCascadeAfterAnimation(RenderStyle&, const HashSet<AnimatableCSSProperty>&, bool isTransition, const MatchResult&, const Element&, const ResolutionContext&);

    std::optional<ElementUpdate> resolvePseudoElement(Element&, const PseudoElementIdentifier&, const ElementUpdate&, IsInDisplayNoneTree, const RenderStyle*);
    std::optional<ElementUpdate> resolveAncestorPseudoElement(Element&, const PseudoElementIdentifier&, const ElementUpdate&);
    std::optional<ResolvedStyle> resolveAncestorFirstLinePseudoElement(Element&, const ElementUpdate&);
    std::optional<ResolvedStyle> resolveAncestorFirstLetterPseudoElement(Element&, const ElementUpdate&, ResolutionContext&);

    void resetStyleForNonRenderedDescendants(Element&);

    struct Scope : RefCounted<Scope> {
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(TreeResolverScope, TreeResolverScope);
        Ref<Resolver> resolver;
        SelectorMatchingState selectorMatchingState;
        RefPtr<ShadowRoot> shadowRoot;
        RefPtr<Scope> enclosingScope;

        Scope(Document&, Update&);
        Scope(ShadowRoot&, Scope& enclosingScope);
        ~Scope();
    };

    struct Parent {
        Element* element;
        const RenderStyle& style;
        OptionSet<Change> changes;
        DescendantsToResolve descendantsToResolve { DescendantsToResolve::None };
        bool didPushScope { false };
        bool resolvedFirstLineAndLetterChild { false };
        bool needsUpdateQueryContainerDependentStyle { false };
        IsInDisplayNoneTree isInDisplayNoneTree { IsInDisplayNoneTree::No };

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        // Used to determine whether the AXObjectCache has already propagated down font and color updates for the current subtree.
        bool didAXUpdateFontSubtree { false };
        bool didAXUpdateTextColorSubtree { false };
#endif

        Parent(Document&);
        Parent(Element&, const RenderStyle&, OptionSet<Change>, DescendantsToResolve, IsInDisplayNoneTree);
    };

    Scope& scope() { return m_scopeStack.last(); }
    const Scope& scope() const { return m_scopeStack.last(); }

    Parent& parent() { return m_parentStack.last(); }
    const Parent& parent() const { return m_parentStack.last(); }

    void pushScope(ShadowRoot&);
    void pushEnclosingScope();
    void popScope();

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    void pushParent(Element&, const RenderStyle&, OptionSet<Change>, DescendantsToResolve, IsInDisplayNoneTree, bool, bool);
#else
    void pushParent(Element&, const RenderStyle&, OptionSet<Change>, DescendantsToResolve, IsInDisplayNoneTree);
#endif
    void popParent();
    void popParentsToDepth(unsigned depth);

    DescendantsToResolve computeDescendantsToResolve(const ElementUpdate&, const RenderStyle* existingStyle, Validity) const;
    static std::optional<ResolutionType> determineResolutionType(const Element&, const RenderStyle*, DescendantsToResolve, OptionSet<Change> parentChange);
    static void resetDescendantStyleRelations(Element&, DescendantsToResolve);

    ResolutionContext makeResolutionContext();
    ResolutionContext makeResolutionContextForPseudoElement(const ElementUpdate&, const PseudoElementIdentifier&);
    std::optional<ResolutionContext> makeResolutionContextForInheritedFirstLine(const ElementUpdate&, const RenderStyle& inheritStyle);
    const Parent* boxGeneratingParent() const;
    const RenderStyle* parentBoxStyle() const;
    const RenderStyle* parentBoxStyleForPseudoElement(const ElementUpdate&) const;
    const RenderStyle* documentElementStyle() const;

    LayoutInterleavingAction updateAnchorPositioningState(Element&, const RenderStyle*);

    void generatePositionOptionsIfNeeded(const ResolvedStyle&, const Styleable&, const ResolutionContext&);
    std::unique_ptr<RenderStyle> generatePositionOption(const PositionTryFallback&, const ResolvedStyle&, const Styleable&, const ResolutionContext&);
    struct PositionOptions;
    void sortPositionOptionsIfNeeded(PositionOptions&, const Styleable&);
    std::optional<ResolvedStyle> tryChoosePositionOption(const Styleable&, const ResolutionContext&);

    void updateForPositionVisibility(RenderStyle&, const Styleable&);

    // This returns the style that was in effect (applied to the render tree) before we started the style resolution.
    // Layout interleaving may cause different styles to be applied during the style resolution.
    const RenderStyle* beforeResolutionStyle(const Element&, std::optional<PseudoElementIdentifier>);
    void saveBeforeResolutionStyleForInterleaving(const Element&);

    bool hasUnresolvedAnchorPosition(const Styleable&) const;
    bool hasResolvedAnchorPosition(const Styleable&) const;

    void collectChangedAnchorNames(const RenderStyle&, const RenderStyle* currentStyle);

    const CheckedRef<Document> m_document;
    std::unique_ptr<RenderStyle> m_computedDocumentElementStyle;

    Vector<Ref<Scope>, 4> m_scopeStack;
    Vector<Parent, 32> m_parentStack;
    bool m_didSeePendingStylesheet { false };

    // States relevant to deferring and resuming descendant resolution.
    // Also see deferDescendantResolution and resumeDescendantResolutionIfNeeded.
    struct DeferredDescendantResolutionState {
        OptionSet<Change> changes;
        DescendantsToResolve descendantsToResolve { DescendantsToResolve::None };
    };
    HashMap<Ref<Element>, DeferredDescendantResolutionState> m_deferredDescendantResolutionStates;
    bool m_needsInterleavedLayout { false };
    bool m_didFirstInterleavedLayout { false };

    struct QueryContainerState {
        bool invalidated { false };
    };
    HashMap<Ref<Element>, QueryContainerState> m_queryContainerStates;

    // This state gets passes to the style builder and holds state for a single tree resolution, including over any interleaving.
    TreeResolutionState m_treeResolutionState;
    HashMap<Ref<const Element>, std::unique_ptr<RenderStyle>> m_savedBeforeResolutionStylesForInterleaving;

    struct PositionOption {
        // New style after applying the option.
        std::unique_ptr<RenderStyle> style;

        // The position option used to generate the style. If option is nullopt, no position option is used.
        std::optional<PositionTryFallback> option;

        // The size of the content box of the element when the style is generated.
        // Non-overlay scrollbars appearing or disappearing may affect the content box size that anchor functions are resolved against.
        std::optional<LayoutSize> scrollContainerSizeOnGeneration;
    };

    struct PositionOptions {
        // Array of option styles. By convention, the original style is at index 0.
        Vector<PositionOption> optionStyles { };
        ResolvedStyle originalResolvedStyle;
        size_t index { 0 };
        bool sorted { false };
        bool chosen { false };
        bool isFirstTry { true };

        const RenderStyle& originalStyle() const;
        std::unique_ptr<RenderStyle> currentOption() const;
    };
    HashMap<AnchorPositionedKey, PositionOptions> m_positionOptions;

    HashSet<AtomString> m_changedAnchorNames;
    bool m_allAnchorNamesInvalid { false };

    std::unique_ptr<Update> m_update;
};

// Integrate with the HTML5 event loop instead, see EventLoop.cpp and consumers.
void deprecatedQueuePostResolutionCallback(Function<void()>&&);
bool postResolutionCallbacksAreSuspended();

inline bool supportsFirstLineAndLetterPseudoElement(const RenderStyle& style)
{
    auto display = style.display();
    return display == DisplayType::Block
        || display == DisplayType::ListItem
        || display == DisplayType::InlineBlock
        || display == DisplayType::TableCell
        || display == DisplayType::TableCaption
        || display == DisplayType::FlowRoot;
}

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
