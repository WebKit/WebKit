/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#include "config.h"
#include "SelectorCompiler.h"

#if ENABLE(CSS_SELECTOR_JIT)

#include "CSSSelector.h"
#include "Element.h"
#include "ElementData.h"
#include "FunctionCall.h"
#include "HTMLNames.h"
#include "NodeRenderStyle.h"
#include "QualifiedName.h"
#include "RegisterAllocator.h"
#include "RenderElement.h"
#include "RenderStyle.h"
#include "SVGElement.h"
#include "StackAllocator.h"
#include "StyledElement.h"
#include <JavaScriptCore/LinkBuffer.h>
#include <JavaScriptCore/MacroAssembler.h>
#include <JavaScriptCore/VM.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace WebCore {
namespace SelectorCompiler {

#define CSS_SELECTOR_JIT_DEBUGGING 0

enum class BacktrackingAction {
    NoBacktracking,
    JumpToDescendantEntryPoint,
    JumpToIndirectAdjacentEntryPoint,
    JumpToDescendantTreeWalkerEntryPoint,
    JumpToDescendantTail,
    JumpToClearAdjacentDescendantTail,
    JumpToDirectAdjacentTail
};

struct BacktrackingFlag {
    enum {
        DescendantEntryPoint = 1,
        IndirectAdjacentEntryPoint = 1 << 1,
        SaveDescendantBacktrackingStart = 1 << 2,
        SaveAdjacentBacktrackingStart = 1 << 3,
        DirectAdjacentTail = 1 << 4,
        DescendantTail = 1 << 5,
    };
};

enum class FragmentRelation {
    Rightmost,
    Descendant,
    Child,
    DirectAdjacent,
    IndirectAdjacent
};

enum class FunctionType {
    SimpleSelectorChecker,
    SelectorCheckerWithCheckingContext,
    CannotCompile
};

struct SelectorFragment {
    SelectorFragment()
        : traversalBacktrackingAction(BacktrackingAction::NoBacktracking)
        , matchingBacktrackingAction(BacktrackingAction::NoBacktracking)
        , backtrackingFlags(0)
        , tagName(nullptr)
        , id(nullptr)
    {
    }
    FragmentRelation relationToLeftFragment;
    FragmentRelation relationToRightFragment;

    BacktrackingAction traversalBacktrackingAction;
    BacktrackingAction matchingBacktrackingAction;
    unsigned char backtrackingFlags;

    const QualifiedName* tagName;
    const AtomicString* id;
    Vector<const AtomicStringImpl*, 1> classNames;
    HashSet<unsigned> pseudoClasses;
    Vector<const CSSSelector*> attributes;
};

typedef JSC::MacroAssembler Assembler;
typedef Vector<SelectorFragment, 8> SelectorFragmentList;

class SelectorCodeGenerator {
public:
    SelectorCodeGenerator(const CSSSelector*);
    SelectorCompilationStatus compile(JSC::VM*, JSC::MacroAssemblerCodeRef&);

private:
#if CPU(X86_64)
    static const Assembler::RegisterID returnRegister = JSC::X86Registers::eax;
    static const Assembler::RegisterID elementAddressRegister = JSC::X86Registers::edi;
    static const Assembler::RegisterID checkingContextRegister = JSC::X86Registers::esi;
#endif

    void computeBacktrackingInformation();

    // Element relations tree walker.
    void generateWalkToParentElement(Assembler::JumpList& failureCases, Assembler::RegisterID targetRegister);
    void generateParentElementTreeWalker(Assembler::JumpList& failureCases, const SelectorFragment&);
    void generateAncestorTreeWalker(Assembler::JumpList& failureCases, const SelectorFragment&);

    void generateWalkToPreviousAdjacent(Assembler::JumpList& failureCases, const SelectorFragment&);
    void generateDirectAdjacentTreeWalker(Assembler::JumpList& failureCases, const SelectorFragment&);
    void generateIndirectAdjacentTreeWalker(Assembler::JumpList& failureCases, const SelectorFragment&);
    void markParentElementIfResolvingStyle(JSC::FunctionPtr);

    void linkFailures(Assembler::JumpList& globalFailureCases, BacktrackingAction, Assembler::JumpList& localFailureCases);
    void generateAdjacentBacktrackingTail(StackAllocator& adjacentTailStack);
    void generateDescendantBacktrackingTail();
    void generateBacktrackingTailsIfNeeded(const SelectorFragment&);

    // Element properties matchers.
    void generateElementMatching(Assembler::JumpList& failureCases, const SelectorFragment&);
    void generateElementDataMatching(Assembler::JumpList& failureCases, const SelectorFragment&);
    void generateSynchronizeStyleAttribute(Assembler::RegisterID elementDataArraySizeAndFlags);
    void generateSynchronizeAllAnimatedSVGAttribute(Assembler::RegisterID elementDataArraySizeAndFlags);
    void generateElementAttributesMatching(Assembler::JumpList& failureCases, const LocalRegister& elementDataAddress, const SelectorFragment&);
    void generateElementAttributeMatching(Assembler::JumpList& failureCases, Assembler::RegisterID currentAttributeAddress, Assembler::RegisterID decIndexRegister, const CSSSelector* attributeSelector);
    void generateElementHasTagName(Assembler::JumpList& failureCases, const QualifiedName& nameToMatch);
    void generateElementHasId(Assembler::JumpList& failureCases, const LocalRegister& elementDataAddress, const AtomicString& idToMatch);
    void generateElementHasClasses(Assembler::JumpList& failureCases, const LocalRegister& elementDataAddress, const Vector<const AtomicStringImpl*>& classNames);
    void generateElementIsFocused(Assembler::JumpList& failureCases);
    void generateElementIsLink(Assembler::JumpList& failureCases);

    Assembler m_assembler;
    RegisterAllocator m_registerAllocator;
    StackAllocator m_stackAllocator;
    Vector<std::pair<Assembler::Call, JSC::FunctionPtr>> m_functionCalls;

    FunctionType m_functionType;
    SelectorFragmentList m_selectorFragments;
    bool m_selectorCannotMatchAnything;

    StackAllocator::StackReference m_checkingContextStackReference;

    Assembler::Label m_descendantEntryPoint;
    Assembler::Label m_indirectAdjacentEntryPoint;
    Assembler::Label m_descendantTreeWalkerBacktrackingPoint;
    Assembler::RegisterID m_descendantBacktrackingStart;
    Assembler::JumpList m_descendantBacktrackingFailureCases;
    StackAllocator::StackReference m_adjacentBacktrackingStart;
    Assembler::JumpList m_adjacentBacktrackingFailureCases;
    Assembler::JumpList m_clearAdjacentEntryPointDescendantFailureCases;

#if CSS_SELECTOR_JIT_DEBUGGING
    const CSSSelector* m_originalSelector;
#endif
};

SelectorCompilationStatus compileSelector(const CSSSelector* lastSelector, JSC::VM* vm, JSC::MacroAssemblerCodeRef& codeRef)
{
    if (!vm->canUseJIT())
        return SelectorCompilationStatus::CannotCompile;
    SelectorCodeGenerator codeGenerator(lastSelector);
    return codeGenerator.compile(vm, codeRef);
}

static inline FragmentRelation fragmentRelationForSelectorRelation(CSSSelector::Relation relation)
{
    switch (relation) {
    case CSSSelector::Descendant:
        return FragmentRelation::Descendant;
    case CSSSelector::Child:
        return FragmentRelation::Child;
    case CSSSelector::DirectAdjacent:
        return FragmentRelation::DirectAdjacent;
    case CSSSelector::IndirectAdjacent:
        return FragmentRelation::IndirectAdjacent;
    case CSSSelector::SubSelector:
    case CSSSelector::ShadowDescendant:
        ASSERT_NOT_REACHED();
    }
    ASSERT_NOT_REACHED();
    return FragmentRelation::Descendant;
}

static inline FunctionType mostRestrictiveFunctionType(FunctionType a, FunctionType b)
{
    return std::max(a, b);
}

static inline FunctionType addPseudoType(CSSSelector::PseudoType type, HashSet<unsigned>& pseudoClasses)
{
    switch (type) {
    case CSSSelector::PseudoAnyLink:
    case CSSSelector::PseudoLink:
        pseudoClasses.add(CSSSelector::PseudoLink);
        return FunctionType::SimpleSelectorChecker;
    case CSSSelector::PseudoFocus:
        pseudoClasses.add(CSSSelector::PseudoFocus);
        return FunctionType::SimpleSelectorChecker;
    default:
        break;
    }
    return FunctionType::CannotCompile;
}

inline SelectorCodeGenerator::SelectorCodeGenerator(const CSSSelector* rootSelector)
    : m_stackAllocator(m_assembler)
    , m_functionType(FunctionType::SimpleSelectorChecker)
    , m_selectorCannotMatchAnything(false)
#if CSS_SELECTOR_JIT_DEBUGGING
    , m_originalSelector(rootSelector)
#endif
{
#if CSS_SELECTOR_JIT_DEBUGGING
    dataLogF("Compiling \"%s\"\n", m_originalSelector->selectorText().utf8().data());
#endif

    SelectorFragment fragment;
    FragmentRelation relationToPreviousFragment = FragmentRelation::Rightmost;
    for (const CSSSelector* selector = rootSelector; selector; selector = selector->tagHistory()) {
        switch (selector->m_match) {
        case CSSSelector::Tag:
            ASSERT(!fragment.tagName);
            fragment.tagName = &(selector->tagQName());
            break;
        case CSSSelector::Id: {
            const AtomicString& id = selector->value();
            if (fragment.id) {
                if (id != *fragment.id)
                    goto InconsistentSelector;
            } else
                fragment.id = &(selector->value());
            break;
        }
        case CSSSelector::Class:
            fragment.classNames.append(selector->value().impl());
            break;
        case CSSSelector::PseudoClass:
            m_functionType = mostRestrictiveFunctionType(m_functionType, addPseudoType(selector->pseudoType(), fragment.pseudoClasses));
            if (m_functionType == FunctionType::CannotCompile)
                goto CannotHandleSelector;
            break;
        case CSSSelector::Set:
            fragment.attributes.append(selector);
            break;
        case CSSSelector::Unknown:
        case CSSSelector::Exact:
        case CSSSelector::List:
        case CSSSelector::Hyphen:
        case CSSSelector::PseudoElement:
        case CSSSelector::Contain:
        case CSSSelector::Begin:
        case CSSSelector::End:
        case CSSSelector::PagePseudoClass:
            goto CannotHandleSelector;
        }

        CSSSelector::Relation relation = selector->relation();
        if (relation == CSSSelector::SubSelector)
            continue;

        if (relation == CSSSelector::ShadowDescendant && !selector->isLastInTagHistory())
            goto CannotHandleSelector;

        if (relation == CSSSelector::DirectAdjacent || relation == CSSSelector::IndirectAdjacent)
            m_functionType = std::max(m_functionType, FunctionType::SelectorCheckerWithCheckingContext);

        fragment.relationToLeftFragment = fragmentRelationForSelectorRelation(relation);
        fragment.relationToRightFragment = relationToPreviousFragment;
        relationToPreviousFragment = fragment.relationToLeftFragment;

        m_selectorFragments.append(fragment);
        fragment = SelectorFragment();
    }

    computeBacktrackingInformation();

    return;
InconsistentSelector:
    m_functionType = FunctionType::SimpleSelectorChecker;
    m_selectorCannotMatchAnything = true;
CannotHandleSelector:
    m_selectorFragments.clear();
}

static inline unsigned minimumRegisterRequirements(const SelectorFragmentList& selectorFragments)
{
    // Strict minimum to match anything interesting:
    // Element + BacktrackingRegister + ElementData + a pointer to values + an index on that pointer + the value we expect;
    unsigned minimum = 6;

    // Attributes cause some register pressure.
    for (unsigned selectorFragmentIndex = 0; selectorFragmentIndex < selectorFragments.size(); ++selectorFragmentIndex) {
        const SelectorFragment& selectorFragment = selectorFragments[selectorFragmentIndex];
        const Vector<const CSSSelector*>& attributes = selectorFragment.attributes;

        for (unsigned attributeIndex = 0; attributeIndex < attributes.size(); ++attributeIndex) {
            // Element + ElementData + scratchRegister + attributeArrayPointer + expectedLocalName + qualifiedNameImpl.
            unsigned attributeMinimum = 6;
            if (selectorFragment.traversalBacktrackingAction == BacktrackingAction::JumpToDescendantTail
                || selectorFragment.matchingBacktrackingAction == BacktrackingAction::JumpToDescendantTail)
                attributeMinimum += 1; // If there is a DescendantTail, there is a backtracking register.

            if (attributes.size() != 1)
                attributeMinimum += 2; // For the local copy of the counter and attributeArrayPointer.

            const CSSSelector* attributeSelector = attributes[attributeIndex];
            if (attributeSelector->attribute().prefix() != starAtom && !attributeSelector->attribute().namespaceURI().isNull())
                attributeMinimum += 1; // Additional register for the expected namespace.

            minimum = std::max(minimum, attributeMinimum);
        }
    }

    return minimum;
}

inline SelectorCompilationStatus SelectorCodeGenerator::compile(JSC::VM* vm, JSC::MacroAssemblerCodeRef& codeRef)
{
    if (m_selectorFragments.isEmpty() && !m_selectorCannotMatchAnything)
        return SelectorCompilationStatus::CannotCompile;

    bool reservedCalleeSavedRegisters = false;
    unsigned availableRegisterCount = m_registerAllocator.availableRegisterCount();
    unsigned minimumRegisterCountForAttributes = minimumRegisterRequirements(m_selectorFragments);
    if (availableRegisterCount < minimumRegisterCountForAttributes) {
        reservedCalleeSavedRegisters = true;
        m_registerAllocator.reserveCalleeSavedRegisters(m_stackAllocator, minimumRegisterCountForAttributes - availableRegisterCount);
    }

    m_registerAllocator.allocateRegister(elementAddressRegister);

    if (m_functionType == FunctionType::SelectorCheckerWithCheckingContext)
        m_checkingContextStackReference = m_stackAllocator.push(checkingContextRegister);

    Assembler::JumpList failureCases;

    for (unsigned i = 0; i < m_selectorFragments.size(); ++i) {
        const SelectorFragment& fragment = m_selectorFragments[i];
        switch (fragment.relationToRightFragment) {
        case FragmentRelation::Rightmost:
            generateElementMatching(failureCases, fragment);
            break;
        case FragmentRelation::Descendant:
            generateAncestorTreeWalker(failureCases, fragment);
            break;
        case FragmentRelation::Child:
            generateParentElementTreeWalker(failureCases, fragment);
            break;
        case FragmentRelation::DirectAdjacent:
            generateDirectAdjacentTreeWalker(failureCases, fragment);
            break;
        case FragmentRelation::IndirectAdjacent:
            generateIndirectAdjacentTreeWalker(failureCases, fragment);
            break;
        }
        generateBacktrackingTailsIfNeeded(fragment);
    }

    m_registerAllocator.deallocateRegister(elementAddressRegister);

    if (m_functionType == FunctionType::SimpleSelectorChecker && m_selectorCannotMatchAnything) {
        m_assembler.move(Assembler::TrustedImm32(0), returnRegister);
        m_assembler.ret();
    } else if (m_functionType == FunctionType::SimpleSelectorChecker) {
        // Success.
        m_assembler.move(Assembler::TrustedImm32(1), returnRegister);
        if (!reservedCalleeSavedRegisters)
            m_assembler.ret();

        // Failure.
        if (!failureCases.empty()) {
            Assembler::Jump skipFailureCase;
            if (reservedCalleeSavedRegisters)
                skipFailureCase = m_assembler.jump();

            failureCases.link(&m_assembler);
            m_assembler.move(Assembler::TrustedImm32(0), returnRegister);

            if (!reservedCalleeSavedRegisters)
                m_assembler.ret();
            else
                skipFailureCase.link(&m_assembler);
        }
        if (reservedCalleeSavedRegisters) {
            m_registerAllocator.restoreCalleeSavedRegisters(m_stackAllocator);
            m_assembler.ret();
        }
    } else {
        ASSERT(m_functionType == FunctionType::SelectorCheckerWithCheckingContext);
        ASSERT(!m_selectorCannotMatchAnything);

        // Success.
        m_assembler.move(Assembler::TrustedImm32(1), returnRegister);

        StackAllocator successStack = m_stackAllocator;
        StackAllocator failureStack = m_stackAllocator;

        LocalRegister checkingContextRegister(m_registerAllocator);
        successStack.pop(m_checkingContextStackReference, checkingContextRegister);

        // Failure.
        if (!failureCases.empty()) {
            Assembler::Jump skipFailureCase = m_assembler.jump();

            failureCases.link(&m_assembler);
            failureStack.discard();
            m_assembler.move(Assembler::TrustedImm32(0), returnRegister);

            skipFailureCase.link(&m_assembler);
        }

        m_stackAllocator.merge(std::move(successStack), std::move(failureStack));
        m_registerAllocator.restoreCalleeSavedRegisters(m_stackAllocator);
        m_assembler.ret();
    }

    JSC::LinkBuffer linkBuffer(*vm, &m_assembler, CSS_CODE_ID);
    for (unsigned i = 0; i < m_functionCalls.size(); i++)
        linkBuffer.link(m_functionCalls[i].first, m_functionCalls[i].second);

#if CSS_SELECTOR_JIT_DEBUGGING && ASSERT_DISABLED
    codeRef = linkBuffer.finalizeCodeWithDisassembly("CSS Selector JIT for \"%s\"", m_originalSelector->selectorText().utf8().data());
#else
    codeRef = FINALIZE_CODE(linkBuffer, ("CSS Selector JIT"));
#endif

    if (m_functionType == FunctionType::SimpleSelectorChecker)
        return SelectorCompilationStatus::SimpleSelectorChecker;
    return SelectorCompilationStatus::SelectorCheckerWithCheckingContext;
}


static inline void updateChainStates(const SelectorFragment& fragment, bool& hasDescendantRelationOnTheRight, unsigned& ancestorPositionSinceDescendantRelation, bool& hasIndirectAdjacentRelationOnTheRightOfDirectAdjacentChain, unsigned& adjacentPositionSinceIndirectAdjacentTreeWalk)
{
    switch (fragment.relationToRightFragment) {
    case FragmentRelation::Rightmost:
        break;
    case FragmentRelation::Descendant:
        hasDescendantRelationOnTheRight = true;
        ancestorPositionSinceDescendantRelation = 0;
        hasIndirectAdjacentRelationOnTheRightOfDirectAdjacentChain = false;
        break;
    case FragmentRelation::Child:
        ++ancestorPositionSinceDescendantRelation;
        hasIndirectAdjacentRelationOnTheRightOfDirectAdjacentChain = false;
        break;
    case FragmentRelation::DirectAdjacent:
        if (hasIndirectAdjacentRelationOnTheRightOfDirectAdjacentChain)
            ++adjacentPositionSinceIndirectAdjacentTreeWalk;
        break;
    case FragmentRelation::IndirectAdjacent:
        hasIndirectAdjacentRelationOnTheRightOfDirectAdjacentChain = true;
        break;
    }
}

static inline bool isFirstAncestor(unsigned ancestorPositionSinceDescendantRelation)
{
    return ancestorPositionSinceDescendantRelation == 1;
}

static inline bool isFirstAdjacent(unsigned ancestorPositionSinceDescendantRelation)
{
    return ancestorPositionSinceDescendantRelation == 1;
}

static inline bool isAfterChildRelation(unsigned ancestorPositionSinceDescendantRelation)
{
    return ancestorPositionSinceDescendantRelation > 0;
}

static inline void solveBacktrackingAction(SelectorFragment& fragment, bool hasDescendantRelationOnTheRight, unsigned ancestorPositionSinceDescendantRelation, bool hasIndirectAdjacentRelationOnTheRightOfDirectAdjacentChain, unsigned adjacentPositionSinceIndirectAdjacentTreeWalk)
{
    switch (fragment.relationToRightFragment) {
    case FragmentRelation::Rightmost:
    case FragmentRelation::Descendant:
        break;
    case FragmentRelation::Child:
        // Failure to match the element should resume matching at the nearest ancestor/descendant entry point.
        if (hasDescendantRelationOnTheRight) {
            if (isFirstAncestor(ancestorPositionSinceDescendantRelation))
                fragment.matchingBacktrackingAction = BacktrackingAction::JumpToDescendantEntryPoint;
            else
                fragment.matchingBacktrackingAction = BacktrackingAction::JumpToDescendantTail;
        }
        break;
    case FragmentRelation::DirectAdjacent:
        // Failure on traversal implies no other sibling traversal can match. Matching should resume at the
        // nearest ancestor/descendant traversal.
        if (hasDescendantRelationOnTheRight) {
            if (!isAfterChildRelation(ancestorPositionSinceDescendantRelation))
                fragment.traversalBacktrackingAction = BacktrackingAction::JumpToDescendantTreeWalkerEntryPoint;
            else {
                if (!hasIndirectAdjacentRelationOnTheRightOfDirectAdjacentChain || isFirstAdjacent(adjacentPositionSinceIndirectAdjacentTreeWalk))
                    fragment.traversalBacktrackingAction = BacktrackingAction::JumpToDescendantTail;
                else
                    fragment.traversalBacktrackingAction = BacktrackingAction::JumpToClearAdjacentDescendantTail;
            }
        }

        // If the rightmost relation is a indirect adjacent, matching sould resume from there.
        // Otherwise, we resume from the latest ancestor/descendant if any.
        if (hasIndirectAdjacentRelationOnTheRightOfDirectAdjacentChain) {
            if (isFirstAdjacent(adjacentPositionSinceIndirectAdjacentTreeWalk))
                fragment.matchingBacktrackingAction = BacktrackingAction::JumpToIndirectAdjacentEntryPoint;
            else
                fragment.matchingBacktrackingAction = BacktrackingAction::JumpToDirectAdjacentTail;
        } else if (hasDescendantRelationOnTheRight) {
            if (isAfterChildRelation(ancestorPositionSinceDescendantRelation))
                fragment.matchingBacktrackingAction = BacktrackingAction::JumpToDescendantTail;
            else if (hasDescendantRelationOnTheRight)
                fragment.matchingBacktrackingAction = BacktrackingAction::JumpToDescendantTreeWalkerEntryPoint;
        }
        break;
    case FragmentRelation::IndirectAdjacent:
        // Failure on traversal implies no other sibling matching will succeed. Matching can resume
        // from the latest ancestor/descendant.
        if (hasDescendantRelationOnTheRight) {
            if (isAfterChildRelation(ancestorPositionSinceDescendantRelation))
                fragment.traversalBacktrackingAction = BacktrackingAction::JumpToDescendantTail;
            else
                fragment.traversalBacktrackingAction = BacktrackingAction::JumpToDescendantTreeWalkerEntryPoint;
        }
        break;
    }
}

static bool requiresAdjacentTail(const SelectorFragment& fragment)
{
    ASSERT(fragment.traversalBacktrackingAction != BacktrackingAction::JumpToDirectAdjacentTail);
    return fragment.matchingBacktrackingAction == BacktrackingAction::JumpToDirectAdjacentTail;
}

static bool requiresDescendantTail(const SelectorFragment& fragment)
{
    return fragment.matchingBacktrackingAction == BacktrackingAction::JumpToDescendantTail || fragment.traversalBacktrackingAction == BacktrackingAction::JumpToDescendantTail;
}

void SelectorCodeGenerator::computeBacktrackingInformation()
{
    bool hasDescendantRelationOnTheRight = false;
    unsigned ancestorPositionSinceDescendantRelation = 0;
    bool hasIndirectAdjacentRelationOnTheRightOfDirectAdjacentChain = false;
    unsigned adjacentPositionSinceIndirectAdjacentTreeWalk = 0;

    bool needsAdjacentTail = false;
    bool needsDescendantTail = false;

    for (unsigned i = 0; i < m_selectorFragments.size(); ++i) {
        SelectorFragment& fragment = m_selectorFragments[i];

        updateChainStates(fragment, hasDescendantRelationOnTheRight, ancestorPositionSinceDescendantRelation, hasIndirectAdjacentRelationOnTheRightOfDirectAdjacentChain, adjacentPositionSinceIndirectAdjacentTreeWalk);

        solveBacktrackingAction(fragment, hasDescendantRelationOnTheRight, ancestorPositionSinceDescendantRelation, hasIndirectAdjacentRelationOnTheRightOfDirectAdjacentChain, adjacentPositionSinceIndirectAdjacentTreeWalk);

        needsAdjacentTail |= requiresAdjacentTail(fragment);
        needsDescendantTail |= requiresDescendantTail(fragment);

        // Add code generation flags.
        if (fragment.relationToLeftFragment != FragmentRelation::Descendant && fragment.relationToRightFragment == FragmentRelation::Descendant)
            fragment.backtrackingFlags |= BacktrackingFlag::DescendantEntryPoint;
        if (fragment.relationToLeftFragment == FragmentRelation::DirectAdjacent && fragment.relationToRightFragment == FragmentRelation::IndirectAdjacent)
            fragment.backtrackingFlags |= BacktrackingFlag::IndirectAdjacentEntryPoint;
        if (fragment.relationToLeftFragment != FragmentRelation::Descendant && fragment.relationToRightFragment == FragmentRelation::Child && isFirstAncestor(ancestorPositionSinceDescendantRelation))
            fragment.backtrackingFlags |= BacktrackingFlag::SaveDescendantBacktrackingStart;
        if (fragment.relationToLeftFragment == FragmentRelation::DirectAdjacent && fragment.relationToRightFragment == FragmentRelation::DirectAdjacent && isFirstAdjacent(adjacentPositionSinceIndirectAdjacentTreeWalk))
            fragment.backtrackingFlags |= BacktrackingFlag::SaveAdjacentBacktrackingStart;
        if (fragment.relationToLeftFragment != FragmentRelation::DirectAdjacent && needsAdjacentTail) {
            ASSERT(fragment.relationToRightFragment == FragmentRelation::DirectAdjacent);
            fragment.backtrackingFlags |= BacktrackingFlag::DirectAdjacentTail;
            needsAdjacentTail = false;
        }
        if (fragment.relationToLeftFragment == FragmentRelation::Descendant && needsDescendantTail) {
            fragment.backtrackingFlags |= BacktrackingFlag::DescendantTail;
            needsDescendantTail = false;
        }
    }
}

static inline Assembler::Jump testIsElementFlagOnNode(Assembler::ResultCondition condition, Assembler& assembler, Assembler::RegisterID nodeAddress)
{
    return assembler.branchTest32(condition, Assembler::Address(nodeAddress, Node::nodeFlagsMemoryOffset()), Assembler::TrustedImm32(Node::flagIsElement()));
}

void SelectorCodeGenerator::generateWalkToParentElement(Assembler::JumpList& failureCases, Assembler::RegisterID targetRegister)
{
    //    ContainerNode* parent = parentNode()
    //    if (!parent || !parent->isElementNode())
    //         failure
    m_assembler.loadPtr(Assembler::Address(elementAddressRegister, Node::parentNodeMemoryOffset()), targetRegister);
    failureCases.append(m_assembler.branchTestPtr(Assembler::Zero, targetRegister));
    failureCases.append(testIsElementFlagOnNode(Assembler::Zero, m_assembler, targetRegister));
}

void SelectorCodeGenerator::generateParentElementTreeWalker(Assembler::JumpList& failureCases, const SelectorFragment& fragment)
{
    Assembler::JumpList traversalFailureCases;
    generateWalkToParentElement(traversalFailureCases, elementAddressRegister);
    linkFailures(failureCases, fragment.traversalBacktrackingAction, traversalFailureCases);

    Assembler::JumpList matchingFailureCases;
    generateElementMatching(matchingFailureCases, fragment);
    linkFailures(failureCases, fragment.matchingBacktrackingAction, matchingFailureCases);

    if (fragment.backtrackingFlags & BacktrackingFlag::SaveDescendantBacktrackingStart) {
        m_descendantBacktrackingStart = m_registerAllocator.allocateRegister();
        m_assembler.move(elementAddressRegister, m_descendantBacktrackingStart);
    }
}

void SelectorCodeGenerator::generateAncestorTreeWalker(Assembler::JumpList& failureCases, const SelectorFragment& fragment)
{
    // Loop over the ancestors until one of them matches the fragment.
    Assembler::Label loopStart(m_assembler.label());

    if (fragment.backtrackingFlags & BacktrackingFlag::DescendantEntryPoint)
        m_descendantTreeWalkerBacktrackingPoint = m_assembler.label();

    generateWalkToParentElement(failureCases, elementAddressRegister);

    if (fragment.backtrackingFlags & BacktrackingFlag::DescendantEntryPoint)
        m_descendantEntryPoint = m_assembler.label();

    Assembler::JumpList tagMatchingLocalFailureCases;
    generateElementMatching(tagMatchingLocalFailureCases, fragment);
    tagMatchingLocalFailureCases.linkTo(loopStart, &m_assembler);
}

void SelectorCodeGenerator::generateWalkToPreviousAdjacent(Assembler::JumpList& failureCases, const SelectorFragment& fragment)
{
    //    do {
    //        previousSibling = previousSibling->previousSibling();
    //        if (!previousSibling)
    //            failure!
    //    while (!previousSibling->isElement());
    Assembler::RegisterID previousSibling;
    bool useTailOnTraversalFailure = fragment.traversalBacktrackingAction >= BacktrackingAction::JumpToDescendantTail;
    if (!useTailOnTraversalFailure) {
        // If the current fragment is not dependant on a previously saved elementAddressRegister, a fast recover
        // from a failure would resume with elementAddressRegister.
        // When walking to the previous sibling, the failure can be that previousSibling is null. We cannot backtrack
        // with a null elementAddressRegister so we do the traversal on a copy.
        previousSibling = m_registerAllocator.allocateRegister();
        m_assembler.move(elementAddressRegister, previousSibling);
    } else
        previousSibling = elementAddressRegister;

    Assembler::Label loopStart = m_assembler.label();
    m_assembler.loadPtr(Assembler::Address(previousSibling, Node::previousSiblingMemoryOffset()), previousSibling);
    failureCases.append(m_assembler.branchTestPtr(Assembler::Zero, previousSibling));
    testIsElementFlagOnNode(Assembler::Zero, m_assembler, previousSibling).linkTo(loopStart, &m_assembler);

    // On success, move previousSibling over to elementAddressRegister if we could not work on elementAddressRegister directly.
    if (!useTailOnTraversalFailure) {
        m_assembler.move(previousSibling, elementAddressRegister);
        m_registerAllocator.deallocateRegister(previousSibling);
    }
}

void SelectorCodeGenerator::generateDirectAdjacentTreeWalker(Assembler::JumpList& failureCases, const SelectorFragment& fragment)
{
    markParentElementIfResolvingStyle(Element::setChildrenAffectedByDirectAdjacentRules);

    Assembler::JumpList traversalFailureCases;
    generateWalkToPreviousAdjacent(traversalFailureCases, fragment);
    linkFailures(failureCases, fragment.traversalBacktrackingAction, traversalFailureCases);

    Assembler::JumpList matchingFailureCases;
    generateElementMatching(matchingFailureCases, fragment);
    linkFailures(failureCases, fragment.matchingBacktrackingAction, matchingFailureCases);

    if (fragment.backtrackingFlags & BacktrackingFlag::SaveAdjacentBacktrackingStart)
        m_adjacentBacktrackingStart = m_stackAllocator.push(elementAddressRegister);
}

void SelectorCodeGenerator::generateIndirectAdjacentTreeWalker(Assembler::JumpList& failureCases, const SelectorFragment& fragment)
{
    markParentElementIfResolvingStyle(Element::setChildrenAffectedByForwardPositionalRules);

    Assembler::Label loopStart(m_assembler.label());

    Assembler::JumpList traversalFailureCases;
    generateWalkToPreviousAdjacent(traversalFailureCases, fragment);
    linkFailures(failureCases, fragment.traversalBacktrackingAction, traversalFailureCases);

    if (fragment.backtrackingFlags & BacktrackingFlag::IndirectAdjacentEntryPoint)
        m_indirectAdjacentEntryPoint = m_assembler.label();

    Assembler::JumpList localFailureCases;
    generateElementMatching(localFailureCases, fragment);
    localFailureCases.linkTo(loopStart, &m_assembler);
}

void SelectorCodeGenerator::markParentElementIfResolvingStyle(JSC::FunctionPtr markingFunction)
{
    //     if (checkingContext.resolvingMode == ResolvingStyle) {
    //         Element* parent = element->parentNode();
    //         markingFunction(parent);
    //     }
    Assembler::JumpList failedToGetParent;
    Assembler::Jump notResolvingStyle;
    {
        // Get the checking context.
        unsigned offsetToCheckingContext = m_stackAllocator.offsetToStackReference(m_checkingContextStackReference);
        LocalRegister checkingContext(m_registerAllocator);
        m_assembler.loadPtr(Assembler::Address(Assembler::stackPointerRegister, offsetToCheckingContext), checkingContext);

        // If we not resolving style, skip the whole marking.
        notResolvingStyle = m_assembler.branch8(Assembler::NotEqual, Assembler::Address(checkingContext, OBJECT_OFFSETOF(CheckingContext, resolvingMode)), Assembler::TrustedImm32(SelectorChecker::ResolvingStyle));
    }

    // Get the parent element in a temporary register.
    Assembler::RegisterID parentElement = m_registerAllocator.allocateRegister();
    generateWalkToParentElement(failedToGetParent, parentElement);

    // Return the register parentElement just before the function call since we don't need it to be preserved
    // on the stack.
    m_registerAllocator.deallocateRegister(parentElement);

    // Invoke the marking function on the parent element.
    FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
    functionCall.setFunctionAddress(markingFunction);
    functionCall.setFirstArgument(parentElement);
    functionCall.call();

    notResolvingStyle.link(&m_assembler);
    failedToGetParent.link(&m_assembler);
}


void SelectorCodeGenerator::linkFailures(Assembler::JumpList& globalFailureCases, BacktrackingAction backtrackingAction, Assembler::JumpList& localFailureCases)
{
    switch (backtrackingAction) {
    case BacktrackingAction::NoBacktracking:
        globalFailureCases.append(localFailureCases);
        break;
    case BacktrackingAction::JumpToDescendantEntryPoint:
        localFailureCases.linkTo(m_descendantEntryPoint, &m_assembler);
        break;
    case BacktrackingAction::JumpToDescendantTreeWalkerEntryPoint:
        localFailureCases.linkTo(m_descendantTreeWalkerBacktrackingPoint, &m_assembler);
        break;
    case BacktrackingAction::JumpToDescendantTail:
        m_descendantBacktrackingFailureCases.append(localFailureCases);
        break;
    case BacktrackingAction::JumpToIndirectAdjacentEntryPoint:
        localFailureCases.linkTo(m_indirectAdjacentEntryPoint, &m_assembler);
        break;
    case BacktrackingAction::JumpToDirectAdjacentTail:
        m_adjacentBacktrackingFailureCases.append(localFailureCases);
        break;
    case BacktrackingAction::JumpToClearAdjacentDescendantTail:
        m_clearAdjacentEntryPointDescendantFailureCases.append(localFailureCases);
        break;
    }
}

void SelectorCodeGenerator::generateAdjacentBacktrackingTail(StackAllocator& adjacentTailStack)
{
    m_adjacentBacktrackingFailureCases.link(&m_assembler);
    m_adjacentBacktrackingFailureCases.clear();
    adjacentTailStack.pop(m_adjacentBacktrackingStart, elementAddressRegister);
    m_assembler.jump(m_indirectAdjacentEntryPoint);
}

void SelectorCodeGenerator::generateDescendantBacktrackingTail()
{
    m_descendantBacktrackingFailureCases.link(&m_assembler);
    m_descendantBacktrackingFailureCases.clear();
    m_assembler.move(m_descendantBacktrackingStart, elementAddressRegister);
    m_registerAllocator.deallocateRegister(m_descendantBacktrackingStart);
    m_assembler.jump(m_descendantEntryPoint);
}

void SelectorCodeGenerator::generateBacktrackingTailsIfNeeded(const SelectorFragment& fragment)
{
    if (fragment.backtrackingFlags & BacktrackingFlag::DirectAdjacentTail && fragment.backtrackingFlags & BacktrackingFlag::DescendantTail) {
        StackAllocator successStack = m_stackAllocator;
        StackAllocator adjacentTailStack = m_stackAllocator;
        StackAllocator descendantTailStack = m_stackAllocator;

        successStack.popAndDiscard(m_adjacentBacktrackingStart);

        Assembler::Jump normalCase = m_assembler.jump();

        generateAdjacentBacktrackingTail(adjacentTailStack);

        m_clearAdjacentEntryPointDescendantFailureCases.link(&m_assembler);
        m_clearAdjacentEntryPointDescendantFailureCases.clear();
        descendantTailStack.popAndDiscard(m_adjacentBacktrackingStart);

        generateDescendantBacktrackingTail();

        normalCase.link(&m_assembler);

        m_stackAllocator.merge(std::move(successStack), std::move(adjacentTailStack), std::move(descendantTailStack));
    } else if (fragment.backtrackingFlags & BacktrackingFlag::DirectAdjacentTail) {
        StackAllocator successStack = m_stackAllocator;
        StackAllocator adjacentTailStack = m_stackAllocator;

        successStack.popAndDiscard(m_adjacentBacktrackingStart);

        Assembler::Jump normalCase = m_assembler.jump();
        generateAdjacentBacktrackingTail(adjacentTailStack);
        normalCase.link(&m_assembler);

        m_stackAllocator.merge(std::move(successStack), std::move(adjacentTailStack));
    } else if (fragment.backtrackingFlags & BacktrackingFlag::DescendantTail) {
        Assembler::Jump normalCase = m_assembler.jump();
        generateDescendantBacktrackingTail();
        normalCase.link(&m_assembler);
    }
}

void SelectorCodeGenerator::generateElementMatching(Assembler::JumpList& failureCases, const SelectorFragment& fragment)
{
    if (fragment.pseudoClasses.contains(CSSSelector::PseudoLink))
        generateElementIsLink(failureCases);

    if (fragment.tagName)
        generateElementHasTagName(failureCases, *(fragment.tagName));

    if (fragment.pseudoClasses.contains(CSSSelector::PseudoFocus))
        generateElementIsFocused(failureCases);

    generateElementDataMatching(failureCases, fragment);
}

void SelectorCodeGenerator::generateElementDataMatching(Assembler::JumpList& failureCases, const SelectorFragment& fragment)
{
    if (!fragment.id && fragment.classNames.isEmpty() && fragment.attributes.isEmpty())
        return;

    //  Generate:
    //     elementDataAddress = element->elementData();
    //     if (!elementDataAddress)
    //         failure!
    LocalRegister elementDataAddress(m_registerAllocator);
    m_assembler.loadPtr(Assembler::Address(elementAddressRegister, Element::elementDataMemoryOffset()), elementDataAddress);
    failureCases.append(m_assembler.branchTestPtr(Assembler::Zero, elementDataAddress));

    if (fragment.id)
        generateElementHasId(failureCases, elementDataAddress, *fragment.id);
    if (!fragment.classNames.isEmpty())
        generateElementHasClasses(failureCases, elementDataAddress, fragment.classNames);
    if (!fragment.attributes.isEmpty())
    generateElementAttributesMatching(failureCases, elementDataAddress, fragment);
}

static inline Assembler::Jump testIsHTMLFlagOnNode(Assembler::ResultCondition condition, Assembler& assembler, Assembler::RegisterID nodeAddress)
{
    return assembler.branchTest32(condition, Assembler::Address(nodeAddress, Node::nodeFlagsMemoryOffset()), Assembler::TrustedImm32(Node::flagIsHTML()));
}

static inline bool canMatchStyleAttribute(const SelectorFragment& fragment)
{
    for (unsigned i = 0; i < fragment.attributes.size(); ++i) {
        const CSSSelector* attributeSelector = fragment.attributes[i];
        const QualifiedName& attributeName = attributeSelector->attribute();
        if (Attribute::nameMatchesFilter(HTMLNames::styleAttr, attributeName.prefix(), attributeName.localName(), attributeName.namespaceURI())
            || Attribute::nameMatchesFilter(HTMLNames::styleAttr, attributeName.prefix(), attributeSelector->attributeCanonicalLocalName(), attributeName.namespaceURI())) {
            return true;
        }
    }
    return false;
}

void SelectorCodeGenerator::generateSynchronizeStyleAttribute(Assembler::RegisterID elementDataArraySizeAndFlags)
{
    // The style attribute is updated lazily based on the flag styleAttributeIsDirty.
    Assembler::Jump styleAttributeNotDirty = m_assembler.branchTest32(Assembler::Zero, elementDataArraySizeAndFlags, Assembler::TrustedImm32(ElementData::styleAttributeIsDirtyFlag()));

    FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
    functionCall.setFunctionAddress(StyledElement::synchronizeStyleAttributeInternal);
    Assembler::RegisterID elementAddress = elementAddressRegister;
    functionCall.setFirstArgument(elementAddress);
    functionCall.call();

    styleAttributeNotDirty.link(&m_assembler);
}

void SelectorCodeGenerator::generateSynchronizeAllAnimatedSVGAttribute(Assembler::RegisterID elementDataArraySizeAndFlags)
{
    // SVG attributes can be updated lazily depending on the flag AnimatedSVGAttributesAreDirty. We need to check
    // that first.
    Assembler::Jump animatedSVGAttributesNotDirty = m_assembler.branchTest32(Assembler::Zero, elementDataArraySizeAndFlags, Assembler::TrustedImm32(ElementData::animatedSVGAttributesAreDirtyFlag()));

    FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
    functionCall.setFunctionAddress(SVGElement::synchronizeAllAnimatedSVGAttribute);
    Assembler::RegisterID elementAddress = elementAddressRegister;
    functionCall.setFirstArgument(elementAddress);
    functionCall.call();

    animatedSVGAttributesNotDirty.link(&m_assembler);
}

void SelectorCodeGenerator::generateElementAttributesMatching(Assembler::JumpList& failureCases, const LocalRegister& elementDataAddress, const SelectorFragment& fragment)
{
    LocalRegister scratchRegister(m_registerAllocator);
    Assembler::RegisterID elementDataArraySizeAndFlags = scratchRegister;
    Assembler::RegisterID attributeArrayLength = scratchRegister;

    m_assembler.load32(Assembler::Address(elementDataAddress, ElementData::arraySizeAndFlagsMemoryOffset()), elementDataArraySizeAndFlags);

    if (canMatchStyleAttribute(fragment))
        generateSynchronizeStyleAttribute(elementDataArraySizeAndFlags);

    // FIXME: Systematically generating the function call for animatable SVG attributes causes a runtime penaltly. We should instead
    // filter from the list of SVGElement::isAnimatableAttribute at runtime when compiling.
    generateSynchronizeAllAnimatedSVGAttribute(elementDataArraySizeAndFlags);

    // Attributes can be stored either in a separate vector for UniqueElementData, or after the elementData itself
    // for ShareableElementData.
    LocalRegister attributeArrayPointer(m_registerAllocator);
    Assembler::Jump isShareableElementData  = m_assembler.branchTest32(Assembler::Zero, elementDataArraySizeAndFlags, Assembler::TrustedImm32(ElementData::isUniqueFlag()));
    {
        ptrdiff_t attributeVectorOffset = UniqueElementData::attributeVectorMemoryOffset();
        m_assembler.loadPtr(Assembler::Address(elementDataAddress, attributeVectorOffset + UniqueElementData::AttributeVector::dataMemoryOffset()), attributeArrayPointer);
        m_assembler.load32(Assembler::Address(elementDataAddress, attributeVectorOffset + UniqueElementData::AttributeVector::sizeMemoryOffset()), attributeArrayLength);
    }
    Assembler::Jump skipShareable = m_assembler.jump();

    {
        isShareableElementData.link(&m_assembler);
        m_assembler.urshift32(elementDataArraySizeAndFlags, Assembler::TrustedImm32(ElementData::arraySizeOffset()), attributeArrayLength);
        m_assembler.add64(Assembler::TrustedImm32(ShareableElementData::attributeArrayMemoryOffset()), elementDataAddress, attributeArrayPointer);
    }

    skipShareable.link(&m_assembler);

    // If there are no attributes, fail immediately.
    failureCases.append(m_assembler.branchTest32(Assembler::Zero, attributeArrayLength));

    unsigned attributeCount = fragment.attributes.size();
    for (unsigned i = 0; i < attributeCount; ++i) {
        Assembler::RegisterID decIndexRegister;
        Assembler::RegisterID currentAttributeAddress;

        bool isLastAttribute = i == (attributeCount - 1);
        if (!isLastAttribute) {
            // We need to make a copy to let the next iterations use the values.
            currentAttributeAddress = m_registerAllocator.allocateRegister();
            decIndexRegister = m_registerAllocator.allocateRegister();
            m_assembler.move(attributeArrayPointer, currentAttributeAddress);
            m_assembler.move(attributeArrayLength, decIndexRegister);
        } else {
            currentAttributeAddress = attributeArrayPointer;
            decIndexRegister = attributeArrayLength;
        }

        generateElementAttributeMatching(failureCases, currentAttributeAddress, decIndexRegister, fragment.attributes[i]);

        if (!isLastAttribute) {
            m_registerAllocator.deallocateRegister(decIndexRegister);
            m_registerAllocator.deallocateRegister(currentAttributeAddress);
        }
    }
}

void SelectorCodeGenerator::generateElementAttributeMatching(Assembler::JumpList& failureCases, Assembler::RegisterID currentAttributeAddress, Assembler::RegisterID decIndexRegister, const CSSSelector* attributeSelector)
{
    // Get the localName used for comparison. HTML elements use a lowercase local name known in selectors as canonicalLocalName.
    LocalRegister localNameToMatch(m_registerAllocator);

    // In general, canonicalLocalName and localName are the same. When they differ, we have to check if the node is HTML to know
    // which one to use.
    const AtomicStringImpl* canonicalLocalName = attributeSelector->attributeCanonicalLocalName().impl();
    const AtomicStringImpl* localName = attributeSelector->attribute().localName().impl();
    if (canonicalLocalName == localName)
        m_assembler.move(Assembler::TrustedImmPtr(canonicalLocalName), localNameToMatch);
    else {
        m_assembler.move(Assembler::TrustedImmPtr(canonicalLocalName), localNameToMatch);
        Assembler::Jump elementIsHTML = testIsHTMLFlagOnNode(Assembler::NonZero, m_assembler, elementAddressRegister);
        m_assembler.move(Assembler::TrustedImmPtr(localName), localNameToMatch);
        elementIsHTML.link(&m_assembler);
    }

    Assembler::JumpList successCases;
    Assembler::Label loopStart(m_assembler.label());

    LocalRegister qualifiedNameImpl(m_registerAllocator);
    m_assembler.loadPtr(Assembler::Address(currentAttributeAddress, Attribute::nameMemoryOffset()), qualifiedNameImpl);

    bool shouldCheckNamespace = attributeSelector->attribute().prefix() != starAtom;
    if (shouldCheckNamespace) {
        Assembler::Jump nameDoesNotMatch = m_assembler.branchPtr(Assembler::NotEqual, Assembler::Address(qualifiedNameImpl, QualifiedName::QualifiedNameImpl::localNameMemoryOffset()), localNameToMatch);

        const AtomicStringImpl* namespaceURI = attributeSelector->attribute().namespaceURI().impl();
        if (namespaceURI) {
            LocalRegister namespaceToMatch(m_registerAllocator);
            m_assembler.move(Assembler::TrustedImmPtr(namespaceURI), namespaceToMatch);
            successCases.append(m_assembler.branchPtr(Assembler::Equal, Assembler::Address(qualifiedNameImpl, QualifiedName::QualifiedNameImpl::namespaceMemoryOffset()), namespaceToMatch));
        } else
            successCases.append(m_assembler.branchTestPtr(Assembler::Zero, Assembler::Address(qualifiedNameImpl, QualifiedName::QualifiedNameImpl::namespaceMemoryOffset())));
        nameDoesNotMatch.link(&m_assembler);
    } else
        successCases.append(m_assembler.branchPtr(Assembler::Equal, Assembler::Address(qualifiedNameImpl, QualifiedName::QualifiedNameImpl::localNameMemoryOffset()), localNameToMatch));

    // If we reached the last element -> failure.
    failureCases.append(m_assembler.branchSub32(Assembler::Zero, Assembler::TrustedImm32(1), decIndexRegister));

    // Otherwise just loop over.
    m_assembler.addPtr(Assembler::TrustedImm32(sizeof(Attribute)), currentAttributeAddress);
    m_assembler.jump().linkTo(loopStart, &m_assembler);
    successCases.link(&m_assembler);
}

inline void SelectorCodeGenerator::generateElementHasTagName(Assembler::JumpList& failureCases, const QualifiedName& nameToMatch)
{
    if (nameToMatch == anyQName())
        return;

    // Load the QualifiedNameImpl from the input.
    LocalRegister qualifiedNameImpl(m_registerAllocator);
    m_assembler.loadPtr(Assembler::Address(elementAddressRegister, Element::tagQNameMemoryOffset() + QualifiedName::implMemoryOffset()), qualifiedNameImpl);

    const AtomicString& selectorLocalName = nameToMatch.localName();
    if (selectorLocalName != starAtom) {
        // Generate localName == element->localName().
        LocalRegister constantRegister(m_registerAllocator);
        m_assembler.move(Assembler::TrustedImmPtr(selectorLocalName.impl()), constantRegister);
        failureCases.append(m_assembler.branchPtr(Assembler::NotEqual, Assembler::Address(qualifiedNameImpl, QualifiedName::QualifiedNameImpl::localNameMemoryOffset()), constantRegister));
    }

    const AtomicString& selectorNamespaceURI = nameToMatch.namespaceURI();
    if (selectorNamespaceURI != starAtom) {
        // Generate namespaceURI == element->namespaceURI().
        LocalRegister constantRegister(m_registerAllocator);
        m_assembler.move(Assembler::TrustedImmPtr(selectorNamespaceURI.impl()), constantRegister);
        failureCases.append(m_assembler.branchPtr(Assembler::NotEqual, Assembler::Address(qualifiedNameImpl, QualifiedName::QualifiedNameImpl::namespaceMemoryOffset()), constantRegister));
    }
}

void SelectorCodeGenerator::generateElementHasId(Assembler::JumpList& failureCases, const LocalRegister& elementDataAddress, const AtomicString& idToMatch)
{
    // Compare the pointers of the AtomicStringImpl from idForStyleResolution with the reference idToMatch.
    LocalRegister idToMatchRegister(m_registerAllocator);
    m_assembler.move(Assembler::TrustedImmPtr(idToMatch.impl()), idToMatchRegister);
    failureCases.append(m_assembler.branchPtr(Assembler::NotEqual, Assembler::Address(elementDataAddress, ElementData::idForStyleResolutionMemoryOffset()), idToMatchRegister));
}

void SelectorCodeGenerator::generateElementHasClasses(Assembler::JumpList& failureCases, const LocalRegister& elementDataAddress, const Vector<const AtomicStringImpl*>& classNames)
{
    // Load m_classNames.
    LocalRegister spaceSplitStringData(m_registerAllocator);
    m_assembler.loadPtr(Assembler::Address(elementDataAddress, ElementData::classNamesMemoryOffset()), spaceSplitStringData);

    // If SpaceSplitString does not have a SpaceSplitStringData pointer, it is empty -> failure case.
    failureCases.append(m_assembler.branchTestPtr(Assembler::Zero, spaceSplitStringData));

    // We loop over the classes of SpaceSplitStringData for each class name we need to match.
    LocalRegister indexRegister(m_registerAllocator);
    for (unsigned i = 0; i < classNames.size(); ++i) {
        LocalRegister classNameToMatch(m_registerAllocator);
        m_assembler.move(Assembler::TrustedImmPtr(classNames[i]), classNameToMatch);
        m_assembler.move(Assembler::TrustedImm32(0), indexRegister);

        // Beginning of a loop over all the class name of element to find the one we are looking for.
        Assembler::Label loopStart(m_assembler.label());

        // If the pointers match, proceed to the next matcher.
        Assembler::Jump classFound = m_assembler.branchPtr(Assembler::Equal, Assembler::BaseIndex(spaceSplitStringData, indexRegister, Assembler::timesPtr(), SpaceSplitStringData::tokensMemoryOffset()), classNameToMatch);

        // Increment the index.
        m_assembler.add32(Assembler::TrustedImm32(1), indexRegister);

        // If we reached the last element -> failure.
        failureCases.append(m_assembler.branch32(Assembler::Equal, Assembler::Address(spaceSplitStringData, SpaceSplitStringData::sizeMemoryOffset()), indexRegister));
        // Otherwise just loop over.
        m_assembler.jump().linkTo(loopStart, &m_assembler);

        // Success case.
        classFound.link(&m_assembler);
    }
}

void SelectorCodeGenerator::generateElementIsFocused(Assembler::JumpList& failureCases)
{
    Assembler::RegisterID elementAddress = elementAddressRegister;
    FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
    functionCall.setFunctionAddress(SelectorChecker::matchesFocusPseudoClass);
    functionCall.setFirstArgument(elementAddress);
    failureCases.append(functionCall.callAndBranchOnCondition(Assembler::Zero));
}

void SelectorCodeGenerator::generateElementIsLink(Assembler::JumpList& failureCases)
{
    failureCases.append(m_assembler.branchTest32(Assembler::Zero, Assembler::Address(elementAddressRegister, Node::nodeFlagsMemoryOffset()), Assembler::TrustedImm32(Node::flagIsLink())));
}

}; // namespace SelectorCompiler.
}; // namespace WebCore.

#endif // ENABLE(CSS_SELECTOR_JIT)
