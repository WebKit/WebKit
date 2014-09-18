/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Yusuke Suzuki <utatane.tea@gmail.com>
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
#include "CSSSelectorList.h"
#include "Element.h"
#include "ElementData.h"
#include "ElementRareData.h"
#include "FunctionCall.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "InspectorInstrumentation.h"
#include "NodeRenderStyle.h"
#include "QualifiedName.h"
#include "RegisterAllocator.h"
#include "RenderElement.h"
#include "RenderStyle.h"
#include "SVGElement.h"
#include "SelectorCheckerTestFunctions.h"
#include "StackAllocator.h"
#include "StyledElement.h"
#include <JavaScriptCore/GPRInfo.h>
#include <JavaScriptCore/LinkBuffer.h>
#include <JavaScriptCore/MacroAssembler.h>
#include <JavaScriptCore/VM.h>
#include <limits>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>

namespace WebCore {
namespace SelectorCompiler {

#define CSS_SELECTOR_JIT_DEBUGGING 0

enum class BacktrackingAction {
    NoBacktracking,
    JumpToDescendantEntryPoint,
    JumpToIndirectAdjacentEntryPoint,
    JumpToDescendantTreeWalkerEntryPoint,
    JumpToIndirectAdjacentTreeWalkerEntryPoint,
    JumpToDescendantTail,
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
        InChainWithDescendantTail = 1 << 6
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
    CannotMatchAnything,
    CannotCompile
};

enum class FragmentPositionInRootFragments {
    Rightmost,
    NotRightmost
};

class AttributeMatchingInfo {
public:
    AttributeMatchingInfo(const CSSSelector* selector, bool canDefaultToCaseSensitiveValueMatch)
        : m_selector(selector)
        , m_canDefaultToCaseSensitiveValueMatch(canDefaultToCaseSensitiveValueMatch)
    {
    }

    bool canDefaultToCaseSensitiveValueMatch() const { return m_canDefaultToCaseSensitiveValueMatch; }
    const CSSSelector& selector() const { return *m_selector; }

private:
    const CSSSelector* m_selector;
    bool m_canDefaultToCaseSensitiveValueMatch;
};

static const unsigned invalidHeight = std::numeric_limits<unsigned>::max();
static const unsigned invalidWidth = std::numeric_limits<unsigned>::max();

struct SelectorFragment {
    SelectorFragment()
        : traversalBacktrackingAction(BacktrackingAction::NoBacktracking)
        , matchingTagNameBacktrackingAction(BacktrackingAction::NoBacktracking)
        , matchingPostTagNameBacktrackingAction(BacktrackingAction::NoBacktracking)
        , backtrackingFlags(0)
        , tagNameMatchedBacktrackingStartHeightFromDescendant(invalidHeight)
        , tagNameNotMatchedBacktrackingStartHeightFromDescendant(invalidHeight)
        , heightFromDescendant(0)
        , tagNameMatchedBacktrackingStartWidthFromIndirectAdjacent(invalidWidth)
        , tagNameNotMatchedBacktrackingStartWidthFromIndirectAdjacent(invalidWidth)
        , widthFromIndirectAdjacent(0)
        , tagName(nullptr)
        , id(nullptr)
        , langFilter(nullptr)
        , onlyMatchesLinksInQuirksMode(true)
    {
    }
    FragmentRelation relationToLeftFragment;
    FragmentRelation relationToRightFragment;
    FragmentPositionInRootFragments positionInRootFragments;

    BacktrackingAction traversalBacktrackingAction;
    BacktrackingAction matchingTagNameBacktrackingAction;
    BacktrackingAction matchingPostTagNameBacktrackingAction;
    unsigned char backtrackingFlags;
    unsigned tagNameMatchedBacktrackingStartHeightFromDescendant;
    unsigned tagNameNotMatchedBacktrackingStartHeightFromDescendant;
    unsigned heightFromDescendant;
    unsigned tagNameMatchedBacktrackingStartWidthFromIndirectAdjacent;
    unsigned tagNameNotMatchedBacktrackingStartWidthFromIndirectAdjacent;
    unsigned widthFromIndirectAdjacent;

    const QualifiedName* tagName;
    const AtomicString* id;
    const AtomicString* langFilter;
    Vector<const AtomicStringImpl*, 32> classNames;
    HashSet<unsigned> pseudoClasses;
    Vector<JSC::FunctionPtr, 32> unoptimizedPseudoClasses;
    Vector<AttributeMatchingInfo, 32> attributes;
    Vector<std::pair<int, int>, 32> nthChildFilters;
    Vector<SelectorFragment> notFilters;
    Vector<Vector<SelectorFragment>> anyFilters;

    // For quirks mode, follow this: http://quirks.spec.whatwg.org/#the-:active-and-:hover-quirk
    // In quirks mode, a compound selector 'selector' that matches the following conditions must not match elements that would not also match the ':any-link' selector.
    //
    //    selector uses the ':active' or ':hover' pseudo-classes.
    //    selector does not use a type selector.
    //    selector does not use an attribute selector.
    //    selector does not use an ID selector.
    //    selector does not use a class selector.
    //    selector does not use a pseudo-class selector other than ':active' and ':hover'.
    //    selector does not use a pseudo-element selector.
    //    selector is not part of an argument to a functional pseudo-class or pseudo-element.
    bool onlyMatchesLinksInQuirksMode;
};

struct TagNamePattern {
    TagNamePattern()
        : tagName(nullptr)
        , inverted(false)
    {
    }
    const QualifiedName* tagName;
    bool inverted;
};

typedef JSC::MacroAssembler Assembler;
typedef Vector<SelectorFragment, 32> SelectorFragmentList;
typedef Vector<TagNamePattern, 32> TagNameList;

class SelectorCodeGenerator {
public:
    SelectorCodeGenerator(const CSSSelector*, SelectorContext);
    SelectorCompilationStatus compile(JSC::VM*, JSC::MacroAssemblerCodeRef&);

private:
    static const Assembler::RegisterID returnRegister;
    static const Assembler::RegisterID elementAddressRegister;
    static const Assembler::RegisterID checkingContextRegister;
    static const Assembler::RegisterID callFrameRegister;

    void generateSelectorChecker();

    // Element relations tree walker.
    void generateWalkToParentNode(Assembler::RegisterID targetRegister);
    void generateWalkToParentElement(Assembler::JumpList& failureCases, Assembler::RegisterID targetRegister);
    void generateParentElementTreeWalker(Assembler::JumpList& failureCases, const SelectorFragment&);
    void generateAncestorTreeWalker(Assembler::JumpList& failureCases, const SelectorFragment&);

    void generateWalkToNextAdjacentElement(Assembler::JumpList& failureCases, Assembler::RegisterID);
    void generateWalkToPreviousAdjacentElement(Assembler::JumpList& failureCases, Assembler::RegisterID);
    void generateWalkToPreviousAdjacent(Assembler::JumpList& failureCases, const SelectorFragment&);
    void generateDirectAdjacentTreeWalker(Assembler::JumpList& failureCases, const SelectorFragment&);
    void generateIndirectAdjacentTreeWalker(Assembler::JumpList& failureCases, const SelectorFragment&);
    void markParentElementIfResolvingStyle(int32_t);
    void markParentElementIfResolvingStyle(JSC::FunctionPtr);

    void linkFailures(Assembler::JumpList& globalFailureCases, BacktrackingAction, Assembler::JumpList& localFailureCases);
    void generateAdjacentBacktrackingTail();
    void generateDescendantBacktrackingTail();
    void generateBacktrackingTailsIfNeeded(Assembler::JumpList& failureCases, const SelectorFragment&);

    // Element properties matchers.
    void generateElementMatching(Assembler::JumpList& matchingTagNameFailureCases, Assembler::JumpList& matchingPostTagNameFailureCases, const SelectorFragment&);
    void generateElementDataMatching(Assembler::JumpList& failureCases, const SelectorFragment&);
    void generateElementFunctionCallTest(Assembler::JumpList& failureCases, JSC::FunctionPtr);
    void generateElementIsActive(Assembler::JumpList& failureCases, const SelectorFragment&);
    void generateElementIsFirstChild(Assembler::JumpList& failureCases, const SelectorFragment&);
    void generateElementIsHovered(Assembler::JumpList& failureCases, const SelectorFragment&);
    void generateElementIsInLanguage(Assembler::JumpList& failureCases, const AtomicString&);
    void generateElementIsLastChild(Assembler::JumpList& failureCases, const SelectorFragment&);
    void generateElementIsOnlyChild(Assembler::JumpList& failureCases, const SelectorFragment&);
    void generateSynchronizeStyleAttribute(Assembler::RegisterID elementDataArraySizeAndFlags);
    void generateSynchronizeAllAnimatedSVGAttribute(Assembler::RegisterID elementDataArraySizeAndFlags);
    void generateElementAttributesMatching(Assembler::JumpList& failureCases, const LocalRegister& elementDataAddress, const SelectorFragment&);
    void generateElementAttributeMatching(Assembler::JumpList& failureCases, Assembler::RegisterID currentAttributeAddress, Assembler::RegisterID decIndexRegister, const AttributeMatchingInfo& attributeInfo);
    void generateElementAttributeValueMatching(Assembler::JumpList& failureCases, Assembler::RegisterID currentAttributeAddress, const AttributeMatchingInfo& attributeInfo);
    void generateElementAttributeValueExactMatching(Assembler::JumpList& failureCases, Assembler::RegisterID currentAttributeAddress, const AtomicString& expectedValue, bool caseSensitive);
    void generateElementAttributeFunctionCallValueMatching(Assembler::JumpList& failureCases, Assembler::RegisterID currentAttributeAddress, const AtomicString& expectedValue, bool caseSensitive, JSC::FunctionPtr caseSensitiveTest, JSC::FunctionPtr caseInsensitiveTest);
    void generateElementHasTagName(Assembler::JumpList& failureCases, const QualifiedName& nameToMatch);
    void generateElementHasId(Assembler::JumpList& failureCases, const LocalRegister& elementDataAddress, const AtomicString& idToMatch);
    void generateElementHasClasses(Assembler::JumpList& failureCases, const LocalRegister& elementDataAddress, const Vector<const AtomicStringImpl*>& classNames);
    void generateElementIsLink(Assembler::JumpList& failureCases);
    void generateElementIsNthChild(Assembler::JumpList& failureCases, const SelectorFragment&);
    void generateElementMatchesNotPseudoClass(Assembler::JumpList& failureCases, const SelectorFragment&);
    void generateElementMatchesAnyPseudoClass(Assembler::JumpList& failureCases, const SelectorFragment&);
    void generateElementIsRoot(Assembler::JumpList& failureCases);
    void generateElementIsTarget(Assembler::JumpList& failureCases);

    // Helpers.
    void addFlagsToElementStyleFromContext(Assembler::RegisterID checkingContext, int64_t);
    Assembler::JumpList jumpIfNoPreviousAdjacentElement();
    Assembler::JumpList jumpIfNoNextAdjacentElement();
    Assembler::Jump jumpIfNotResolvingStyle(Assembler::RegisterID checkingContextRegister);
    void generateSpecialFailureInQuirksModeForActiveAndHoverIfNeeded(Assembler::JumpList& failureCases, const SelectorFragment&);
    Assembler::Jump modulo(JSC::MacroAssembler::ResultCondition, Assembler::RegisterID inputDividend, int divisor);
    void moduloIsZero(Assembler::JumpList& failureCases, Assembler::RegisterID inputDividend, int divisor);

    bool generatePrologue();
    void generateEpilogue();
    StackAllocator::StackReferenceVector m_prologueStackReferences;

    Assembler m_assembler;
    RegisterAllocator m_registerAllocator;
    StackAllocator m_stackAllocator;
    Vector<std::pair<Assembler::Call, JSC::FunctionPtr>, 32> m_functionCalls;

    SelectorContext m_selectorContext;
    FunctionType m_functionType;
    SelectorFragmentList m_selectorFragments;
    bool m_needsAdjacentBacktrackingStart;

    StackAllocator::StackReference m_checkingContextStackReference;

    Assembler::Label m_descendantEntryPoint;
    Assembler::Label m_indirectAdjacentEntryPoint;
    Assembler::Label m_descendantTreeWalkerBacktrackingPoint;
    Assembler::Label m_indirectAdjacentTreeWalkerBacktrackingPoint;
    Assembler::RegisterID m_descendantBacktrackingStart;
    Assembler::JumpList m_descendantBacktrackingFailureCases;
    StackAllocator::StackReference m_adjacentBacktrackingStart;
    Assembler::JumpList m_adjacentBacktrackingFailureCases;

#if CSS_SELECTOR_JIT_DEBUGGING
    const CSSSelector* m_originalSelector;
#endif
};

const Assembler::RegisterID SelectorCodeGenerator::returnRegister = JSC::GPRInfo::returnValueGPR;
const Assembler::RegisterID SelectorCodeGenerator::elementAddressRegister = JSC::GPRInfo::argumentGPR0;
const Assembler::RegisterID SelectorCodeGenerator::checkingContextRegister = JSC::GPRInfo::argumentGPR1;
const Assembler::RegisterID SelectorCodeGenerator::callFrameRegister = JSC::GPRInfo::callFrameRegister;

enum class FragmentsLevel {
    Root = 0,
    InFunctionalPseudoType = 1
};

static FunctionType constructFragments(const CSSSelector* rootSelector, SelectorContext, SelectorFragmentList& selectorFragments, FragmentsLevel, FragmentPositionInRootFragments);

static void computeBacktrackingInformation(SelectorFragmentList& selectorFragments, bool& needsAdjacentBacktrackingStart);

SelectorCompilationStatus compileSelector(const CSSSelector* lastSelector, JSC::VM* vm, SelectorContext selectorContext, JSC::MacroAssemblerCodeRef& codeRef)
{
    if (!vm->canUseJIT())
        return SelectorCompilationStatus::CannotCompile;
    SelectorCodeGenerator codeGenerator(lastSelector, selectorContext);
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

static inline bool shouldUseRenderStyleFromCheckingContext(const SelectorFragment& fragment)
{
    // Return true if the position of this fragment is Rightmost in the root fragments.
    // In this case, we should use the RenderStyle stored in the CheckingContext.
    return fragment.relationToRightFragment == FragmentRelation::Rightmost && fragment.positionInRootFragments == FragmentPositionInRootFragments::Rightmost;
}

static inline FunctionType addPseudoClassType(const CSSSelector& selector, SelectorFragment& fragment, SelectorContext selectorContext, FragmentPositionInRootFragments positionInRootFragments)
{
    CSSSelector::PseudoClassType type = selector.pseudoClassType();
    switch (type) {
    // Unoptimized pseudo selector. They are just function call to a simple testing function.
    case CSSSelector::PseudoClassAutofill:
        fragment.unoptimizedPseudoClasses.append(JSC::FunctionPtr(isAutofilled));
        return FunctionType::SimpleSelectorChecker;
    case CSSSelector::PseudoClassChecked:
        fragment.unoptimizedPseudoClasses.append(JSC::FunctionPtr(isChecked));
        return FunctionType::SimpleSelectorChecker;
    case CSSSelector::PseudoClassDefault:
        fragment.unoptimizedPseudoClasses.append(JSC::FunctionPtr(isDefaultButtonForForm));
        return FunctionType::SimpleSelectorChecker;
    case CSSSelector::PseudoClassDisabled:
        fragment.unoptimizedPseudoClasses.append(JSC::FunctionPtr(isDisabled));
        return FunctionType::SimpleSelectorChecker;
    case CSSSelector::PseudoClassEnabled:
        fragment.unoptimizedPseudoClasses.append(JSC::FunctionPtr(isEnabled));
        return FunctionType::SimpleSelectorChecker;
    case CSSSelector::PseudoClassFocus:
        fragment.unoptimizedPseudoClasses.append(JSC::FunctionPtr(SelectorChecker::matchesFocusPseudoClass));
        return FunctionType::SimpleSelectorChecker;
    case CSSSelector::PseudoClassInRange:
        fragment.unoptimizedPseudoClasses.append(JSC::FunctionPtr(isInRange));
        return FunctionType::SimpleSelectorChecker;
    case CSSSelector::PseudoClassIndeterminate:
        fragment.unoptimizedPseudoClasses.append(JSC::FunctionPtr(shouldAppearIndeterminate));
        return FunctionType::SimpleSelectorChecker;
    case CSSSelector::PseudoClassInvalid:
        fragment.unoptimizedPseudoClasses.append(JSC::FunctionPtr(isInvalid));
        return FunctionType::SimpleSelectorChecker;
    case CSSSelector::PseudoClassOptional:
        fragment.unoptimizedPseudoClasses.append(JSC::FunctionPtr(isOptionalFormControl));
        return FunctionType::SimpleSelectorChecker;
    case CSSSelector::PseudoClassOutOfRange:
        fragment.unoptimizedPseudoClasses.append(JSC::FunctionPtr(isOutOfRange));
        return FunctionType::SimpleSelectorChecker;
    case CSSSelector::PseudoClassReadOnly:
        fragment.unoptimizedPseudoClasses.append(JSC::FunctionPtr(matchesReadOnlyPseudoClass));
        return FunctionType::SimpleSelectorChecker;
    case CSSSelector::PseudoClassReadWrite:
        fragment.unoptimizedPseudoClasses.append(JSC::FunctionPtr(matchesReadWritePseudoClass));
        return FunctionType::SimpleSelectorChecker;
    case CSSSelector::PseudoClassRequired:
        fragment.unoptimizedPseudoClasses.append(JSC::FunctionPtr(isRequiredFormControl));
        return FunctionType::SimpleSelectorChecker;
    case CSSSelector::PseudoClassValid:
        fragment.unoptimizedPseudoClasses.append(JSC::FunctionPtr(isValid));
        return FunctionType::SimpleSelectorChecker;
#if ENABLE(FULLSCREEN_API)
    case CSSSelector::PseudoClassFullScreen:
        fragment.unoptimizedPseudoClasses.append(JSC::FunctionPtr(matchesFullScreenPseudoClass));
        return FunctionType::SimpleSelectorChecker;
#endif
#if ENABLE(VIDEO_TRACK)
    case CSSSelector::PseudoClassFuture:
        fragment.unoptimizedPseudoClasses.append(JSC::FunctionPtr(matchesFutureCuePseudoClass));
        return FunctionType::SimpleSelectorChecker;
    case CSSSelector::PseudoClassPast:
        fragment.unoptimizedPseudoClasses.append(JSC::FunctionPtr(matchesPastCuePseudoClass));
        return FunctionType::SimpleSelectorChecker;
#endif

    // Optimized pseudo selectors.
    case CSSSelector::PseudoClassAnyLink:
        fragment.pseudoClasses.add(CSSSelector::PseudoClassLink);
        return FunctionType::SimpleSelectorChecker;

    case CSSSelector::PseudoClassLink:
    case CSSSelector::PseudoClassRoot:
    case CSSSelector::PseudoClassTarget:
        fragment.pseudoClasses.add(type);
        return FunctionType::SimpleSelectorChecker;

    case CSSSelector::PseudoClassActive:
    case CSSSelector::PseudoClassFirstChild:
    case CSSSelector::PseudoClassHover:
    case CSSSelector::PseudoClassLastChild:
    case CSSSelector::PseudoClassOnlyChild:
        fragment.pseudoClasses.add(type);
        if (selectorContext == SelectorContext::QuerySelector)
            return FunctionType::SimpleSelectorChecker;
        return FunctionType::SelectorCheckerWithCheckingContext;

    case CSSSelector::PseudoClassNthChild:
        {
            if (!selector.parseNth())
                return FunctionType::CannotMatchAnything;

            int a = selector.nthA();
            int b = selector.nthB();

            // The element count is always positive.
            if (a <= 0 && b < 1)
                return FunctionType::CannotMatchAnything;

            fragment.nthChildFilters.append(std::pair<int, int>(a, b));
            if (selectorContext == SelectorContext::QuerySelector)
                return FunctionType::SimpleSelectorChecker;
            return FunctionType::SelectorCheckerWithCheckingContext;
        }

    case CSSSelector::PseudoClassNot:
        {
            const CSSSelectorList* selectorList = selector.selectorList();

            if (!selectorList)
                return FunctionType::CannotMatchAnything;

            SelectorFragmentList notFragments;
            FunctionType functionType = constructFragments(selectorList->first(), selectorContext, notFragments, FragmentsLevel::InFunctionalPseudoType, positionInRootFragments);

            // Since this is not pseudo class filter, CannotMatchAnything implies this filter always passes.
            if (functionType == FunctionType::CannotMatchAnything)
                return FunctionType::SimpleSelectorChecker;

            if (functionType == FunctionType::CannotCompile)
                return functionType;

            ASSERT(notFragments.size() == 1);
            if (notFragments.size() != 1)
                return FunctionType::CannotCompile;

            const SelectorFragment& subFragment = notFragments.first();

            // FIXME: Currently we don't support visitedMatchType.
            if (subFragment.pseudoClasses.contains(CSSSelector::PseudoClassLink))
                return FunctionType::CannotCompile;

            fragment.notFilters.append(subFragment);
            return functionType;
        }

    case CSSSelector::PseudoClassAny:
        {
            Vector<SelectorFragment, 32> anyFragments;
            FunctionType functionType = FunctionType::SimpleSelectorChecker;
            for (const CSSSelector* rootSelector = selector.selectorList()->first(); rootSelector; rootSelector = CSSSelectorList::next(rootSelector)) {
                SelectorFragmentList fragmentList;
                FunctionType subFunctionType = constructFragments(rootSelector, selectorContext, fragmentList, FragmentsLevel::InFunctionalPseudoType, positionInRootFragments);

                // Since this fragment always unmatch against the element, don't insert it to anyFragments.
                if (subFunctionType == FunctionType::CannotMatchAnything)
                    continue;

                if (subFunctionType == FunctionType::CannotCompile)
                    return FunctionType::CannotCompile;

                // :any() may not contain complex selectors which have combinators.
                ASSERT(fragmentList.size() == 1);
                if (fragmentList.size() != 1)
                    return FunctionType::CannotCompile;

                const SelectorFragment& subFragment = fragmentList.first();
                anyFragments.append(subFragment);
                functionType = mostRestrictiveFunctionType(functionType, subFunctionType);
            }

            // Since all fragments in :any() cannot match anything, this :any() filter cannot match anything.
            if (anyFragments.isEmpty())
                return FunctionType::CannotMatchAnything;

            ASSERT(!anyFragments.isEmpty());
            fragment.anyFilters.append(anyFragments);

            return functionType;
        }
    case CSSSelector::PseudoClassLang:
        {
            const AtomicString& argument = selector.argument();
            if (argument.isEmpty())
                return FunctionType::CannotMatchAnything;

            if (!fragment.langFilter)
                fragment.langFilter = &argument;
            else if (*fragment.langFilter != argument) {
                // If there are multiple definition, we only care about the most restrictive one.
                if (argument.startsWith(*fragment.langFilter, false))
                    fragment.langFilter = &argument;
                else if (fragment.langFilter->startsWith(argument, false))
                    { } // The existing filter is more restrictive.
                else
                    return FunctionType::CannotMatchAnything;
            }
            return FunctionType::SimpleSelectorChecker;
        }

    default:
        break;
    }
    return FunctionType::CannotCompile;
}

inline SelectorCodeGenerator::SelectorCodeGenerator(const CSSSelector* rootSelector, SelectorContext selectorContext)
    : m_stackAllocator(m_assembler)
    , m_selectorContext(selectorContext)
    , m_functionType(FunctionType::SimpleSelectorChecker)
    , m_needsAdjacentBacktrackingStart(false)
#if CSS_SELECTOR_JIT_DEBUGGING
    , m_originalSelector(rootSelector)
#endif
{
#if CSS_SELECTOR_JIT_DEBUGGING
    dataLogF("Compiling \"%s\"\n", m_originalSelector->selectorText().utf8().data());
#endif

    m_functionType = constructFragments(rootSelector, m_selectorContext, m_selectorFragments, FragmentsLevel::Root, FragmentPositionInRootFragments::Rightmost);
    if (m_functionType != FunctionType::CannotCompile && m_functionType != FunctionType::CannotMatchAnything)
        computeBacktrackingInformation(m_selectorFragments, m_needsAdjacentBacktrackingStart);
}

static bool pseudoClassOnlyMatchesLinksInQuirksMode(const CSSSelector& selector)
{
    CSSSelector::PseudoClassType pseudoClassType = selector.pseudoClassType();
    return pseudoClassType == CSSSelector::PseudoClassHover || pseudoClassType == CSSSelector::PseudoClassActive;
}

static FunctionType constructFragments(const CSSSelector* rootSelector, SelectorContext selectorContext, SelectorFragmentList& selectorFragments, FragmentsLevel fragmentLevel, FragmentPositionInRootFragments positionInRootFragments)
{
    SelectorFragment fragment;
    FragmentRelation relationToPreviousFragment = FragmentRelation::Rightmost;
    FunctionType functionType = FunctionType::SimpleSelectorChecker;
    for (const CSSSelector* selector = rootSelector; selector; selector = selector->tagHistory()) {
        switch (selector->m_match) {
        case CSSSelector::Tag:
            ASSERT(!fragment.tagName);
            fragment.tagName = &(selector->tagQName());
            if (*fragment.tagName != anyQName())
                fragment.onlyMatchesLinksInQuirksMode = false;
            break;
        case CSSSelector::Id: {
            const AtomicString& id = selector->value();
            if (fragment.id) {
                if (id != *fragment.id)
                    return FunctionType::CannotMatchAnything;
            } else
                fragment.id = &(selector->value());
            fragment.onlyMatchesLinksInQuirksMode = false;
            break;
        }
        case CSSSelector::Class:
            fragment.classNames.append(selector->value().impl());
            fragment.onlyMatchesLinksInQuirksMode = false;
            break;
        case CSSSelector::PseudoClass: {
            FragmentPositionInRootFragments subPosition = positionInRootFragments;
            if (relationToPreviousFragment != FragmentRelation::Rightmost)
                subPosition = FragmentPositionInRootFragments::NotRightmost;

            functionType = mostRestrictiveFunctionType(functionType, addPseudoClassType(*selector, fragment, selectorContext, subPosition));
            if (!pseudoClassOnlyMatchesLinksInQuirksMode(*selector))
                fragment.onlyMatchesLinksInQuirksMode = false;
            if (functionType == FunctionType::CannotCompile || functionType == FunctionType::CannotMatchAnything)
                return functionType;
            break;
        }

        case CSSSelector::List:
            if (selector->value().contains(' '))
                return FunctionType::CannotMatchAnything;
            FALLTHROUGH;
        case CSSSelector::Begin:
        case CSSSelector::End:
        case CSSSelector::Contain:
            if (selector->value().isEmpty())
                return FunctionType::CannotMatchAnything;
            FALLTHROUGH;
        case CSSSelector::Exact:
        case CSSSelector::Hyphen:
            fragment.onlyMatchesLinksInQuirksMode = false;
            fragment.attributes.append(AttributeMatchingInfo(selector, HTMLDocument::isCaseSensitiveAttribute(selector->attribute())));
            break;

        case CSSSelector::Set:
            fragment.onlyMatchesLinksInQuirksMode = false;
            fragment.attributes.append(AttributeMatchingInfo(selector, true));
            break;
        case CSSSelector::PagePseudoClass:
            fragment.onlyMatchesLinksInQuirksMode = false;
            // Pseudo page class are only relevant for style resolution, they are ignored for matching.
            break;
        case CSSSelector::Unknown:
            ASSERT_NOT_REACHED();
            return FunctionType::CannotMatchAnything;
        case CSSSelector::PseudoElement:
            fragment.onlyMatchesLinksInQuirksMode = false;
            return FunctionType::CannotCompile;
        }

        CSSSelector::Relation relation = selector->relation();
        if (relation == CSSSelector::SubSelector)
            continue;

        if (relation == CSSSelector::ShadowDescendant && !selector->isLastInTagHistory())
            return FunctionType::CannotCompile;

        if (relation == CSSSelector::DirectAdjacent || relation == CSSSelector::IndirectAdjacent) {
            FunctionType relationFunctionType = FunctionType::SelectorCheckerWithCheckingContext;
            if (selectorContext == SelectorContext::QuerySelector)
                relationFunctionType = FunctionType::SimpleSelectorChecker;
            functionType = mostRestrictiveFunctionType(functionType, relationFunctionType);
        }

        fragment.relationToLeftFragment = fragmentRelationForSelectorRelation(relation);
        fragment.relationToRightFragment = relationToPreviousFragment;
        fragment.positionInRootFragments = positionInRootFragments;
        relationToPreviousFragment = fragment.relationToLeftFragment;

        if (fragmentLevel == FragmentsLevel::InFunctionalPseudoType)
            fragment.onlyMatchesLinksInQuirksMode = false;
        selectorFragments.append(fragment);
        fragment = SelectorFragment();
    }
    return functionType;
}

static inline bool attributeNameTestingRequiresNamespaceRegister(const CSSSelector& attributeSelector)
{
    return attributeSelector.attribute().prefix() != starAtom && !attributeSelector.attribute().namespaceURI().isNull();
}

static inline bool attributeValueTestingRequiresCaseFoldingRegister(const AttributeMatchingInfo& attributeInfo)
{
    return !attributeInfo.canDefaultToCaseSensitiveValueMatch();
}

// Strict minimum to match anything interesting:
// Element + BacktrackingRegister + ElementData + a pointer to values + an index on that pointer + the value we expect;
static const unsigned minimumRequiredRegisterCount = 6;
// Element + ElementData + scratchRegister + attributeArrayPointer + expectedLocalName + (qualifiedNameImpl && expectedValue).
static const unsigned minimumRequiredRegisterCountForAttributeFilter = 6;

// Element + parentElement + elementCounter + previousSibling + elementRareData + cachedChildIndex.
static const unsigned minimumRequiredRegisterCountForNthChildFilter = 6;

static inline unsigned minimumRegisterRequirements(const SelectorFragment& selectorFragment)
{
    unsigned minimum = minimumRequiredRegisterCount;
    const Vector<AttributeMatchingInfo>& attributes = selectorFragment.attributes;

    unsigned backtrackingRegisterRequirements = 0;
    if (selectorFragment.backtrackingFlags & BacktrackingFlag::InChainWithDescendantTail)
        backtrackingRegisterRequirements = 1; // If there is a DescendantTail, there is a backtracking register.

    // Attributes cause some register pressure.
    unsigned attributeCount = attributes.size();
    for (unsigned attributeIndex = 0; attributeIndex < attributeCount; ++attributeIndex) {
        unsigned attributeMinimum = minimumRequiredRegisterCountForAttributeFilter + backtrackingRegisterRequirements;

        if (attributeIndex + 1 < attributeCount)
            attributeMinimum += 2; // For the local copy of the counter and attributeArrayPointer.

        const AttributeMatchingInfo& attributeInfo = attributes[attributeIndex];
        const CSSSelector& attributeSelector = attributeInfo.selector();
        if (attributeNameTestingRequiresNamespaceRegister(attributeSelector)
            || attributeValueTestingRequiresCaseFoldingRegister(attributeInfo))
            attributeMinimum += 1;

        minimum = std::max(minimum, attributeMinimum);
    }

    if (!selectorFragment.nthChildFilters.isEmpty())
        minimum = std::max(minimum, minimumRequiredRegisterCountForNthChildFilter + backtrackingRegisterRequirements);

    // :not pseudo class filters cause some register pressure.
    for (const SelectorFragment& subFragment : selectorFragment.notFilters) {
        unsigned notFilterMinimum = minimumRegisterRequirements(subFragment) + backtrackingRegisterRequirements;
        minimum = std::max(minimum, notFilterMinimum);
    }

    // :any pseudo class filters cause some register pressure.
    for (const auto& subFragments : selectorFragment.anyFilters) {
        for (const SelectorFragment& subFragment : subFragments) {
            unsigned anyFilterMinimum = minimumRegisterRequirements(subFragment) + backtrackingRegisterRequirements;
            minimum = std::max(minimum, anyFilterMinimum);
        }
    }

    return minimum;
}

static inline unsigned minimumRegisterRequirements(const SelectorFragmentList& selectorFragments)
{
    unsigned minimum = minimumRequiredRegisterCount;

    for (unsigned selectorFragmentIndex = 0; selectorFragmentIndex < selectorFragments.size(); ++selectorFragmentIndex) {
        const SelectorFragment& selectorFragment = selectorFragments[selectorFragmentIndex];
        minimum = std::max(minimum, minimumRegisterRequirements(selectorFragment));
    }

    return minimum;
}

inline SelectorCompilationStatus SelectorCodeGenerator::compile(JSC::VM* vm, JSC::MacroAssemblerCodeRef& codeRef)
{
    switch (m_functionType) {
    case FunctionType::SimpleSelectorChecker:
    case FunctionType::SelectorCheckerWithCheckingContext:
        generateSelectorChecker();
        break;
    case FunctionType::CannotMatchAnything:
        m_assembler.move(Assembler::TrustedImm32(0), returnRegister);
        m_assembler.ret();
        break;
    case FunctionType::CannotCompile:
        return SelectorCompilationStatus::CannotCompile;
    }

    JSC::LinkBuffer linkBuffer(*vm, m_assembler, CSS_CODE_ID);
    for (unsigned i = 0; i < m_functionCalls.size(); i++)
        linkBuffer.link(m_functionCalls[i].first, m_functionCalls[i].second);

#if CSS_SELECTOR_JIT_DEBUGGING
    codeRef = linkBuffer.finalizeCodeWithDisassembly("CSS Selector JIT for \"%s\"", m_originalSelector->selectorText().utf8().data());
#else
    codeRef = FINALIZE_CODE(linkBuffer, ("CSS Selector JIT"));
#endif

    if (m_functionType == FunctionType::SimpleSelectorChecker || m_functionType == FunctionType::CannotMatchAnything)
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
        if (hasDescendantRelationOnTheRight)
            ++ancestorPositionSinceDescendantRelation;
        hasIndirectAdjacentRelationOnTheRightOfDirectAdjacentChain = false;
        break;
    case FragmentRelation::DirectAdjacent:
        if (hasIndirectAdjacentRelationOnTheRightOfDirectAdjacentChain)
            ++adjacentPositionSinceIndirectAdjacentTreeWalk;
        break;
    case FragmentRelation::IndirectAdjacent:
        hasIndirectAdjacentRelationOnTheRightOfDirectAdjacentChain = true;
        adjacentPositionSinceIndirectAdjacentTreeWalk = 0;
        break;
    }
}

static inline bool isFirstAncestor(unsigned ancestorPositionSinceDescendantRelation)
{
    return ancestorPositionSinceDescendantRelation == 1;
}

static inline bool isFirstAdjacent(unsigned adjacentPositionSinceIndirectAdjacentTreeWalk)
{
    return adjacentPositionSinceIndirectAdjacentTreeWalk == 1;
}

static inline BacktrackingAction solveDescendantBacktrackingActionForChild(const SelectorFragment& fragment, unsigned backtrackingStartHeightFromDescendant)
{
    // If height is invalid (e.g. There's no tag name).
    if (backtrackingStartHeightFromDescendant == invalidHeight)
        return BacktrackingAction::NoBacktracking;

    // Start backtracking from the current element.
    if (backtrackingStartHeightFromDescendant == fragment.heightFromDescendant)
        return BacktrackingAction::JumpToDescendantEntryPoint;

    // Start backtracking from the parent of current element.
    if (backtrackingStartHeightFromDescendant == (fragment.heightFromDescendant + 1))
        return BacktrackingAction::JumpToDescendantTreeWalkerEntryPoint;

    return BacktrackingAction::JumpToDescendantTail;
}

static inline BacktrackingAction solveAdjacentBacktrackingActionForDirectAdjacent(const SelectorFragment& fragment, unsigned backtrackingStartWidthFromIndirectAdjacent)
{
    // If width is invalid (e.g. There's no tag name).
    if (backtrackingStartWidthFromIndirectAdjacent == invalidWidth)
        return BacktrackingAction::NoBacktracking;

    // Start backtracking from the current element.
    if (backtrackingStartWidthFromIndirectAdjacent == fragment.widthFromIndirectAdjacent)
        return BacktrackingAction::JumpToIndirectAdjacentEntryPoint;

    // Start backtracking from the previous adjacent of current element.
    if (backtrackingStartWidthFromIndirectAdjacent == (fragment.widthFromIndirectAdjacent + 1))
        return BacktrackingAction::JumpToIndirectAdjacentTreeWalkerEntryPoint;

    return BacktrackingAction::JumpToDirectAdjacentTail;
}

static inline BacktrackingAction solveAdjacentTraversalBacktrackingAction(const SelectorFragment& fragment, bool hasDescendantRelationOnTheRight)
{
    if (!hasDescendantRelationOnTheRight)
        return BacktrackingAction::NoBacktracking;

    if (fragment.tagNameMatchedBacktrackingStartHeightFromDescendant == (fragment.heightFromDescendant + 1))
        return BacktrackingAction::JumpToDescendantTreeWalkerEntryPoint;

    return BacktrackingAction::JumpToDescendantTail;
}

static inline void solveBacktrackingAction(SelectorFragment& fragment, bool hasDescendantRelationOnTheRight, bool hasIndirectAdjacentRelationOnTheRightOfDirectAdjacentChain)
{
    switch (fragment.relationToRightFragment) {
    case FragmentRelation::Rightmost:
    case FragmentRelation::Descendant:
        break;
    case FragmentRelation::Child:
        // Failure to match the element should resume matching at the nearest ancestor/descendant entry point.
        if (hasDescendantRelationOnTheRight) {
            fragment.matchingTagNameBacktrackingAction = solveDescendantBacktrackingActionForChild(fragment, fragment.tagNameNotMatchedBacktrackingStartHeightFromDescendant);
            fragment.matchingPostTagNameBacktrackingAction = solveDescendantBacktrackingActionForChild(fragment, fragment.tagNameMatchedBacktrackingStartHeightFromDescendant);
        }
        break;
    case FragmentRelation::DirectAdjacent:
        // Failure on traversal implies no other sibling traversal can match. Matching should resume at the
        // nearest ancestor/descendant traversal.
        fragment.traversalBacktrackingAction = solveAdjacentTraversalBacktrackingAction(fragment, hasDescendantRelationOnTheRight);

        // If the rightmost relation is a indirect adjacent, matching sould resume from there.
        // Otherwise, we resume from the latest ancestor/descendant if any.
        if (hasIndirectAdjacentRelationOnTheRightOfDirectAdjacentChain) {
            fragment.matchingTagNameBacktrackingAction = solveAdjacentBacktrackingActionForDirectAdjacent(fragment, fragment.tagNameNotMatchedBacktrackingStartWidthFromIndirectAdjacent);
            fragment.matchingPostTagNameBacktrackingAction = solveAdjacentBacktrackingActionForDirectAdjacent(fragment, fragment.tagNameMatchedBacktrackingStartWidthFromIndirectAdjacent);
        } else if (hasDescendantRelationOnTheRight) {
            // Since we resume from the latest ancestor/descendant, the action is the same as the traversal action.
            fragment.matchingTagNameBacktrackingAction = fragment.traversalBacktrackingAction;
            fragment.matchingPostTagNameBacktrackingAction = fragment.traversalBacktrackingAction;
        }
        break;
    case FragmentRelation::IndirectAdjacent:
        // Failure on traversal implies no other sibling matching will succeed. Matching can resume
        // from the latest ancestor/descendant.
        fragment.traversalBacktrackingAction = solveAdjacentTraversalBacktrackingAction(fragment, hasDescendantRelationOnTheRight);
        break;
    }
}

enum class TagNameEquality {
    StrictlyNotEqual,
    MaybeEqual,
    StrictlyEqual
};

static inline TagNameEquality equalTagNames(const QualifiedName* lhs, const QualifiedName* rhs)
{
    if (!lhs || *lhs == anyQName())
        return TagNameEquality::MaybeEqual;

    if (!rhs || *rhs == anyQName())
        return TagNameEquality::MaybeEqual;

    ASSERT(lhs && rhs);

    const AtomicString& lhsLocalName = lhs->localName();
    const AtomicString& rhsLocalName = rhs->localName();
    if (lhsLocalName != starAtom && rhsLocalName != starAtom) {
        if (lhsLocalName != rhsLocalName)
            return TagNameEquality::StrictlyNotEqual;
        return TagNameEquality::StrictlyEqual;
    }

    const AtomicString& lhsNamespaceURI = lhs->namespaceURI();
    const AtomicString& rhsNamespaceURI = rhs->namespaceURI();
    if (lhsNamespaceURI != starAtom && rhsNamespaceURI != starAtom) {
        if (lhsNamespaceURI != rhsNamespaceURI)
            return TagNameEquality::StrictlyNotEqual;
        return TagNameEquality::StrictlyEqual;
    }

    return TagNameEquality::MaybeEqual;
}

static inline bool equalTagNamePatterns(const TagNamePattern& lhs, const QualifiedName* rhs)
{
    TagNameEquality result = equalTagNames(lhs.tagName, rhs);
    if (result == TagNameEquality::MaybeEqual)
        return true;

    // If both rhs & lhs have actual localName (or NamespaceURI),
    // TagNameEquality result becomes StrictlyEqual or StrictlyNotEqual Since inverted lhs never matches on rhs.
    bool equal = result == TagNameEquality::StrictlyEqual;
    if (lhs.inverted)
        return !equal;
    return equal;
}

// Find the largest matching prefix from already known tagNames.
// And by using this, compute an appropriate height of backtracking start element from the closest base element in the chain.
static inline unsigned computeBacktrackingStartOffsetInChain(const TagNameList& tagNames, unsigned maxPrefixSize)
{
    RELEASE_ASSERT(!tagNames.isEmpty());
    RELEASE_ASSERT(maxPrefixSize < tagNames.size());

    for (unsigned largestPrefixSize = maxPrefixSize; largestPrefixSize > 0; --largestPrefixSize) {
        unsigned offsetToLargestPrefix = tagNames.size() - largestPrefixSize;
        bool matched = true;
        // Since TagNamePatterns are pushed to a tagNames, check tagNames with reverse order.
        for (unsigned i = 0; i < largestPrefixSize; ++i) {
            unsigned lastIndex = tagNames.size() - 1;
            unsigned currentIndex = lastIndex - i;
            if (!equalTagNamePatterns(tagNames[currentIndex], tagNames[currentIndex - offsetToLargestPrefix].tagName)) {
                matched = false;
                break;
            }
        }
        if (matched)
            return offsetToLargestPrefix;
    }
    return tagNames.size();
}

static inline void computeBacktrackingHeightFromDescendant(SelectorFragment& fragment, TagNameList& tagNamesForChildChain, bool hasDescendantRelationOnTheRight, const SelectorFragment*& previousChildFragmentInChildChain)
{
    if (!hasDescendantRelationOnTheRight)
        return;

    if (fragment.relationToRightFragment == FragmentRelation::Descendant) {
        tagNamesForChildChain.clear();

        TagNamePattern pattern;
        pattern.tagName = fragment.tagName;
        tagNamesForChildChain.append(pattern);
        fragment.heightFromDescendant = 0;
        previousChildFragmentInChildChain = nullptr;
    } else if (fragment.relationToRightFragment == FragmentRelation::Child) {
        TagNamePattern pattern;
        pattern.tagName = fragment.tagName;
        tagNamesForChildChain.append(pattern);

        unsigned maxPrefixSize = tagNamesForChildChain.size() - 1;
        if (previousChildFragmentInChildChain) {
            RELEASE_ASSERT(tagNamesForChildChain.size() >= previousChildFragmentInChildChain->tagNameMatchedBacktrackingStartHeightFromDescendant);
            maxPrefixSize = tagNamesForChildChain.size() - previousChildFragmentInChildChain->tagNameMatchedBacktrackingStartHeightFromDescendant;
        }

        if (pattern.tagName) {
            // Compute height from descendant in the case that tagName is not matched.
            tagNamesForChildChain.last().inverted = true;
            fragment.tagNameNotMatchedBacktrackingStartHeightFromDescendant = computeBacktrackingStartOffsetInChain(tagNamesForChildChain, maxPrefixSize);
        }

        // Compute height from descendant in the case that tagName is matched.
        tagNamesForChildChain.last().inverted = false;
        fragment.tagNameMatchedBacktrackingStartHeightFromDescendant = computeBacktrackingStartOffsetInChain(tagNamesForChildChain, maxPrefixSize);
        fragment.heightFromDescendant = tagNamesForChildChain.size() - 1;
        previousChildFragmentInChildChain = &fragment;
    } else {
        if (previousChildFragmentInChildChain) {
            fragment.tagNameNotMatchedBacktrackingStartHeightFromDescendant = previousChildFragmentInChildChain->tagNameNotMatchedBacktrackingStartHeightFromDescendant;
            fragment.tagNameMatchedBacktrackingStartHeightFromDescendant = previousChildFragmentInChildChain->tagNameMatchedBacktrackingStartHeightFromDescendant;
            fragment.heightFromDescendant = previousChildFragmentInChildChain->heightFromDescendant;
        } else {
            fragment.tagNameNotMatchedBacktrackingStartHeightFromDescendant = tagNamesForChildChain.size();
            fragment.tagNameMatchedBacktrackingStartHeightFromDescendant = tagNamesForChildChain.size();
            fragment.heightFromDescendant = 0;
        }
    }
}

static inline void computeBacktrackingWidthFromIndirectAdjacent(SelectorFragment& fragment, TagNameList& tagNamesForDirectAdjacentChain, bool hasIndirectAdjacentRelationOnTheRightOfDirectAdjacentChain, const SelectorFragment*& previousDirectAdjacentFragmentInDirectAdjacentChain)
{
    if (!hasIndirectAdjacentRelationOnTheRightOfDirectAdjacentChain)
        return;

    if (fragment.relationToRightFragment == FragmentRelation::IndirectAdjacent) {
        tagNamesForDirectAdjacentChain.clear();

        TagNamePattern pattern;
        pattern.tagName = fragment.tagName;
        tagNamesForDirectAdjacentChain.append(pattern);
        fragment.widthFromIndirectAdjacent = 0;
        previousDirectAdjacentFragmentInDirectAdjacentChain = nullptr;
    } else if (fragment.relationToRightFragment == FragmentRelation::DirectAdjacent) {
        TagNamePattern pattern;
        pattern.tagName = fragment.tagName;
        tagNamesForDirectAdjacentChain.append(pattern);

        unsigned maxPrefixSize = tagNamesForDirectAdjacentChain.size() - 1;
        if (previousDirectAdjacentFragmentInDirectAdjacentChain) {
            RELEASE_ASSERT(tagNamesForDirectAdjacentChain.size() >= previousDirectAdjacentFragmentInDirectAdjacentChain->tagNameMatchedBacktrackingStartWidthFromIndirectAdjacent);
            maxPrefixSize = tagNamesForDirectAdjacentChain.size() - previousDirectAdjacentFragmentInDirectAdjacentChain->tagNameMatchedBacktrackingStartWidthFromIndirectAdjacent;
        }

        if (pattern.tagName) {
            // Compute height from descendant in the case that tagName is not matched.
            tagNamesForDirectAdjacentChain.last().inverted = true;
            fragment.tagNameNotMatchedBacktrackingStartWidthFromIndirectAdjacent = computeBacktrackingStartOffsetInChain(tagNamesForDirectAdjacentChain, maxPrefixSize);
        }

        // Compute height from descendant in the case that tagName is matched.
        tagNamesForDirectAdjacentChain.last().inverted = false;
        fragment.tagNameMatchedBacktrackingStartWidthFromIndirectAdjacent = computeBacktrackingStartOffsetInChain(tagNamesForDirectAdjacentChain, maxPrefixSize);
        fragment.widthFromIndirectAdjacent = tagNamesForDirectAdjacentChain.size() - 1;
        previousDirectAdjacentFragmentInDirectAdjacentChain = &fragment;
    }
}

static bool requiresAdjacentTail(const SelectorFragment& fragment)
{
    ASSERT(fragment.traversalBacktrackingAction != BacktrackingAction::JumpToDirectAdjacentTail);
    return fragment.matchingTagNameBacktrackingAction == BacktrackingAction::JumpToDirectAdjacentTail || fragment.matchingPostTagNameBacktrackingAction == BacktrackingAction::JumpToDirectAdjacentTail;
}

static bool requiresDescendantTail(const SelectorFragment& fragment)
{
    return fragment.matchingTagNameBacktrackingAction == BacktrackingAction::JumpToDescendantTail || fragment.matchingPostTagNameBacktrackingAction == BacktrackingAction::JumpToDescendantTail || fragment.traversalBacktrackingAction == BacktrackingAction::JumpToDescendantTail;
}

void computeBacktrackingInformation(SelectorFragmentList& selectorFragments, bool& needsAdjacentBacktrackingStart)
{
    bool hasDescendantRelationOnTheRight = false;
    unsigned ancestorPositionSinceDescendantRelation = 0;
    bool hasIndirectAdjacentRelationOnTheRightOfDirectAdjacentChain = false;
    unsigned adjacentPositionSinceIndirectAdjacentTreeWalk = 0;

    bool needsAdjacentTail = false;
    bool needsDescendantTail = false;
    unsigned saveDescendantBacktrackingStartFragmentIndex = std::numeric_limits<unsigned>::max();
    unsigned saveIndirectAdjacentBacktrackingStartFragmentIndex = std::numeric_limits<unsigned>::max();

    TagNameList tagNamesForChildChain;
    TagNameList tagNamesForDirectAdjacentChain;
    const SelectorFragment* previousChildFragmentInChildChain = nullptr;
    const SelectorFragment* previousDirectAdjacentFragmentInDirectAdjacentChain = nullptr;

    for (unsigned i = 0; i < selectorFragments.size(); ++i) {
        SelectorFragment& fragment = selectorFragments[i];

        updateChainStates(fragment, hasDescendantRelationOnTheRight, ancestorPositionSinceDescendantRelation, hasIndirectAdjacentRelationOnTheRightOfDirectAdjacentChain, adjacentPositionSinceIndirectAdjacentTreeWalk);

        computeBacktrackingHeightFromDescendant(fragment, tagNamesForChildChain, hasDescendantRelationOnTheRight, previousChildFragmentInChildChain);

        computeBacktrackingWidthFromIndirectAdjacent(fragment, tagNamesForDirectAdjacentChain, hasIndirectAdjacentRelationOnTheRightOfDirectAdjacentChain, previousDirectAdjacentFragmentInDirectAdjacentChain);

#if CSS_SELECTOR_JIT_DEBUGGING
        dataLogF("Computing fragment[%d] backtracking height %u. NotMatched %u / Matched %u | width %u. NotMatched %u / Matched %u\n", i, fragment.heightFromDescendant, fragment.tagNameNotMatchedBacktrackingStartHeightFromDescendant, fragment.tagNameMatchedBacktrackingStartHeightFromDescendant, fragment.widthFromIndirectAdjacent, fragment.tagNameNotMatchedBacktrackingStartWidthFromIndirectAdjacent, fragment.tagNameMatchedBacktrackingStartWidthFromIndirectAdjacent);
#endif

        solveBacktrackingAction(fragment, hasDescendantRelationOnTheRight, hasIndirectAdjacentRelationOnTheRightOfDirectAdjacentChain);

        needsAdjacentTail |= requiresAdjacentTail(fragment);
        needsDescendantTail |= requiresDescendantTail(fragment);

        // Add code generation flags.
        if (fragment.relationToLeftFragment != FragmentRelation::Descendant && fragment.relationToRightFragment == FragmentRelation::Descendant)
            fragment.backtrackingFlags |= BacktrackingFlag::DescendantEntryPoint;
        if (fragment.relationToLeftFragment == FragmentRelation::DirectAdjacent && fragment.relationToRightFragment == FragmentRelation::IndirectAdjacent)
            fragment.backtrackingFlags |= BacktrackingFlag::IndirectAdjacentEntryPoint;
        if (fragment.relationToLeftFragment != FragmentRelation::Descendant && fragment.relationToRightFragment == FragmentRelation::Child && isFirstAncestor(ancestorPositionSinceDescendantRelation)) {
            ASSERT(saveDescendantBacktrackingStartFragmentIndex == std::numeric_limits<unsigned>::max());
            saveDescendantBacktrackingStartFragmentIndex = i;
        }
        if (fragment.relationToLeftFragment == FragmentRelation::DirectAdjacent && fragment.relationToRightFragment == FragmentRelation::DirectAdjacent && isFirstAdjacent(adjacentPositionSinceIndirectAdjacentTreeWalk)) {
            ASSERT(saveIndirectAdjacentBacktrackingStartFragmentIndex == std::numeric_limits<unsigned>::max());
            saveIndirectAdjacentBacktrackingStartFragmentIndex = i;
        }
        if (fragment.relationToLeftFragment != FragmentRelation::DirectAdjacent) {
            if (needsAdjacentTail) {
                ASSERT(fragment.relationToRightFragment == FragmentRelation::DirectAdjacent);
                ASSERT(saveIndirectAdjacentBacktrackingStartFragmentIndex != std::numeric_limits<unsigned>::max());
                fragment.backtrackingFlags |= BacktrackingFlag::DirectAdjacentTail;
                selectorFragments[saveIndirectAdjacentBacktrackingStartFragmentIndex].backtrackingFlags |= BacktrackingFlag::SaveAdjacentBacktrackingStart;
                needsAdjacentBacktrackingStart = true;
                needsAdjacentTail = false;
            }
            saveIndirectAdjacentBacktrackingStartFragmentIndex = std::numeric_limits<unsigned>::max();
        }
        if (fragment.relationToLeftFragment == FragmentRelation::Descendant) {
            if (needsDescendantTail) {
                ASSERT(saveDescendantBacktrackingStartFragmentIndex != std::numeric_limits<unsigned>::max());
                fragment.backtrackingFlags |= BacktrackingFlag::DescendantTail;
                selectorFragments[saveDescendantBacktrackingStartFragmentIndex].backtrackingFlags |= BacktrackingFlag::SaveDescendantBacktrackingStart;
                needsDescendantTail = false;
                for (unsigned j = saveDescendantBacktrackingStartFragmentIndex; j <= i; ++j)
                    selectorFragments[j].backtrackingFlags |= BacktrackingFlag::InChainWithDescendantTail;
            }
            saveDescendantBacktrackingStartFragmentIndex = std::numeric_limits<unsigned>::max();
        }
    }
}

inline bool SelectorCodeGenerator::generatePrologue()
{
#if CPU(ARM64)
    Vector<JSC::MacroAssembler::RegisterID, 2> prologueRegisters;
    prologueRegisters.append(JSC::ARM64Registers::lr);
    prologueRegisters.append(JSC::ARM64Registers::fp);
    m_prologueStackReferences = m_stackAllocator.push(prologueRegisters);
    return true;
#elif CPU(ARM_THUMB2)
    Vector<JSC::MacroAssembler::RegisterID, 2> prologueRegisters;
    prologueRegisters.append(JSC::ARMRegisters::lr);
    // r6 is tempRegister in RegisterAllocator.h and addressTempRegister in MacroAssemblerARMv7.h and must be preserved by the callee.
    prologueRegisters.append(JSC::ARMRegisters::r6);
    m_prologueStackReferences = m_stackAllocator.push(prologueRegisters);
    return true;
#elif CPU(X86_64) && CSS_SELECTOR_JIT_DEBUGGING
    Vector<JSC::MacroAssembler::RegisterID, 1> prologueRegister;
    prologueRegister.append(callFrameRegister);
    m_prologueStackReferences = m_stackAllocator.push(prologueRegister);
    return true;
#endif
    return false;
}

inline void SelectorCodeGenerator::generateEpilogue()
{
#if CPU(ARM64)
    Vector<JSC::MacroAssembler::RegisterID, 2> prologueRegisters;
    prologueRegisters.append(JSC::ARM64Registers::lr);
    prologueRegisters.append(JSC::ARM64Registers::fp);
    m_stackAllocator.pop(m_prologueStackReferences, prologueRegisters);
#elif CPU(ARM_THUMB2)
    Vector<JSC::MacroAssembler::RegisterID, 2> prologueRegisters;
    prologueRegisters.append(JSC::ARMRegisters::lr);
    prologueRegisters.append(JSC::ARMRegisters::r6);
    m_stackAllocator.pop(m_prologueStackReferences, prologueRegisters);
#elif CPU(X86_64) && CSS_SELECTOR_JIT_DEBUGGING
    Vector<JSC::MacroAssembler::RegisterID, 1> prologueRegister;
    prologueRegister.append(callFrameRegister);
    m_stackAllocator.pop(m_prologueStackReferences, prologueRegister);
#endif
}

void SelectorCodeGenerator::generateSelectorChecker()
{
    StackAllocator::StackReferenceVector calleeSavedRegisterStackReferences;
    bool reservedCalleeSavedRegisters = false;
    unsigned availableRegisterCount = m_registerAllocator.availableRegisterCount();
    unsigned minimumRegisterCountForAttributes = minimumRegisterRequirements(m_selectorFragments);
#if CSS_SELECTOR_JIT_DEBUGGING
    dataLogF("Compiling with minimum required register count %u\n", minimumRegisterCountForAttributes);
#endif
    
    bool needsEpilogue = generatePrologue();
    
    ASSERT(minimumRegisterCountForAttributes <= maximumRegisterCount);
    if (availableRegisterCount < minimumRegisterCountForAttributes) {
        reservedCalleeSavedRegisters = true;
        calleeSavedRegisterStackReferences = m_stackAllocator.push(m_registerAllocator.reserveCalleeSavedRegisters(minimumRegisterCountForAttributes - availableRegisterCount));
    }

    m_registerAllocator.allocateRegister(elementAddressRegister);

    if (m_functionType == FunctionType::SelectorCheckerWithCheckingContext)
        m_checkingContextStackReference = m_stackAllocator.push(checkingContextRegister);

    if (m_needsAdjacentBacktrackingStart)
        m_adjacentBacktrackingStart = m_stackAllocator.allocateUninitialized();

    Assembler::JumpList failureCases;

    for (unsigned i = 0; i < m_selectorFragments.size(); ++i) {
        const SelectorFragment& fragment = m_selectorFragments[i];
        switch (fragment.relationToRightFragment) {
        case FragmentRelation::Rightmost:
            generateElementMatching(failureCases, failureCases, fragment);
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
        generateBacktrackingTailsIfNeeded(failureCases, fragment);
    }

    m_registerAllocator.deallocateRegister(elementAddressRegister);

    if (m_functionType == FunctionType::SimpleSelectorChecker) {
        if (!m_needsAdjacentBacktrackingStart && !reservedCalleeSavedRegisters && !needsEpilogue) {
            // Success.
            m_assembler.move(Assembler::TrustedImm32(1), returnRegister);
            m_assembler.ret();

            // Failure.
            if (!failureCases.empty()) {
                failureCases.link(&m_assembler);
                m_assembler.move(Assembler::TrustedImm32(0), returnRegister);
                m_assembler.ret();
            }
        } else {
            // Success.
            m_assembler.move(Assembler::TrustedImm32(1), returnRegister);

            // Failure.
            if (!failureCases.empty()) {
                Assembler::Jump skipFailureCase = m_assembler.jump();
                failureCases.link(&m_assembler);
                m_assembler.move(Assembler::TrustedImm32(0), returnRegister);
                skipFailureCase.link(&m_assembler);
            }

            if (m_needsAdjacentBacktrackingStart)
                m_stackAllocator.popAndDiscardUpTo(m_adjacentBacktrackingStart);
            if (reservedCalleeSavedRegisters)
                m_stackAllocator.pop(calleeSavedRegisterStackReferences, m_registerAllocator.restoreCalleeSavedRegisters());
            if (needsEpilogue)
                generateEpilogue();
            m_assembler.ret();
        }
    } else {
        ASSERT(m_functionType == FunctionType::SelectorCheckerWithCheckingContext);

        // Success.
        m_assembler.move(Assembler::TrustedImm32(1), returnRegister);

        // Failure.
        if (!failureCases.empty()) {
            Assembler::Jump skipFailureCase = m_assembler.jump();
            failureCases.link(&m_assembler);
            m_assembler.move(Assembler::TrustedImm32(0), returnRegister);
            skipFailureCase.link(&m_assembler);
        }

        m_stackAllocator.popAndDiscardUpTo(m_checkingContextStackReference);
        if (reservedCalleeSavedRegisters)
            m_stackAllocator.pop(calleeSavedRegisterStackReferences, m_registerAllocator.restoreCalleeSavedRegisters());
        if (needsEpilogue)
            generateEpilogue();
        m_assembler.ret();
    }
}

static inline Assembler::Jump testIsElementFlagOnNode(Assembler::ResultCondition condition, Assembler& assembler, Assembler::RegisterID nodeAddress)
{
    return assembler.branchTest32(condition, Assembler::Address(nodeAddress, Node::nodeFlagsMemoryOffset()), Assembler::TrustedImm32(Node::flagIsElement()));
}

void SelectorCodeGenerator::generateWalkToParentNode(Assembler::RegisterID targetRegister)
{
    m_assembler.loadPtr(Assembler::Address(elementAddressRegister, Node::parentNodeMemoryOffset()), targetRegister);
}

void SelectorCodeGenerator::generateWalkToParentElement(Assembler::JumpList& failureCases, Assembler::RegisterID targetRegister)
{
    //    ContainerNode* parent = parentNode()
    //    if (!parent || !parent->isElementNode())
    //         failure
    generateWalkToParentNode(targetRegister);
    failureCases.append(m_assembler.branchTestPtr(Assembler::Zero, targetRegister));
    failureCases.append(testIsElementFlagOnNode(Assembler::Zero, m_assembler, targetRegister));
}

void SelectorCodeGenerator::generateParentElementTreeWalker(Assembler::JumpList& failureCases, const SelectorFragment& fragment)
{
    Assembler::JumpList traversalFailureCases;
    generateWalkToParentElement(traversalFailureCases, elementAddressRegister);
    linkFailures(failureCases, fragment.traversalBacktrackingAction, traversalFailureCases);

    Assembler::JumpList matchingTagNameFailureCases;
    Assembler::JumpList matchingPostTagNameFailureCases;
    generateElementMatching(matchingTagNameFailureCases, matchingPostTagNameFailureCases, fragment);
    linkFailures(failureCases, fragment.matchingTagNameBacktrackingAction, matchingTagNameFailureCases);
    linkFailures(failureCases, fragment.matchingPostTagNameBacktrackingAction, matchingPostTagNameFailureCases);

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

    Assembler::JumpList matchingFailureCases;
    generateElementMatching(matchingFailureCases, matchingFailureCases, fragment);
    matchingFailureCases.linkTo(loopStart, &m_assembler);
}

inline void SelectorCodeGenerator::generateWalkToNextAdjacentElement(Assembler::JumpList& failureCases, Assembler::RegisterID workRegister)
{
    Assembler::Label loopStart = m_assembler.label();
    m_assembler.loadPtr(Assembler::Address(workRegister, Node::nextSiblingMemoryOffset()), workRegister);
    failureCases.append(m_assembler.branchTestPtr(Assembler::Zero, workRegister));
    testIsElementFlagOnNode(Assembler::Zero, m_assembler, workRegister).linkTo(loopStart, &m_assembler);
}

inline void SelectorCodeGenerator::generateWalkToPreviousAdjacentElement(Assembler::JumpList& failureCases, Assembler::RegisterID workRegister)
{
    Assembler::Label loopStart = m_assembler.label();
    m_assembler.loadPtr(Assembler::Address(workRegister, Node::previousSiblingMemoryOffset()), workRegister);
    failureCases.append(m_assembler.branchTestPtr(Assembler::Zero, workRegister));
    testIsElementFlagOnNode(Assembler::Zero, m_assembler, workRegister).linkTo(loopStart, &m_assembler);
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

    Assembler::JumpList traversalFailureCases;
    generateWalkToPreviousAdjacentElement(traversalFailureCases, previousSibling);
    linkFailures(failureCases, fragment.traversalBacktrackingAction, traversalFailureCases);

    // On success, move previousSibling over to elementAddressRegister if we could not work on elementAddressRegister directly.
    if (!useTailOnTraversalFailure) {
        m_assembler.move(previousSibling, elementAddressRegister);
        m_registerAllocator.deallocateRegister(previousSibling);
    }
}

void SelectorCodeGenerator::generateDirectAdjacentTreeWalker(Assembler::JumpList& failureCases, const SelectorFragment& fragment)
{
    markParentElementIfResolvingStyle(Node::flagChildrenAffectedByDirectAdjacentRulesFlag());

    generateWalkToPreviousAdjacent(failureCases, fragment);

    Assembler::JumpList matchingTagNameFailureCases;
    Assembler::JumpList matchingPostTagNameFailureCases;
    generateElementMatching(matchingTagNameFailureCases, matchingPostTagNameFailureCases, fragment);
    linkFailures(failureCases, fragment.matchingTagNameBacktrackingAction, matchingTagNameFailureCases);
    linkFailures(failureCases, fragment.matchingPostTagNameBacktrackingAction, matchingPostTagNameFailureCases);

    if (fragment.backtrackingFlags & BacktrackingFlag::SaveAdjacentBacktrackingStart) {
        unsigned offsetToAdjacentBacktrackingStart = m_stackAllocator.offsetToStackReference(m_adjacentBacktrackingStart);
        m_assembler.storePtr(elementAddressRegister, Assembler::Address(Assembler::stackPointerRegister, offsetToAdjacentBacktrackingStart));
    }
}

void SelectorCodeGenerator::generateIndirectAdjacentTreeWalker(Assembler::JumpList& failureCases, const SelectorFragment& fragment)
{
    markParentElementIfResolvingStyle(Element::setChildrenAffectedByForwardPositionalRules);

    Assembler::Label loopStart(m_assembler.label());

    if (fragment.backtrackingFlags & BacktrackingFlag::IndirectAdjacentEntryPoint)
        m_indirectAdjacentTreeWalkerBacktrackingPoint = m_assembler.label();

    generateWalkToPreviousAdjacent(failureCases, fragment);

    if (fragment.backtrackingFlags & BacktrackingFlag::IndirectAdjacentEntryPoint)
        m_indirectAdjacentEntryPoint = m_assembler.label();

    Assembler::JumpList localFailureCases;
    generateElementMatching(localFailureCases, localFailureCases, fragment);
    localFailureCases.linkTo(loopStart, &m_assembler);
}


void SelectorCodeGenerator::addFlagsToElementStyleFromContext(Assembler::RegisterID checkingContext, int64_t newFlag)
{
    LocalRegister childStyle(m_registerAllocator);
    m_assembler.loadPtr(Assembler::Address(checkingContext, OBJECT_OFFSETOF(CheckingContext, elementStyle)), childStyle);

    // FIXME: We should look into doing something smart in MacroAssembler instead.
    Assembler::Address flagAddress(childStyle, RenderStyle::noninheritedFlagsMemoryOffset() + RenderStyle::NonInheritedFlags::flagsMemoryOffset());
#if CPU(ARM_THUMB2)
    int32_t flagLowBits = newFlag & 0xffffffff;
    int32_t flagHighBits = newFlag >> 32;
    if (flagLowBits)
        m_assembler.or32(Assembler::TrustedImm32(flagLowBits), flagAddress);
    if (flagHighBits) {
        Assembler::Address flagHighAddress = flagAddress.withOffset(4);
        m_assembler.or32(Assembler::TrustedImm32(flagHighBits), flagHighAddress);
    }
#elif CPU(X86_64) || CPU(ARM64)
    LocalRegister flags(m_registerAllocator);
    m_assembler.load64(flagAddress, flags);
    LocalRegister isFirstChildStateFlagImmediate(m_registerAllocator);
    m_assembler.move(Assembler::TrustedImm64(newFlag), isFirstChildStateFlagImmediate);
    m_assembler.or64(isFirstChildStateFlagImmediate, flags);
    m_assembler.store64(flags, flagAddress);
#else
#error SelectorCodeGenerator::addFlagsToElementStyleFromContext not implemented for this architecture.
#endif
}

Assembler::JumpList SelectorCodeGenerator::jumpIfNoPreviousAdjacentElement()
{
    Assembler::JumpList successCase;
    LocalRegister previousSibling(m_registerAllocator);
    m_assembler.move(elementAddressRegister, previousSibling);
    generateWalkToPreviousAdjacentElement(successCase, previousSibling);
    return successCase;
}

Assembler::JumpList SelectorCodeGenerator::jumpIfNoNextAdjacentElement()
{
    Assembler::JumpList successCase;
    LocalRegister nextSibling(m_registerAllocator);
    m_assembler.move(elementAddressRegister, nextSibling);
    generateWalkToNextAdjacentElement(successCase, nextSibling);
    return successCase;
}

Assembler::Jump SelectorCodeGenerator::jumpIfNotResolvingStyle(Assembler::RegisterID checkingContext)
{
    RELEASE_ASSERT(m_selectorContext == SelectorContext::RuleCollector);

    // Get the checking context.
    unsigned offsetToCheckingContext = m_stackAllocator.offsetToStackReference(m_checkingContextStackReference);
    m_assembler.loadPtr(Assembler::Address(Assembler::stackPointerRegister, offsetToCheckingContext), checkingContext);

    // If we not resolving style, skip the whole marking.
    static_assert(sizeof(SelectorChecker::Mode) == 1, "We generate a byte load/test for the SelectorChecker::Mode.");
    return m_assembler.branch8(Assembler::NotEqual, Assembler::Address(checkingContext, OBJECT_OFFSETOF(CheckingContext, resolvingMode)), Assembler::TrustedImm32(static_cast<std::underlying_type<SelectorChecker::Mode>::type>(SelectorChecker::Mode::ResolvingStyle)));
}

static void getDocument(Assembler& assembler, Assembler::RegisterID element, Assembler::RegisterID output)
{
    assembler.loadPtr(Assembler::Address(element, Node::treeScopeMemoryOffset()), output);
    assembler.loadPtr(Assembler::Address(output, TreeScope::documentScopeMemoryOffset()), output);
}

void SelectorCodeGenerator::generateSpecialFailureInQuirksModeForActiveAndHoverIfNeeded(Assembler::JumpList& failureCases, const SelectorFragment& fragment)
{
    if (fragment.onlyMatchesLinksInQuirksMode) {
        // If the element is a link, it can always match :hover or :active.
        Assembler::Jump isLink = m_assembler.branchTest32(Assembler::NonZero, Assembler::Address(elementAddressRegister, Node::nodeFlagsMemoryOffset()), Assembler::TrustedImm32(Node::flagIsLink()));

        // Only quirks mode restrict :hover and :active.
        static_assert(sizeof(DocumentCompatibilityMode) == 1, "We generate a byte load/test for the compatibility mode.");
        LocalRegister documentAddress(m_registerAllocator);
        getDocument(m_assembler, elementAddressRegister, documentAddress);
        failureCases.append(m_assembler.branchTest8(Assembler::NonZero, Assembler::Address(documentAddress, Document::compatibilityModeMemoryOffset()), Assembler::TrustedImm32(static_cast<std::underlying_type<DocumentCompatibilityMode>::type>(DocumentCompatibilityMode::QuirksMode))));

        isLink.link(&m_assembler);
    }
}

#if CPU(ARM_THUMB2) && !CPU(APPLE_ARMV7S)
// FIXME: This could be implemented in assembly to avoid a function call, and we know the divisor at jit-compile time.
static int moduloHelper(int dividend, int divisor)
{
    return dividend % divisor;
}
#endif

// The value in inputDividend is destroyed by the modulo operation.
Assembler::Jump SelectorCodeGenerator::modulo(Assembler::ResultCondition condition, Assembler::RegisterID inputDividend, int divisor)
{
    RELEASE_ASSERT(divisor);
#if CPU(ARM64) || CPU(APPLE_ARMV7S)
    LocalRegister divisorRegister(m_registerAllocator);
    m_assembler.move(Assembler::TrustedImm32(divisor), divisorRegister);

    LocalRegister resultRegister(m_registerAllocator);
    m_assembler.m_assembler.sdiv<32>(resultRegister, inputDividend, divisorRegister);
    m_assembler.mul32(divisorRegister, resultRegister);
    return m_assembler.branchSub32(condition, inputDividend, resultRegister, resultRegister);
#elif CPU(ARM_THUMB2) && !CPU(APPLE_ARMV7S)
    LocalRegisterWithPreference divisorRegister(m_registerAllocator, JSC::GPRInfo::argumentGPR1);
    m_assembler.move(Assembler::TrustedImm32(divisor), divisorRegister);
    FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
    functionCall.setFunctionAddress(moduloHelper);
    functionCall.setTwoArguments(inputDividend, divisorRegister);
    return functionCall.callAndBranchOnBooleanReturnValue(condition);
#elif CPU(X86_64)
    // idiv takes RAX + an arbitrary register, and return RAX + RDX. Most of this code is about doing
    // an efficient allocation of those registers. If a register is already in use and is not the inputDividend,
    // we first try to copy it to a temporary register, it that is not possible we fall back to the stack.
    enum class RegisterAllocationType {
        External,
        AllocatedLocally,
        CopiedToTemporary,
        PushedToStack
    };

    // 1) Get RAX and RDX.
    // If they are already used, push them to the stack.
    Assembler::RegisterID dividend = JSC::X86Registers::eax;
    RegisterAllocationType dividendAllocation = RegisterAllocationType::External;
    StackAllocator::StackReference temporaryDividendStackReference;
    Assembler::RegisterID temporaryDividendCopy = InvalidGPRReg;
    if (inputDividend != dividend) {
        bool registerIsInUse = m_registerAllocator.allocatedRegisters().contains(dividend);
        if (registerIsInUse) {
            if (m_registerAllocator.availableRegisterCount()) {
                temporaryDividendCopy = m_registerAllocator.allocateRegister();
                m_assembler.move(dividend, temporaryDividendCopy);
                dividendAllocation = RegisterAllocationType::CopiedToTemporary;
            } else {
                temporaryDividendStackReference = m_stackAllocator.push(dividend);
                dividendAllocation = RegisterAllocationType::PushedToStack;
            }
        } else {
            m_registerAllocator.allocateRegister(dividend);
            dividendAllocation = RegisterAllocationType::AllocatedLocally;
        }
        m_assembler.move(inputDividend, dividend);
    }

    Assembler::RegisterID remainder = JSC::X86Registers::edx;
    RegisterAllocationType remainderAllocation = RegisterAllocationType::External;
    StackAllocator::StackReference temporaryRemainderStackReference;
    Assembler::RegisterID temporaryRemainderCopy = InvalidGPRReg;
    if (inputDividend != remainder) {
        bool registerIsInUse = m_registerAllocator.allocatedRegisters().contains(remainder);
        if (registerIsInUse) {
            if (m_registerAllocator.availableRegisterCount()) {
                temporaryRemainderCopy = m_registerAllocator.allocateRegister();
                m_assembler.move(remainder, temporaryRemainderCopy);
                remainderAllocation = RegisterAllocationType::CopiedToTemporary;
            } else {
                temporaryRemainderStackReference = m_stackAllocator.push(remainder);
                remainderAllocation = RegisterAllocationType::PushedToStack;
            }
        } else {
            m_registerAllocator.allocateRegister(remainder);
            remainderAllocation = RegisterAllocationType::AllocatedLocally;
        }
    }
    m_assembler.m_assembler.cdq();

    // 2) Perform the division with idiv.
    {
        LocalRegister divisorRegister(m_registerAllocator);
        m_assembler.move(Assembler::TrustedImm64(divisor), divisorRegister);
        m_assembler.m_assembler.idivl_r(divisorRegister);
        m_assembler.test32(condition, remainder);
    }

    // 3) Return RAX and RDX.
    if (remainderAllocation == RegisterAllocationType::AllocatedLocally)
        m_registerAllocator.deallocateRegister(remainder);
    else if (remainderAllocation == RegisterAllocationType::CopiedToTemporary) {
        m_assembler.move(temporaryRemainderCopy, remainder);
        m_registerAllocator.deallocateRegister(temporaryRemainderCopy);
    } else if (remainderAllocation == RegisterAllocationType::PushedToStack)
        m_stackAllocator.pop(temporaryRemainderStackReference, remainder);

    if (dividendAllocation == RegisterAllocationType::AllocatedLocally)
        m_registerAllocator.deallocateRegister(dividend);
    else if (dividendAllocation == RegisterAllocationType::CopiedToTemporary) {
        m_assembler.move(temporaryDividendCopy, dividend);
        m_registerAllocator.deallocateRegister(temporaryDividendCopy);
    } else if (dividendAllocation == RegisterAllocationType::PushedToStack)
        m_stackAllocator.pop(temporaryDividendStackReference, dividend);

    // 4) Branch on the test.
    return m_assembler.branch(condition);
#else
#error Modulo is not implemented for this architecture.
#endif
}

void SelectorCodeGenerator::moduloIsZero(Assembler::JumpList& failureCases, Assembler::RegisterID inputDividend, int divisor)
{
    if (divisor == 1 || divisor == -1)
        return;
    if (divisor == 2 || divisor == -2) {
        failureCases.append(m_assembler.branchTest32(Assembler::NonZero, inputDividend, Assembler::TrustedImm32(1)));
        return;
    }

    failureCases.append(modulo(Assembler::NonZero, inputDividend, divisor));
}

static void setNodeFlag(Assembler& assembler, Assembler::RegisterID elementAddress, int32_t flag)
{
    assembler.or32(Assembler::TrustedImm32(flag), Assembler::Address(elementAddress, Node::nodeFlagsMemoryOffset()));
}

void SelectorCodeGenerator::markParentElementIfResolvingStyle(int32_t nodeFlag)
{
    if (m_selectorContext == SelectorContext::QuerySelector)
        return;

    Assembler::JumpList skipMarking;
    {
        LocalRegister checkingContext(m_registerAllocator);
        skipMarking.append(jumpIfNotResolvingStyle(checkingContext));
    }

    LocalRegister parentElement(m_registerAllocator);
    generateWalkToParentElement(skipMarking, parentElement);

    setNodeFlag(m_assembler, parentElement, nodeFlag);

    skipMarking.link(&m_assembler);
}

void SelectorCodeGenerator::markParentElementIfResolvingStyle(JSC::FunctionPtr markingFunction)
{
    if (m_selectorContext == SelectorContext::QuerySelector)
        return;

    //     if (checkingContext.resolvingMode == ResolvingStyle) {
    //         Element* parent = element->parentNode();
    //         markingFunction(parent);
    //     }

    Assembler::JumpList skipMarking;
    {
        LocalRegister checkingContext(m_registerAllocator);
        skipMarking.append(jumpIfNotResolvingStyle(checkingContext));
    }

    // Get the parent element in a temporary register.
    Assembler::RegisterID parentElement = m_registerAllocator.allocateRegister();
    generateWalkToParentElement(skipMarking, parentElement);

    // Return the register parentElement just before the function call since we don't need it to be preserved
    // on the stack.
    m_registerAllocator.deallocateRegister(parentElement);

    // Invoke the marking function on the parent element.
    FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
    functionCall.setFunctionAddress(markingFunction);
    functionCall.setOneArgument(parentElement);
    functionCall.call();

    skipMarking.link(&m_assembler);
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
    case BacktrackingAction::JumpToIndirectAdjacentTreeWalkerEntryPoint:
        localFailureCases.linkTo(m_indirectAdjacentTreeWalkerBacktrackingPoint, &m_assembler);
        break;
    case BacktrackingAction::JumpToDirectAdjacentTail:
        m_adjacentBacktrackingFailureCases.append(localFailureCases);
        break;
    }
}

void SelectorCodeGenerator::generateAdjacentBacktrackingTail()
{
    // Recovering tail.
    m_adjacentBacktrackingFailureCases.link(&m_assembler);
    m_adjacentBacktrackingFailureCases.clear();
    unsigned offsetToAdjacentBacktrackingStart = m_stackAllocator.offsetToStackReference(m_adjacentBacktrackingStart);
    m_assembler.loadPtr(Assembler::Address(Assembler::stackPointerRegister, offsetToAdjacentBacktrackingStart), elementAddressRegister);
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

void SelectorCodeGenerator::generateBacktrackingTailsIfNeeded(Assembler::JumpList& failureCases, const SelectorFragment& fragment)
{
    if (fragment.backtrackingFlags & BacktrackingFlag::DirectAdjacentTail && fragment.backtrackingFlags & BacktrackingFlag::DescendantTail) {
        Assembler::Jump normalCase = m_assembler.jump();
        generateAdjacentBacktrackingTail();
        generateDescendantBacktrackingTail();
        normalCase.link(&m_assembler);
    } else if (fragment.backtrackingFlags & BacktrackingFlag::DirectAdjacentTail) {
        Assembler::Jump normalCase = m_assembler.jump();
        generateAdjacentBacktrackingTail();
        failureCases.append(m_assembler.jump());
        normalCase.link(&m_assembler);
    } else if (fragment.backtrackingFlags & BacktrackingFlag::DescendantTail) {
        Assembler::Jump normalCase = m_assembler.jump();
        generateDescendantBacktrackingTail();
        normalCase.link(&m_assembler);
    }
}

void SelectorCodeGenerator::generateElementMatching(Assembler::JumpList& matchingTagNameFailureCases, Assembler::JumpList& matchingPostTagNameFailureCases, const SelectorFragment& fragment)
{
    if (fragment.tagName)
        generateElementHasTagName(matchingTagNameFailureCases, *(fragment.tagName));

    if (fragment.pseudoClasses.contains(CSSSelector::PseudoClassLink))
        generateElementIsLink(matchingPostTagNameFailureCases);

    if (fragment.pseudoClasses.contains(CSSSelector::PseudoClassRoot))
        generateElementIsRoot(matchingPostTagNameFailureCases);

    if (fragment.pseudoClasses.contains(CSSSelector::PseudoClassTarget))
        generateElementIsTarget(matchingPostTagNameFailureCases);

    for (unsigned i = 0; i < fragment.unoptimizedPseudoClasses.size(); ++i)
        generateElementFunctionCallTest(matchingPostTagNameFailureCases, fragment.unoptimizedPseudoClasses[i]);

    generateElementDataMatching(matchingPostTagNameFailureCases, fragment);

    if (fragment.pseudoClasses.contains(CSSSelector::PseudoClassActive))
        generateElementIsActive(matchingPostTagNameFailureCases, fragment);
    if (fragment.pseudoClasses.contains(CSSSelector::PseudoClassHover))
        generateElementIsHovered(matchingPostTagNameFailureCases, fragment);
    if (fragment.pseudoClasses.contains(CSSSelector::PseudoClassOnlyChild))
        generateElementIsOnlyChild(matchingPostTagNameFailureCases, fragment);
    if (fragment.pseudoClasses.contains(CSSSelector::PseudoClassFirstChild))
        generateElementIsFirstChild(matchingPostTagNameFailureCases, fragment);
    if (fragment.pseudoClasses.contains(CSSSelector::PseudoClassLastChild))
        generateElementIsLastChild(matchingPostTagNameFailureCases, fragment);
    if (!fragment.nthChildFilters.isEmpty())
        generateElementIsNthChild(matchingPostTagNameFailureCases, fragment);
    if (!fragment.notFilters.isEmpty())
        generateElementMatchesNotPseudoClass(matchingPostTagNameFailureCases, fragment);
    if (!fragment.anyFilters.isEmpty())
        generateElementMatchesAnyPseudoClass(matchingPostTagNameFailureCases, fragment);
    if (fragment.langFilter)
        generateElementIsInLanguage(matchingPostTagNameFailureCases, *fragment.langFilter);
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
        const CSSSelector& attributeSelector = fragment.attributes[i].selector();
        const QualifiedName& attributeName = attributeSelector.attribute();
        if (Attribute::nameMatchesFilter(HTMLNames::styleAttr, attributeName.prefix(), attributeName.localName(), attributeName.namespaceURI()))
            return true;

        const AtomicString& canonicalLocalName = attributeSelector.attributeCanonicalLocalName();
        if (attributeName.localName() != canonicalLocalName
            && Attribute::nameMatchesFilter(HTMLNames::styleAttr, attributeName.prefix(), attributeSelector.attributeCanonicalLocalName(), attributeName.namespaceURI())) {
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
    functionCall.setOneArgument(elementAddress);
    functionCall.call();

    styleAttributeNotDirty.link(&m_assembler);
}

static inline bool canMatchAnimatableSVGAttribute(const SelectorFragment& fragment)
{
    for (unsigned i = 0; i < fragment.attributes.size(); ++i) {
        const CSSSelector& attributeSelector = fragment.attributes[i].selector();
        const QualifiedName& selectorAttributeName = attributeSelector.attribute();

        const QualifiedName& candidateForLocalName = SVGElement::animatableAttributeForName(selectorAttributeName.localName());
        if (Attribute::nameMatchesFilter(candidateForLocalName, selectorAttributeName.prefix(), selectorAttributeName.localName(), selectorAttributeName.namespaceURI()))
            return true;

        const AtomicString& canonicalLocalName = attributeSelector.attributeCanonicalLocalName();
        if (selectorAttributeName.localName() != canonicalLocalName) {
            const QualifiedName& candidateForCanonicalLocalName = SVGElement::animatableAttributeForName(selectorAttributeName.localName());
            if (Attribute::nameMatchesFilter(candidateForCanonicalLocalName, selectorAttributeName.prefix(), selectorAttributeName.localName(), selectorAttributeName.namespaceURI()))
                return true;
        }
    }
    return false;
}

void SelectorCodeGenerator::generateSynchronizeAllAnimatedSVGAttribute(Assembler::RegisterID elementDataArraySizeAndFlags)
{
    // SVG attributes can be updated lazily depending on the flag AnimatedSVGAttributesAreDirty. We need to check
    // that first.
    Assembler::Jump animatedSVGAttributesNotDirty = m_assembler.branchTest32(Assembler::Zero, elementDataArraySizeAndFlags, Assembler::TrustedImm32(ElementData::animatedSVGAttributesAreDirtyFlag()));

    FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
    functionCall.setFunctionAddress(SVGElement::synchronizeAllAnimatedSVGAttribute);
    Assembler::RegisterID elementAddress = elementAddressRegister;
    functionCall.setOneArgument(elementAddress);
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

    if (canMatchAnimatableSVGAttribute(fragment))
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
        m_assembler.addPtr(Assembler::TrustedImm32(ShareableElementData::attributeArrayMemoryOffset()), elementDataAddress, attributeArrayPointer);
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

void SelectorCodeGenerator::generateElementAttributeMatching(Assembler::JumpList& failureCases, Assembler::RegisterID currentAttributeAddress, Assembler::RegisterID decIndexRegister, const AttributeMatchingInfo& attributeInfo)
{
    // Get the localName used for comparison. HTML elements use a lowercase local name known in selectors as canonicalLocalName.
    LocalRegister localNameToMatch(m_registerAllocator);

    // In general, canonicalLocalName and localName are the same. When they differ, we have to check if the node is HTML to know
    // which one to use.
    const CSSSelector& attributeSelector = attributeInfo.selector();
    const AtomicStringImpl* canonicalLocalName = attributeSelector.attributeCanonicalLocalName().impl();
    const AtomicStringImpl* localName = attributeSelector.attribute().localName().impl();
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

    {
        LocalRegister qualifiedNameImpl(m_registerAllocator);
        m_assembler.loadPtr(Assembler::Address(currentAttributeAddress, Attribute::nameMemoryOffset()), qualifiedNameImpl);

        bool shouldCheckNamespace = attributeSelector.attribute().prefix() != starAtom;
        if (shouldCheckNamespace) {
            Assembler::Jump nameDoesNotMatch = m_assembler.branchPtr(Assembler::NotEqual, Assembler::Address(qualifiedNameImpl, QualifiedName::QualifiedNameImpl::localNameMemoryOffset()), localNameToMatch);

            const AtomicStringImpl* namespaceURI = attributeSelector.attribute().namespaceURI().impl();
            if (namespaceURI) {
                LocalRegister namespaceToMatch(m_registerAllocator);
                m_assembler.move(Assembler::TrustedImmPtr(namespaceURI), namespaceToMatch);
                successCases.append(m_assembler.branchPtr(Assembler::Equal, Assembler::Address(qualifiedNameImpl, QualifiedName::QualifiedNameImpl::namespaceMemoryOffset()), namespaceToMatch));
            } else
                successCases.append(m_assembler.branchTestPtr(Assembler::Zero, Assembler::Address(qualifiedNameImpl, QualifiedName::QualifiedNameImpl::namespaceMemoryOffset())));
            nameDoesNotMatch.link(&m_assembler);
        } else
            successCases.append(m_assembler.branchPtr(Assembler::Equal, Assembler::Address(qualifiedNameImpl, QualifiedName::QualifiedNameImpl::localNameMemoryOffset()), localNameToMatch));
    }

    Assembler::Label loopReEntry(m_assembler.label());

    // If we reached the last element -> failure.
    failureCases.append(m_assembler.branchSub32(Assembler::Zero, Assembler::TrustedImm32(1), decIndexRegister));

    // Otherwise just loop over.
    m_assembler.addPtr(Assembler::TrustedImm32(sizeof(Attribute)), currentAttributeAddress);
    m_assembler.jump().linkTo(loopStart, &m_assembler);

    successCases.link(&m_assembler);

    if (attributeSelector.m_match != CSSSelector::Set) {
        // We make the assumption that name matching fails in most cases and we keep value matching outside
        // of the loop. We re-enter the loop if needed.
        // FIXME: exact case sensitive value matching is so simple that it should be done in the loop.
        Assembler::JumpList localFailureCases;
        generateElementAttributeValueMatching(localFailureCases, currentAttributeAddress, attributeInfo);
        localFailureCases.linkTo(loopReEntry, &m_assembler);
    }
}

enum CaseSensitivity {
    CaseSensitive,
    CaseInsensitive
};

template<CaseSensitivity caseSensitivity>
static bool attributeValueBeginsWith(const Attribute* attribute, AtomicStringImpl* expectedString)
{
    AtomicStringImpl& valueImpl = *attribute->value().impl();
    if (caseSensitivity == CaseSensitive)
        return valueImpl.startsWith(expectedString);
    return valueImpl.startsWith(expectedString, false);
}

template<CaseSensitivity caseSensitivity>
static bool attributeValueContains(const Attribute* attribute, AtomicStringImpl* expectedString)
{
    AtomicStringImpl& valueImpl = *attribute->value().impl();
    if (caseSensitivity == CaseSensitive)
        return valueImpl.find(expectedString) != notFound;
    return valueImpl.findIgnoringCase(expectedString) != notFound;
}

template<CaseSensitivity caseSensitivity>
static bool attributeValueEndsWith(const Attribute* attribute, AtomicStringImpl* expectedString)
{
    AtomicStringImpl& valueImpl = *attribute->value().impl();
    if (caseSensitivity == CaseSensitive)
        return valueImpl.endsWith(expectedString);
    return valueImpl.endsWith(expectedString, false);
}

template<CaseSensitivity caseSensitivity>
static bool attributeValueMatchHyphenRule(const Attribute* attribute, AtomicStringImpl* expectedString)
{
    AtomicStringImpl& valueImpl = *attribute->value().impl();
    if (valueImpl.length() < expectedString->length())
        return false;

    bool valueStartsWithExpectedString;
    if (caseSensitivity == CaseSensitive)
        valueStartsWithExpectedString = valueImpl.startsWith(expectedString);
    else
        valueStartsWithExpectedString = valueImpl.startsWith(expectedString, false);

    if (!valueStartsWithExpectedString)
        return false;

    return valueImpl.length() == expectedString->length() || valueImpl[expectedString->length()] == '-';
}

template<CaseSensitivity caseSensitivity>
static bool attributeValueSpaceSeparetedListContains(const Attribute* attribute, AtomicStringImpl* expectedString)
{
    AtomicStringImpl& value = *attribute->value().impl();

    unsigned startSearchAt = 0;
    while (true) {
        size_t foundPos;
        if (caseSensitivity == CaseSensitive)
            foundPos = value.find(expectedString, startSearchAt);
        else
            foundPos = value.findIgnoringCase(expectedString, startSearchAt);
        if (foundPos == notFound)
            return false;
        if (!foundPos || value[foundPos - 1] == ' ') {
            unsigned endStr = foundPos + expectedString->length();
            if (endStr == value.length() || value[endStr] == ' ')
                return true;
        }
        startSearchAt = foundPos + 1;
    }
    return false;
}

void SelectorCodeGenerator::generateElementAttributeValueMatching(Assembler::JumpList& failureCases, Assembler::RegisterID currentAttributeAddress, const AttributeMatchingInfo& attributeInfo)
{
    const CSSSelector& attributeSelector = attributeInfo.selector();
    const AtomicString& expectedValue = attributeSelector.value();
    ASSERT(!expectedValue.isNull());
    bool defaultToCaseSensitiveValueMatch = attributeInfo.canDefaultToCaseSensitiveValueMatch();

    switch (attributeSelector.m_match) {
    case CSSSelector::Begin:
        generateElementAttributeFunctionCallValueMatching(failureCases, currentAttributeAddress, expectedValue, defaultToCaseSensitiveValueMatch, attributeValueBeginsWith<CaseSensitive>, attributeValueBeginsWith<CaseInsensitive>);
        break;
    case CSSSelector::Contain:
        generateElementAttributeFunctionCallValueMatching(failureCases, currentAttributeAddress, expectedValue, defaultToCaseSensitiveValueMatch, attributeValueContains<CaseSensitive>, attributeValueContains<CaseInsensitive>);
        break;
    case CSSSelector::End:
        generateElementAttributeFunctionCallValueMatching(failureCases, currentAttributeAddress, expectedValue, defaultToCaseSensitiveValueMatch, attributeValueEndsWith<CaseSensitive>, attributeValueEndsWith<CaseInsensitive>);
        break;
    case CSSSelector::Exact:
        generateElementAttributeValueExactMatching(failureCases, currentAttributeAddress, expectedValue, defaultToCaseSensitiveValueMatch);
        break;
    case CSSSelector::Hyphen:
        generateElementAttributeFunctionCallValueMatching(failureCases, currentAttributeAddress, expectedValue, defaultToCaseSensitiveValueMatch, attributeValueMatchHyphenRule<CaseSensitive>, attributeValueMatchHyphenRule<CaseInsensitive>);
        break;
    case CSSSelector::List:
        generateElementAttributeFunctionCallValueMatching(failureCases, currentAttributeAddress, expectedValue, defaultToCaseSensitiveValueMatch, attributeValueSpaceSeparetedListContains<CaseSensitive>, attributeValueSpaceSeparetedListContains<CaseInsensitive>);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

static inline Assembler::Jump testIsHTMLClassOnDocument(Assembler::ResultCondition condition, Assembler& assembler, Assembler::RegisterID documentAddress)
{
    return assembler.branchTest32(condition, Assembler::Address(documentAddress, Document::documentClassesMemoryOffset()), Assembler::TrustedImm32(Document::isHTMLDocumentClassFlag()));
}

void SelectorCodeGenerator::generateElementAttributeValueExactMatching(Assembler::JumpList& failureCases, Assembler::RegisterID currentAttributeAddress, const AtomicString& expectedValue, bool canDefaultToCaseSensitiveValueMatch)
{
    LocalRegisterWithPreference expectedValueRegister(m_registerAllocator, JSC::GPRInfo::argumentGPR1);
    m_assembler.move(Assembler::TrustedImmPtr(expectedValue.impl()), expectedValueRegister);

    if (canDefaultToCaseSensitiveValueMatch)
        failureCases.append(m_assembler.branchPtr(Assembler::NotEqual, Assembler::Address(currentAttributeAddress, Attribute::valueMemoryOffset()), expectedValueRegister));
    else {
        Assembler::Jump skipCaseInsensitiveComparison = m_assembler.branchPtr(Assembler::Equal, Assembler::Address(currentAttributeAddress, Attribute::valueMemoryOffset()), expectedValueRegister);

        // If the element is an HTML element, in a HTML dcoument (not including XHTML), value matching is case insensitive.
        // Taking the contrapositive, if we find the element is not HTML or is not in a HTML document, the condition above
        // sould be sufficient and we can fail early.
        failureCases.append(testIsHTMLFlagOnNode(Assembler::Zero, m_assembler, elementAddressRegister));

        {
            LocalRegister document(m_registerAllocator);
            getDocument(m_assembler, elementAddressRegister, document);
            failureCases.append(testIsHTMLClassOnDocument(Assembler::Zero, m_assembler, document));
        }

        LocalRegister valueStringImpl(m_registerAllocator);
        m_assembler.loadPtr(Assembler::Address(currentAttributeAddress, Attribute::valueMemoryOffset()), valueStringImpl);

        FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
        functionCall.setFunctionAddress(WTF::equalIgnoringCaseNonNull);
        functionCall.setTwoArguments(valueStringImpl, expectedValueRegister);
        failureCases.append(functionCall.callAndBranchOnBooleanReturnValue(Assembler::Zero));

        skipCaseInsensitiveComparison.link(&m_assembler);
    }
}

void SelectorCodeGenerator::generateElementAttributeFunctionCallValueMatching(Assembler::JumpList& failureCases, Assembler::RegisterID currentAttributeAddress, const AtomicString& expectedValue, bool canDefaultToCaseSensitiveValueMatch, JSC::FunctionPtr caseSensitiveTest, JSC::FunctionPtr caseInsensitiveTest)
{
    LocalRegisterWithPreference expectedValueRegister(m_registerAllocator, JSC::GPRInfo::argumentGPR1);
    m_assembler.move(Assembler::TrustedImmPtr(expectedValue.impl()), expectedValueRegister);

    if (canDefaultToCaseSensitiveValueMatch) {
        FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
        functionCall.setFunctionAddress(caseSensitiveTest);
        functionCall.setTwoArguments(currentAttributeAddress, expectedValueRegister);
        failureCases.append(functionCall.callAndBranchOnBooleanReturnValue(Assembler::Zero));
    } else {
        Assembler::JumpList shouldUseCaseSensitiveComparison;
        shouldUseCaseSensitiveComparison.append(testIsHTMLFlagOnNode(Assembler::Zero, m_assembler, elementAddressRegister));
        {
            LocalRegister scratchRegister(m_registerAllocator);
            // scratchRegister = pointer to treeScope.
            m_assembler.loadPtr(Assembler::Address(elementAddressRegister, Node::treeScopeMemoryOffset()), scratchRegister);
            // scratchRegister = pointer to document.
            m_assembler.loadPtr(Assembler::Address(scratchRegister, TreeScope::documentScopeMemoryOffset()), scratchRegister);
            shouldUseCaseSensitiveComparison.append(testIsHTMLClassOnDocument(Assembler::Zero, m_assembler, scratchRegister));
        }

        {
            FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
            functionCall.setFunctionAddress(caseInsensitiveTest);
            functionCall.setTwoArguments(currentAttributeAddress, expectedValueRegister);
            failureCases.append(functionCall.callAndBranchOnBooleanReturnValue(Assembler::Zero));
        }

        Assembler::Jump skipCaseSensitiveCase = m_assembler.jump();

        {
            shouldUseCaseSensitiveComparison.link(&m_assembler);
            FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
            functionCall.setFunctionAddress(caseSensitiveTest);
            functionCall.setTwoArguments(currentAttributeAddress, expectedValueRegister);
            failureCases.append(functionCall.callAndBranchOnBooleanReturnValue(Assembler::Zero));
        }

        skipCaseSensitiveCase.link(&m_assembler);
    }
}

void SelectorCodeGenerator::generateElementFunctionCallTest(Assembler::JumpList& failureCases, JSC::FunctionPtr testFunction)
{
    Assembler::RegisterID elementAddress = elementAddressRegister;
    FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
    functionCall.setFunctionAddress(testFunction);
    functionCall.setOneArgument(elementAddress);
    failureCases.append(functionCall.callAndBranchOnBooleanReturnValue(Assembler::Zero));
}

static void setFirstChildState(Element* element)
{
    if (RenderStyle* style = element->renderStyle())
        style->setFirstChildState();
}

static bool elementIsActive(Element* element)
{
    return element->active() || InspectorInstrumentation::forcePseudoState(element, CSSSelector::PseudoClassActive);
}

static bool elementIsActiveForStyleResolution(Element* element, const CheckingContext* checkingContext)
{
    if (checkingContext->resolvingMode == SelectorChecker::Mode::ResolvingStyle)
        element->setChildrenAffectedByActive();
    return element->active() || InspectorInstrumentation::forcePseudoState(element, CSSSelector::PseudoClassActive);
}

void SelectorCodeGenerator::generateElementIsActive(Assembler::JumpList& failureCases, const SelectorFragment& fragment)
{
    generateSpecialFailureInQuirksModeForActiveAndHoverIfNeeded(failureCases, fragment);
    if (m_selectorContext == SelectorContext::QuerySelector) {
        FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
        functionCall.setFunctionAddress(elementIsActive);
        functionCall.setOneArgument(elementAddressRegister);
        failureCases.append(functionCall.callAndBranchOnBooleanReturnValue(Assembler::Zero));
    } else {
        if (shouldUseRenderStyleFromCheckingContext(fragment)) {
            LocalRegister checkingContext(m_registerAllocator);
            Assembler::Jump notResolvingStyle = jumpIfNotResolvingStyle(checkingContext);
            addFlagsToElementStyleFromContext(checkingContext, RenderStyle::NonInheritedFlags::flagIsaffectedByActive());
            notResolvingStyle.link(&m_assembler);

            FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
            functionCall.setFunctionAddress(elementIsActive);
            functionCall.setOneArgument(elementAddressRegister);
            failureCases.append(functionCall.callAndBranchOnBooleanReturnValue(Assembler::Zero));
        } else {
            unsigned offsetToCheckingContext = m_stackAllocator.offsetToStackReference(m_checkingContextStackReference);
            Assembler::RegisterID checkingContext = m_registerAllocator.allocateRegisterWithPreference(JSC::GPRInfo::argumentGPR1);
            m_assembler.loadPtr(Assembler::Address(Assembler::stackPointerRegister, offsetToCheckingContext), checkingContext);
            m_registerAllocator.deallocateRegister(checkingContext);

            FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
            functionCall.setFunctionAddress(elementIsActiveForStyleResolution);
            functionCall.setTwoArguments(elementAddressRegister, checkingContext);
            failureCases.append(functionCall.callAndBranchOnBooleanReturnValue(Assembler::Zero));
        }
    }
}

void SelectorCodeGenerator::generateElementIsFirstChild(Assembler::JumpList& failureCases, const SelectorFragment& fragment)
{
    if (m_selectorContext == SelectorContext::QuerySelector) {
        Assembler::JumpList successCase = jumpIfNoPreviousAdjacentElement();
        failureCases.append(m_assembler.jump());
        successCase.link(&m_assembler);
        LocalRegister parent(m_registerAllocator);
        generateWalkToParentElement(failureCases, parent);
        return;
    }

    Assembler::RegisterID parentElement = m_registerAllocator.allocateRegister();
    generateWalkToParentElement(failureCases, parentElement);

    // Zero in isFirstChildRegister is the success case. The register is set to non-zero if a sibling if found.
    LocalRegister isFirstChildRegister(m_registerAllocator);
    m_assembler.move(Assembler::TrustedImm32(0), isFirstChildRegister);

    {
        Assembler::JumpList successCase = jumpIfNoPreviousAdjacentElement();

        // If there was a sibling element, the element was not the first child -> failure case.
        m_assembler.move(Assembler::TrustedImm32(1), isFirstChildRegister);

        successCase.link(&m_assembler);
    }

    LocalRegister checkingContext(m_registerAllocator);
    Assembler::Jump notResolvingStyle = jumpIfNotResolvingStyle(checkingContext);

    setNodeFlag(m_assembler, parentElement, Node::flagChildrenAffectedByFirstChildRulesFlag());
    m_registerAllocator.deallocateRegister(parentElement);

    // The parent marking is unconditional. If the matching is not a success, we can now fail.
    // Otherwise we need to apply setFirstChildState() on the RenderStyle.
    failureCases.append(m_assembler.branchTest32(Assembler::NonZero, isFirstChildRegister));

    if (shouldUseRenderStyleFromCheckingContext(fragment))
        addFlagsToElementStyleFromContext(checkingContext, RenderStyle::NonInheritedFlags::setFirstChildStateFlags());
    else {
        FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
        functionCall.setFunctionAddress(setFirstChildState);
        Assembler::RegisterID elementAddress = elementAddressRegister;
        functionCall.setOneArgument(elementAddress);
        functionCall.call();
    }

    notResolvingStyle.link(&m_assembler);
    failureCases.append(m_assembler.branchTest32(Assembler::NonZero, isFirstChildRegister));
}

static bool elementIsHovered(Element* element)
{
    return element->hovered() || InspectorInstrumentation::forcePseudoState(element, CSSSelector::PseudoClassHover);
}

static bool elementIsHoveredForStyleResolution(Element* element, const CheckingContext* checkingContext)
{
    if (checkingContext->resolvingMode == SelectorChecker::Mode::ResolvingStyle)
        element->setChildrenAffectedByHover();
    return element->hovered() || InspectorInstrumentation::forcePseudoState(element, CSSSelector::PseudoClassHover);
}

void SelectorCodeGenerator::generateElementIsHovered(Assembler::JumpList& failureCases, const SelectorFragment& fragment)
{
    generateSpecialFailureInQuirksModeForActiveAndHoverIfNeeded(failureCases, fragment);
    if (m_selectorContext == SelectorContext::QuerySelector) {
        FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
        functionCall.setFunctionAddress(elementIsHovered);
        functionCall.setOneArgument(elementAddressRegister);
        failureCases.append(functionCall.callAndBranchOnBooleanReturnValue(Assembler::Zero));
    } else {
        if (shouldUseRenderStyleFromCheckingContext(fragment)) {
            LocalRegisterWithPreference checkingContext(m_registerAllocator, JSC::GPRInfo::argumentGPR1);
            Assembler::Jump notResolvingStyle = jumpIfNotResolvingStyle(checkingContext);
            addFlagsToElementStyleFromContext(checkingContext, RenderStyle::NonInheritedFlags::flagIsaffectedByHover());
            notResolvingStyle.link(&m_assembler);

            FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
            functionCall.setFunctionAddress(elementIsHovered);
            functionCall.setOneArgument(elementAddressRegister);
            failureCases.append(functionCall.callAndBranchOnBooleanReturnValue(Assembler::Zero));
        } else {
            unsigned offsetToCheckingContext = m_stackAllocator.offsetToStackReference(m_checkingContextStackReference);
            Assembler::RegisterID checkingContext = m_registerAllocator.allocateRegisterWithPreference(JSC::GPRInfo::argumentGPR1);
            m_assembler.loadPtr(Assembler::Address(Assembler::stackPointerRegister, offsetToCheckingContext), checkingContext);
            m_registerAllocator.deallocateRegister(checkingContext);

            FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
            functionCall.setFunctionAddress(elementIsHoveredForStyleResolution);
            functionCall.setTwoArguments(elementAddressRegister, checkingContext);
            failureCases.append(functionCall.callAndBranchOnBooleanReturnValue(Assembler::Zero));
        }
    }
}

void SelectorCodeGenerator::generateElementIsInLanguage(Assembler::JumpList& failureCases, const AtomicString& langFilter)
{
    LocalRegisterWithPreference langFilterRegister(m_registerAllocator, JSC::GPRInfo::argumentGPR1);
    m_assembler.move(Assembler::TrustedImmPtr(langFilter.impl()), langFilterRegister);

    Assembler::RegisterID elementAddress = elementAddressRegister;
    FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
    functionCall.setFunctionAddress(matchesLangPseudoClass);
    functionCall.setTwoArguments(elementAddress, langFilterRegister);
    failureCases.append(functionCall.callAndBranchOnBooleanReturnValue(Assembler::Zero));
}

static void setLastChildState(Element* element)
{
    if (RenderStyle* style = element->renderStyle())
        style->setLastChildState();
}

void SelectorCodeGenerator::generateElementIsLastChild(Assembler::JumpList& failureCases, const SelectorFragment& fragment)
{
    if (m_selectorContext == SelectorContext::QuerySelector) {
        Assembler::JumpList successCase = jumpIfNoNextAdjacentElement();
        failureCases.append(m_assembler.jump());

        successCase.link(&m_assembler);
        LocalRegister parent(m_registerAllocator);
        generateWalkToParentElement(failureCases, parent);

        failureCases.append(m_assembler.branchTest32(Assembler::Zero, Assembler::Address(parent, Node::nodeFlagsMemoryOffset()), Assembler::TrustedImm32(Node::flagIsParsingChildrenFinished())));

        return;
    }

    Assembler::RegisterID parentElement = m_registerAllocator.allocateRegister();
    generateWalkToParentElement(failureCases, parentElement);

    // Zero in isLastChildRegister is the success case. The register is set to non-zero if a sibling if found.
    LocalRegister isLastChildRegister(m_registerAllocator);
    m_assembler.move(Assembler::TrustedImm32(0), isLastChildRegister);

    {
        Assembler::Jump notFinishedParsingChildren = m_assembler.branchTest32(Assembler::Zero, Assembler::Address(parentElement, Node::nodeFlagsMemoryOffset()), Assembler::TrustedImm32(Node::flagIsParsingChildrenFinished()));

        Assembler::JumpList successCase = jumpIfNoNextAdjacentElement();

        notFinishedParsingChildren.link(&m_assembler);
        m_assembler.move(Assembler::TrustedImm32(1), isLastChildRegister);

        successCase.link(&m_assembler);
    }

    LocalRegister checkingContext(m_registerAllocator);
    Assembler::Jump notResolvingStyle = jumpIfNotResolvingStyle(checkingContext);

    setNodeFlag(m_assembler, parentElement, Node::flagChildrenAffectedByLastChildRulesFlag());
    m_registerAllocator.deallocateRegister(parentElement);

    // The parent marking is unconditional. If the matching is not a success, we can now fail.
    // Otherwise we need to apply setLastChildState() on the RenderStyle.
    failureCases.append(m_assembler.branchTest32(Assembler::NonZero, isLastChildRegister));

    if (shouldUseRenderStyleFromCheckingContext(fragment))
        addFlagsToElementStyleFromContext(checkingContext, RenderStyle::NonInheritedFlags::setLastChildStateFlags());
    else {
        FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
        functionCall.setFunctionAddress(setLastChildState);
        Assembler::RegisterID elementAddress = elementAddressRegister;
        functionCall.setOneArgument(elementAddress);
        functionCall.call();
    }

    notResolvingStyle.link(&m_assembler);
    failureCases.append(m_assembler.branchTest32(Assembler::NonZero, isLastChildRegister));
}

static void setOnlyChildState(Element* element)
{
    if (RenderStyle* style = element->renderStyle()) {
        style->setFirstChildState();
        style->setLastChildState();
    }
}

void SelectorCodeGenerator::generateElementIsOnlyChild(Assembler::JumpList& failureCases, const SelectorFragment& fragment)
{
    // Is Only child is pretty much a combination of isFirstChild + isLastChild. The main difference is that tree marking is combined.
    if (m_selectorContext == SelectorContext::QuerySelector) {
        Assembler::JumpList previousSuccessCase = jumpIfNoPreviousAdjacentElement();
        failureCases.append(m_assembler.jump());
        previousSuccessCase.link(&m_assembler);

        Assembler::JumpList nextSuccessCase = jumpIfNoNextAdjacentElement();
        failureCases.append(m_assembler.jump());
        nextSuccessCase.link(&m_assembler);

        LocalRegister parent(m_registerAllocator);
        generateWalkToParentElement(failureCases, parent);

        failureCases.append(m_assembler.branchTest32(Assembler::Zero, Assembler::Address(parent, Node::nodeFlagsMemoryOffset()), Assembler::TrustedImm32(Node::flagIsParsingChildrenFinished())));

        return;
    }

    Assembler::RegisterID parentElement = m_registerAllocator.allocateRegister();
    generateWalkToParentElement(failureCases, parentElement);

    // Zero in isOnlyChildRegister is the success case. The register is set to non-zero if a sibling if found.
    LocalRegister isOnlyChildRegister(m_registerAllocator);
    m_assembler.move(Assembler::TrustedImm32(0), isOnlyChildRegister);

    {
        Assembler::JumpList localFailureCases;
        {
            Assembler::JumpList successCase = jumpIfNoPreviousAdjacentElement();
            localFailureCases.append(m_assembler.jump());
            successCase.link(&m_assembler);
        }
        localFailureCases.append(m_assembler.branchTest32(Assembler::Zero, Assembler::Address(parentElement, Node::nodeFlagsMemoryOffset()), Assembler::TrustedImm32(Node::flagIsParsingChildrenFinished())));
        Assembler::JumpList successCase = jumpIfNoNextAdjacentElement();

        localFailureCases.link(&m_assembler);
        m_assembler.move(Assembler::TrustedImm32(1), isOnlyChildRegister);

        successCase.link(&m_assembler);
    }

    LocalRegister checkingContext(m_registerAllocator);
    Assembler::Jump notResolvingStyle = jumpIfNotResolvingStyle(checkingContext);

    setNodeFlag(m_assembler, parentElement, Node::flagChildrenAffectedByFirstChildRulesFlag() | Node::flagChildrenAffectedByLastChildRulesFlag());
    m_registerAllocator.deallocateRegister(parentElement);

    // The parent marking is unconditional. If the matching is not a success, we can now fail.
    // Otherwise we need to apply setLastChildState() on the RenderStyle.
    failureCases.append(m_assembler.branchTest32(Assembler::NonZero, isOnlyChildRegister));

    if (shouldUseRenderStyleFromCheckingContext(fragment))
        addFlagsToElementStyleFromContext(checkingContext, RenderStyle::NonInheritedFlags::setFirstChildStateFlags() | RenderStyle::NonInheritedFlags::setLastChildStateFlags());
    else {
        FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
        functionCall.setFunctionAddress(setOnlyChildState);
        Assembler::RegisterID elementAddress = elementAddressRegister;
        functionCall.setOneArgument(elementAddress);
        functionCall.call();
    }

    notResolvingStyle.link(&m_assembler);
    failureCases.append(m_assembler.branchTest32(Assembler::NonZero, isOnlyChildRegister));
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

void SelectorCodeGenerator::generateElementIsLink(Assembler::JumpList& failureCases)
{
    failureCases.append(m_assembler.branchTest32(Assembler::Zero, Assembler::Address(elementAddressRegister, Node::nodeFlagsMemoryOffset()), Assembler::TrustedImm32(Node::flagIsLink())));
}

static void setElementChildIndex(Element* element, int index)
{
    element->setChildIndex(index);
}

static void setElementChildIndexAndUpdateStyle(Element* element, int index)
{
    element->setChildIndex(index);
    if (RenderStyle* childStyle = element->renderStyle())
        childStyle->setUnique();
}

void SelectorCodeGenerator::generateElementIsNthChild(Assembler::JumpList& failureCases, const SelectorFragment& fragment)
{
    Assembler::RegisterID parentElement = m_registerAllocator.allocateRegister();
    generateWalkToParentElement(failureCases, parentElement);

    Vector<std::pair<int, int>, 32> validSubsetFilters;
    validSubsetFilters.reserveInitialCapacity(fragment.nthChildFilters.size());
    for (const auto& slot : fragment.nthChildFilters) {
        int a = slot.first;
        int b = slot.second;

        // Anything modulo 1 is zero. Unless b restricts the range, this does not filter anything out.
        if (a == 1 && (!b || (b == 1)))
            continue;
        validSubsetFilters.uncheckedAppend(slot);
    }
    if (validSubsetFilters.isEmpty()) {
        m_registerAllocator.deallocateRegister(parentElement);
        return;
    }
    if (m_selectorContext == SelectorContext::QuerySelector)
        m_registerAllocator.deallocateRegister(parentElement);

    // Setup the counter at 1.
    LocalRegisterWithPreference elementCounter(m_registerAllocator, JSC::GPRInfo::argumentGPR1);
    m_assembler.move(Assembler::TrustedImm32(1), elementCounter);

    // Loop over the previous adjacent elements and increment the counter.
    {
        LocalRegister previousSibling(m_registerAllocator);
        m_assembler.move(elementAddressRegister, previousSibling);

        // Getting the child index is very efficient when it works. When there is no child index,
        // querying at every iteration is very inefficient. We solve this by only testing the child
        // index on the first direct adjacent.
        Assembler::JumpList noMoreSiblingsCases;

        Assembler::JumpList noCachedChildIndexCases;
        generateWalkToPreviousAdjacentElement(noMoreSiblingsCases, previousSibling);
        noCachedChildIndexCases.append(m_assembler.branchTest32(Assembler::Zero, Assembler::Address(previousSibling, Node::nodeFlagsMemoryOffset()), Assembler::TrustedImm32(Node::flagHasRareData())));
        {
            LocalRegister elementRareData(m_registerAllocator);
            m_assembler.loadPtr(Assembler::Address(previousSibling, Node::rareDataMemoryOffset()), elementRareData);
            LocalRegister cachedChildIndex(m_registerAllocator);
            m_assembler.load16(Assembler::Address(elementRareData, ElementRareData::childIndexMemoryOffset()), cachedChildIndex);
            noCachedChildIndexCases.append(m_assembler.branchTest32(Assembler::Zero, cachedChildIndex));
            m_assembler.add32(cachedChildIndex, elementCounter);
            noMoreSiblingsCases.append(m_assembler.jump());
        }
        noCachedChildIndexCases.link(&m_assembler);
        m_assembler.add32(Assembler::TrustedImm32(1), elementCounter);

        Assembler::Label loopStart = m_assembler.label();
        generateWalkToPreviousAdjacentElement(noMoreSiblingsCases, previousSibling);
        m_assembler.add32(Assembler::TrustedImm32(1), elementCounter);
        m_assembler.jump().linkTo(loopStart, &m_assembler);
        noMoreSiblingsCases.link(&m_assembler);
    }

    // Tree marking when doing style resolution.
    if (m_selectorContext != SelectorContext::QuerySelector) {
        LocalRegister checkingContext(m_registerAllocator);
        Assembler::Jump notResolvingStyle = jumpIfNotResolvingStyle(checkingContext);

        m_registerAllocator.deallocateRegister(parentElement);
        FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
        functionCall.setFunctionAddress(Element::setChildrenAffectedByForwardPositionalRules);
        functionCall.setOneArgument(parentElement);
        functionCall.call();

        if (shouldUseRenderStyleFromCheckingContext(fragment)) {
            addFlagsToElementStyleFromContext(checkingContext, RenderStyle::NonInheritedFlags::flagIsUnique());

            Assembler::RegisterID elementAddress = elementAddressRegister;
            FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
            functionCall.setFunctionAddress(setElementChildIndex);
            functionCall.setTwoArguments(elementAddress, elementCounter);
            functionCall.call();
        } else {
            Assembler::RegisterID elementAddress = elementAddressRegister;
            FunctionCall functionCall(m_assembler, m_registerAllocator, m_stackAllocator, m_functionCalls);
            functionCall.setFunctionAddress(setElementChildIndexAndUpdateStyle);
            functionCall.setTwoArguments(elementAddress, elementCounter);
            functionCall.call();
        }

        notResolvingStyle.link(&m_assembler);
    }

    // Test every the nth-child filter.
    for (const auto& slot : validSubsetFilters) {
        int a = slot.first;
        int b = slot.second;

        if (!a)
            failureCases.append(m_assembler.branch32(Assembler::NotEqual, Assembler::TrustedImm32(b), elementCounter));
        else if (a > 0) {
            if (a == 2 && b == 1) {
                // This is the common case 2n+1 (or "odd"), we can test for odd values without doing the arithmetic.
                failureCases.append(m_assembler.branchTest32(Assembler::Zero, elementCounter, Assembler::TrustedImm32(1)));
            } else {
                if (b)
                    failureCases.append(m_assembler.branchSub32(Assembler::Signed, Assembler::TrustedImm32(b), elementCounter));
                moduloIsZero(failureCases, elementCounter, a);
            }
        } else {
            LocalRegister bRegister(m_registerAllocator);
            m_assembler.move(Assembler::TrustedImm32(b), bRegister);

            failureCases.append(m_assembler.branchSub32(Assembler::Signed, elementCounter, bRegister));
            moduloIsZero(failureCases, bRegister, a);
        }
    }
}

void SelectorCodeGenerator::generateElementMatchesNotPseudoClass(Assembler::JumpList& failureCases, const SelectorFragment& fragment)
{
    for (const auto& subFragment : fragment.notFilters) {
        Assembler::JumpList localFailureCases;
        generateElementMatching(localFailureCases, localFailureCases, subFragment);
        // Since this is a not pseudo class filter, reaching here is a failure.
        failureCases.append(m_assembler.jump());
        localFailureCases.link(&m_assembler);
    }
}

void SelectorCodeGenerator::generateElementMatchesAnyPseudoClass(Assembler::JumpList& failureCases, const SelectorFragment& fragment)
{
    for (const auto& subFragments : fragment.anyFilters) {
        RELEASE_ASSERT(!subFragments.isEmpty());

        // Don't handle the last fragment in this loop.
        Assembler::JumpList successCases;
        for (unsigned i = 0; i < subFragments.size() - 1; ++i) {
            Assembler::JumpList localFailureCases;
            generateElementMatching(localFailureCases, localFailureCases, subFragments[i]);
            successCases.append(m_assembler.jump());
            localFailureCases.link(&m_assembler);
        }

        // At the last fragment, optimize the failure jump to jump to the non-local failure directly.
        generateElementMatching(failureCases, failureCases, subFragments.last());
        successCases.link(&m_assembler);
    }
}

void SelectorCodeGenerator::generateElementIsRoot(Assembler::JumpList& failureCases)
{
    LocalRegister document(m_registerAllocator);
    getDocument(m_assembler, elementAddressRegister, document);
    failureCases.append(m_assembler.branchPtr(Assembler::NotEqual, Assembler::Address(document, Document::documentElementMemoryOffset()), elementAddressRegister));
}

void SelectorCodeGenerator::generateElementIsTarget(Assembler::JumpList& failureCases)
{
    LocalRegister document(m_registerAllocator);
    getDocument(m_assembler, elementAddressRegister, document);
    failureCases.append(m_assembler.branchPtr(Assembler::NotEqual, Assembler::Address(document, Document::cssTargetMemoryOffset()), elementAddressRegister));
}

}; // namespace SelectorCompiler.
}; // namespace WebCore.

#endif // ENABLE(CSS_SELECTOR_JIT)
