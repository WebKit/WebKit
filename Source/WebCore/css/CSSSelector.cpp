/*
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 *               2001 Andreas Schlapbach (schlpbch@iam.unibe.ch)
 *               2001-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2007, 2008, 2009, 2010, 2013, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "CSSSelector.h"

#include "CSSMarkup.h"
#include "CSSSelectorList.h"
#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "SelectorPseudoTypeMap.h"
#include <wtf/Assertions.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

using namespace HTMLNames;

struct SameSizeAsCSSSelector {
    unsigned flags;
    void* unionPointer;
};

static_assert(CSSSelector::RelationType::Subselector == 0, "Subselector must be 0 for consumeCombinator.");
static_assert(sizeof(CSSSelector) == sizeof(SameSizeAsCSSSelector), "CSSSelector should remain small.");

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSSelectorRareData);

CSSSelector::CSSSelector(const QualifiedName& tagQName, bool tagIsForNamespaceRule)
    : m_relation(DescendantSpace)
    , m_match(Tag)
    , m_pseudoType(0)
    , m_isLastInSelectorList(false)
    , m_isLastInTagHistory(true)
    , m_hasRareData(false)
    , m_hasNameWithCase(false)
    , m_isForPage(false)
    , m_tagIsForNamespaceRule(tagIsForNamespaceRule)
    , m_caseInsensitiveAttributeValueMatching(false)
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    , m_destructorHasBeenCalled(false)
#endif
{
    const AtomString& tagLocalName = tagQName.localName();
    const AtomString tagLocalNameASCIILowercase = tagLocalName.convertToASCIILowercase();

    if (tagLocalName == tagLocalNameASCIILowercase) {
        m_data.m_tagQName = tagQName.impl();
        m_data.m_tagQName->ref();
    } else {
        m_data.m_nameWithCase = adoptRef(new NameWithCase(tagQName, tagLocalNameASCIILowercase)).leakRef();
        m_hasNameWithCase = true;
    }
}

void CSSSelector::createRareData()
{
    ASSERT(match() != Tag);
    ASSERT(!m_hasNameWithCase);
    if (m_hasRareData)
        return;
    // Move the value to the rare data stucture.
    AtomString value { adoptRef(m_data.m_value) };
    m_data.m_rareData = &RareData::create(WTFMove(value)).leakRef();
    m_hasRareData = true;
}

static unsigned simpleSelectorSpecificityInternal(const CSSSelector& simpleSelector);

static unsigned selectorSpecificity(const CSSSelector& firstSimpleSelector)
{
    unsigned total = 0;
    for (const CSSSelector* selector = &firstSimpleSelector; selector; selector = selector->tagHistory())
        total = CSSSelector::addSpecificities(total, simpleSelectorSpecificityInternal(*selector));
    return total;
}

static unsigned maxSpecificity(const CSSSelectorList& selectorList)
{
    unsigned maxSpecificity = 0;
    for (const CSSSelector* subSelector = selectorList.first(); subSelector; subSelector = CSSSelectorList::next(subSelector))
        maxSpecificity = std::max(maxSpecificity, selectorSpecificity(*subSelector));
    return maxSpecificity;
}

static unsigned simpleSelectorSpecificityInternal(const CSSSelector& simpleSelector)
{
    ASSERT_WITH_MESSAGE(!simpleSelector.isForPage(), "At the time of this writing, page selectors are not treated as real selectors that are matched. The value computed here only account for real selectors.");

    switch (simpleSelector.match()) {
    case CSSSelector::Id:
        return static_cast<unsigned>(SelectorSpecificityIncrement::ClassA);

    case CSSSelector::PagePseudoClass:
        break;
    case CSSSelector::PseudoClass:
        switch (simpleSelector.pseudoClassType()) {
        case CSSSelector::PseudoClassIs:
        case CSSSelector::PseudoClassMatches:
        case CSSSelector::PseudoClassNot:
            return maxSpecificity(*simpleSelector.selectorList());
        case CSSSelector::PseudoClassWhere:
            return 0;
        case CSSSelector::PseudoClassNthChild:
        case CSSSelector::PseudoClassNthLastChild:
            return CSSSelector::addSpecificities(static_cast<unsigned>(SelectorSpecificityIncrement::ClassB), simpleSelector.selectorList() ? maxSpecificity(*simpleSelector.selectorList()) : 0);
        default:
            break;
        }
        return static_cast<unsigned>(SelectorSpecificityIncrement::ClassB);
    case CSSSelector::Exact:
    case CSSSelector::Class:
    case CSSSelector::Set:
    case CSSSelector::List:
    case CSSSelector::Hyphen:
    case CSSSelector::Contain:
    case CSSSelector::Begin:
    case CSSSelector::End:
        return static_cast<unsigned>(SelectorSpecificityIncrement::ClassB);
    case CSSSelector::Tag:
        return (simpleSelector.tagQName().localName() != starAtom()) ? static_cast<unsigned>(SelectorSpecificityIncrement::ClassC) : 0;
    case CSSSelector::PseudoElement:
        return static_cast<unsigned>(SelectorSpecificityIncrement::ClassC);
    case CSSSelector::Unknown:
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

unsigned CSSSelector::simpleSelectorSpecificity() const
{
    return simpleSelectorSpecificityInternal(*this);
}

unsigned CSSSelector::computeSpecificity() const
{
    return selectorSpecificity(*this);
}

unsigned CSSSelector::addSpecificities(unsigned a, unsigned b)
{
    unsigned total = a;

    unsigned newIdValue = (b & idMask);
    if (((total & idMask) + newIdValue) & ~idMask)
        total |= idMask;
    else
        total += newIdValue;

    unsigned newClassValue = (b & classMask);
    if (((total & classMask) + newClassValue) & ~classMask)
        total |= classMask;
    else
        total += newClassValue;

    unsigned newElementValue = (b & elementMask);
    if (((total & elementMask) + newElementValue) & ~elementMask)
        total |= elementMask;
    else
        total += newElementValue;

    return total;
}

unsigned CSSSelector::specificityForPage() const
{
    ASSERT(isForPage());

    // See http://dev.w3.org/csswg/css3-page/#cascading-and-page-context
    unsigned s = 0;

    for (const CSSSelector* component = this; component; component = component->tagHistory()) {
        switch (component->match()) {
        case Tag:
            s += tagQName().localName() == starAtom() ? 0 : 4;
            break;
        case PagePseudoClass:
            switch (component->pagePseudoClassType()) {
            case PagePseudoClassFirst:
                s += 2;
                break;
            case PagePseudoClassLeft:
            case PagePseudoClassRight:
                s += 1;
                break;
            }
            break;
        default:
            break;
        }
    }
    return s;
}

PseudoId CSSSelector::pseudoId(PseudoElementType type)
{
    switch (type) {
    case PseudoElementFirstLine:
        return PseudoId::FirstLine;
    case PseudoElementFirstLetter:
        return PseudoId::FirstLetter;
    case PseudoElementSelection:
        return PseudoId::Selection;
    case PseudoElementHighlight:
        return PseudoId::Highlight;
    case PseudoElementMarker:
        return PseudoId::Marker;
    case PseudoElementBefore:
        return PseudoId::Before;
    case PseudoElementAfter:
        return PseudoId::After;
    case PseudoElementScrollbar:
        return PseudoId::Scrollbar;
    case PseudoElementScrollbarButton:
        return PseudoId::ScrollbarButton;
    case PseudoElementScrollbarCorner:
        return PseudoId::ScrollbarCorner;
    case PseudoElementScrollbarThumb:
        return PseudoId::ScrollbarThumb;
    case PseudoElementScrollbarTrack:
        return PseudoId::ScrollbarTrack;
    case PseudoElementScrollbarTrackPiece:
        return PseudoId::ScrollbarTrackPiece;
    case PseudoElementResizer:
        return PseudoId::Resizer;
#if ENABLE(VIDEO)
    case PseudoElementCue:
#endif
    case PseudoElementSlotted:
    case PseudoElementPart:
    case PseudoElementUnknown:
    case PseudoElementWebKitCustom:
    case PseudoElementWebKitCustomLegacyPrefixed:
        return PseudoId::None;
    }

    ASSERT_NOT_REACHED();
    return PseudoId::None;
}

CSSSelector::PseudoElementType CSSSelector::parsePseudoElementType(StringView name)
{
    if (name.isNull())
        return PseudoElementUnknown;

    auto type = parsePseudoElementString(name);
    if (type == PseudoElementUnknown) {
        if (name.startsWith("-webkit-"))
            type = PseudoElementWebKitCustom;
    }

    if (type == PseudoElementHighlight && !RuntimeEnabledFeatures::sharedFeatures().highlightAPIEnabled())
        return PseudoElementUnknown;

    if (type == PseudoElementPart && !RuntimeEnabledFeatures::sharedFeatures().cssShadowPartsEnabled())
        return PseudoElementUnknown;

    return type;
}

bool CSSSelector::operator==(const CSSSelector& other) const
{
    const CSSSelector* sel1 = this;
    const CSSSelector* sel2 = &other;

    while (sel1 && sel2) {
        if (sel1->attribute() != sel2->attribute()
            || sel1->relation() != sel2->relation()
            || sel1->match() != sel2->match()
            || sel1->value() != sel2->value()
            || sel1->m_pseudoType != sel2->m_pseudoType
            || sel1->argument() != sel2->argument()) {
            return false;
        }
        if (sel1->match() == Tag) {
            if (sel1->tagQName() != sel2->tagQName())
                return false;
        }
        sel1 = sel1->tagHistory();
        sel2 = sel2->tagHistory();
    }

    if (sel1 || sel2)
        return false;

    return true;
}

static void appendPseudoClassFunctionTail(StringBuilder& builder, const CSSSelector* selector)
{
    switch (selector->pseudoClassType()) {
#if ENABLE(CSS_SELECTORS_LEVEL4)
    case CSSSelector::PseudoClassDir:
#endif
    case CSSSelector::PseudoClassLang:
    case CSSSelector::PseudoClassNthChild:
    case CSSSelector::PseudoClassNthLastChild:
    case CSSSelector::PseudoClassNthOfType:
    case CSSSelector::PseudoClassNthLastOfType:
#if ENABLE(CSS_SELECTORS_LEVEL4)
    case CSSSelector::PseudoClassRole:
#endif
        builder.append(selector->argument());
        builder.append(')');
        break;
    default:
        break;
    }

}

static void appendLangArgumentList(StringBuilder& builder, const Vector<AtomString>& argumentList)
{
    unsigned argumentListSize = argumentList.size();
    for (unsigned i = 0; i < argumentListSize; ++i) {
        builder.append('"');
        builder.append(argumentList[i]);
        builder.append('"');
        if (i != argumentListSize - 1)
            builder.appendLiteral(", ");
    }
}

// http://dev.w3.org/csswg/css-syntax/#serializing-anb
static void outputNthChildAnPlusB(const CSSSelector& selector, StringBuilder& builder)
{
    auto outputFirstTerm = [&builder] (int a) {
        switch (a) {
        case 1:
            break;
        case -1:
            builder.append('-');
            break;
        default:
            builder.append(a);
        }
    };

    if (selector.argument() == nullAtom())
        return;

    int a = selector.nthA();
    int b = selector.nthB();
    if (a == 0 && b == 0)
        builder.append('0');
    else if (a == 0)
        builder.append(b);
    else if (b == 0) {
        outputFirstTerm(a);
        builder.append('n');
    } else if (b < 0) {
        outputFirstTerm(a);
        builder.append('n', b);
    } else {
        outputFirstTerm(a);
        builder.append("n+", b);
    }
}

String CSSSelector::selectorText(const String& rightSide) const
{
    StringBuilder builder;

    if (match() == CSSSelector::Tag && !m_tagIsForNamespaceRule) {
        if (tagQName().prefix().isNull())
            builder.append(tagQName().localName());
        else {
            builder.append(tagQName().prefix().string());
            builder.append('|');
            builder.append(tagQName().localName());
        }
    }

    const CSSSelector* cs = this;
    while (true) {
        if (cs->match() == CSSSelector::Id) {
            builder.append('#');
            serializeIdentifier(cs->serializingValue(), builder);
        } else if (cs->match() == CSSSelector::Class) {
            builder.append('.');
            serializeIdentifier(cs->serializingValue(), builder);
        } else if (cs->match() == CSSSelector::PseudoClass) {
            switch (cs->pseudoClassType()) {
#if ENABLE(FULLSCREEN_API)
            case CSSSelector::PseudoClassAnimatingFullScreenTransition:
                builder.appendLiteral(":-webkit-animating-full-screen-transition");
                break;
#endif
            case CSSSelector::PseudoClassAny: {
                builder.appendLiteral(":-webkit-any(");
                cs->selectorList()->buildSelectorsText(builder);
                builder.append(')');
                break;
            }
            case CSSSelector::PseudoClassAnyLink:
                builder.appendLiteral(":any-link");
                break;
            case CSSSelector::PseudoClassAnyLinkDeprecated:
                builder.appendLiteral(":-webkit-any-link");
                break;
            case CSSSelector::PseudoClassAutofill:
                builder.appendLiteral(":-webkit-autofill");
                break;
            case CSSSelector::PseudoClassAutofillStrongPassword:
                builder.appendLiteral(":-webkit-autofill-strong-password");
                break;
            case CSSSelector::PseudoClassAutofillStrongPasswordViewable:
                builder.appendLiteral(":-webkit-autofill-strong-password-viewable");
                break;
            case CSSSelector::PseudoClassDirectFocus:
                builder.appendLiteral(":-webkit-direct-focus");
                break;
            case CSSSelector::PseudoClassDrag:
                builder.appendLiteral(":-webkit-drag");
                break;
            case CSSSelector::PseudoClassFullPageMedia:
                builder.appendLiteral(":-webkit-full-page-media");
                break;
#if ENABLE(FULLSCREEN_API)
            case CSSSelector::PseudoClassFullScreen:
                builder.appendLiteral(":-webkit-full-screen");
                break;
            case CSSSelector::PseudoClassFullScreenAncestor:
                builder.appendLiteral(":-webkit-full-screen-ancestor");
                break;
            case CSSSelector::PseudoClassFullScreenDocument:
                builder.appendLiteral(":-webkit-full-screen-document");
                break;
            case CSSSelector::PseudoClassFullScreenControlsHidden:
                builder.appendLiteral(":-webkit-full-screen-controls-hidden");
                break;
#endif
#if ENABLE(PICTURE_IN_PICTURE_API)
            case CSSSelector::PseudoClassPictureInPicture:
                builder.appendLiteral(":picture-in-picture");
                break;
#endif
            case CSSSelector::PseudoClassActive:
                builder.appendLiteral(":active");
                break;
            case CSSSelector::PseudoClassChecked:
                builder.appendLiteral(":checked");
                break;
            case CSSSelector::PseudoClassCornerPresent:
                builder.appendLiteral(":corner-present");
                break;
            case CSSSelector::PseudoClassDecrement:
                builder.appendLiteral(":decrement");
                break;
            case CSSSelector::PseudoClassDefault:
                builder.appendLiteral(":default");
                break;
#if ENABLE(CSS_SELECTORS_LEVEL4)
            case CSSSelector::PseudoClassDir:
                builder.appendLiteral(":dir(");
                appendPseudoClassFunctionTail(builder, cs);
                break;
#endif
            case CSSSelector::PseudoClassDisabled:
                builder.appendLiteral(":disabled");
                break;
            case CSSSelector::PseudoClassDoubleButton:
                builder.appendLiteral(":double-button");
                break;
            case CSSSelector::PseudoClassEmpty:
                builder.appendLiteral(":empty");
                break;
            case CSSSelector::PseudoClassEnabled:
                builder.appendLiteral(":enabled");
                break;
            case CSSSelector::PseudoClassEnd:
                builder.appendLiteral(":end");
                break;
            case CSSSelector::PseudoClassFirstChild:
                builder.appendLiteral(":first-child");
                break;
            case CSSSelector::PseudoClassFirstOfType:
                builder.appendLiteral(":first-of-type");
                break;
            case CSSSelector::PseudoClassFocus:
                builder.appendLiteral(":focus");
                break;
            case CSSSelector::PseudoClassFocusWithin:
                builder.appendLiteral(":focus-within");
                break;
#if ENABLE(VIDEO)
            case CSSSelector::PseudoClassFuture:
                builder.appendLiteral(":future");
                break;
#endif
#if ENABLE(ATTACHMENT_ELEMENT)
            case CSSSelector::PseudoClassHasAttachment:
                builder.appendLiteral(":has-attachment");
                break;
#endif
            case CSSSelector::PseudoClassHorizontal:
                builder.appendLiteral(":horizontal");
                break;
            case CSSSelector::PseudoClassHover:
                builder.appendLiteral(":hover");
                break;
            case CSSSelector::PseudoClassInRange:
                builder.appendLiteral(":in-range");
                break;
            case CSSSelector::PseudoClassIncrement:
                builder.appendLiteral(":increment");
                break;
            case CSSSelector::PseudoClassIndeterminate:
                builder.appendLiteral(":indeterminate");
                break;
            case CSSSelector::PseudoClassInvalid:
                builder.appendLiteral(":invalid");
                break;
            case CSSSelector::PseudoClassLang:
                builder.appendLiteral(":lang(");
                ASSERT_WITH_MESSAGE(cs->argumentList() && !cs->argumentList()->isEmpty(), "An empty :lang() is invalid and should never be generated by the parser.");
                appendLangArgumentList(builder, *cs->argumentList());
                builder.append(')');
                break;
            case CSSSelector::PseudoClassLastChild:
                builder.appendLiteral(":last-child");
                break;
            case CSSSelector::PseudoClassLastOfType:
                builder.appendLiteral(":last-of-type");
                break;
            case CSSSelector::PseudoClassLink:
                builder.appendLiteral(":link");
                break;
            case CSSSelector::PseudoClassNoButton:
                builder.appendLiteral(":no-button");
                break;
            case CSSSelector::PseudoClassNot:
                builder.appendLiteral(":not(");
                cs->selectorList()->buildSelectorsText(builder);
                builder.append(')');
                break;
            case CSSSelector::PseudoClassNthChild:
                builder.appendLiteral(":nth-child(");
                outputNthChildAnPlusB(*cs, builder);
                if (const CSSSelectorList* selectorList = cs->selectorList()) {
                    builder.appendLiteral(" of ");
                    selectorList->buildSelectorsText(builder);
                }
                builder.append(')');
                break;
            case CSSSelector::PseudoClassNthLastChild:
                builder.appendLiteral(":nth-last-child(");
                outputNthChildAnPlusB(*cs, builder);
                if (const CSSSelectorList* selectorList = cs->selectorList()) {
                    builder.appendLiteral(" of ");
                    selectorList->buildSelectorsText(builder);
                }
                builder.append(')');
                break;
            case CSSSelector::PseudoClassNthLastOfType:
                builder.appendLiteral(":nth-last-of-type(");
                appendPseudoClassFunctionTail(builder, cs);
                break;
            case CSSSelector::PseudoClassNthOfType:
                builder.appendLiteral(":nth-of-type(");
                appendPseudoClassFunctionTail(builder, cs);
                break;
            case CSSSelector::PseudoClassOnlyChild:
                builder.appendLiteral(":only-child");
                break;
            case CSSSelector::PseudoClassOnlyOfType:
                builder.appendLiteral(":only-of-type");
                break;
            case CSSSelector::PseudoClassOptional:
                builder.appendLiteral(":optional");
                break;
            case CSSSelector::PseudoClassIs: {
                builder.appendLiteral(":is(");
                cs->selectorList()->buildSelectorsText(builder);
                builder.append(')');
                break;
            }
            case CSSSelector::PseudoClassMatches: {
                builder.appendLiteral(":matches(");
                cs->selectorList()->buildSelectorsText(builder);
                builder.append(')');
                break;
            }
            case CSSSelector::PseudoClassWhere: {
                builder.appendLiteral(":where(");
                cs->selectorList()->buildSelectorsText(builder);
                builder.append(')');
                break;
            }
            case CSSSelector::PseudoClassPlaceholderShown:
                builder.appendLiteral(":placeholder-shown");
                break;
            case CSSSelector::PseudoClassOutOfRange:
                builder.appendLiteral(":out-of-range");
                break;
#if ENABLE(VIDEO)
            case CSSSelector::PseudoClassPast:
                builder.appendLiteral(":past");
                break;
#endif
            case CSSSelector::PseudoClassReadOnly:
                builder.appendLiteral(":read-only");
                break;
            case CSSSelector::PseudoClassReadWrite:
                builder.appendLiteral(":read-write");
                break;
            case CSSSelector::PseudoClassRequired:
                builder.appendLiteral(":required");
                break;
#if ENABLE(CSS_SELECTORS_LEVEL4)
            case CSSSelector::PseudoClassRole:
                builder.appendLiteral(":role(");
                appendPseudoClassFunctionTail(builder, cs);
                break;
#endif
            case CSSSelector::PseudoClassRoot:
                builder.appendLiteral(":root");
                break;
            case CSSSelector::PseudoClassScope:
                builder.appendLiteral(":scope");
                break;
            case CSSSelector::PseudoClassSingleButton:
                builder.appendLiteral(":single-button");
                break;
            case CSSSelector::PseudoClassStart:
                builder.appendLiteral(":start");
                break;
            case CSSSelector::PseudoClassTarget:
                builder.appendLiteral(":target");
                break;
            case CSSSelector::PseudoClassValid:
                builder.appendLiteral(":valid");
                break;
            case CSSSelector::PseudoClassVertical:
                builder.appendLiteral(":vertical");
                break;
            case CSSSelector::PseudoClassVisited:
                builder.appendLiteral(":visited");
                break;
            case CSSSelector::PseudoClassWindowInactive:
                builder.appendLiteral(":window-inactive");
                break;
            case CSSSelector::PseudoClassHost:
                builder.appendLiteral(":host");
                break;
            case CSSSelector::PseudoClassDefined:
                builder.appendLiteral(":defined");
                break;
            case CSSSelector::PseudoClassUnknown:
                ASSERT_NOT_REACHED();
            }
        } else if (cs->match() == CSSSelector::PseudoElement) {
            switch (cs->pseudoElementType()) {
            case CSSSelector::PseudoElementSlotted:
                builder.appendLiteral("::slotted(");
                cs->selectorList()->buildSelectorsText(builder);
                builder.append(')');
                break;
            case CSSSelector::PseudoElementPart: {
                builder.appendLiteral("::part(");
                bool isFirst = true;
                for (auto& partName : *cs->argumentList()) {
                    if (!isFirst)
                        builder.append(' ');
                    isFirst = false;
                    builder.append(partName);
                }
                builder.append(')');
                break;
            }
            case CSSSelector::PseudoElementWebKitCustomLegacyPrefixed:
                if (cs->value() == "placeholder")
                    builder.appendLiteral("::-webkit-input-placeholder");
                break;
#if ENABLE(VIDEO)
            case CSSSelector::PseudoElementCue: {
                if (auto* selectorList = cs->selectorList()) {
                    builder.appendLiteral("::cue(");
                    selectorList->buildSelectorsText(builder);
                    builder.append(')');
                } else
                    builder.appendLiteral("::cue");
                break;
            }
#endif
            default:
                builder.appendLiteral("::");
                builder.append(cs->serializingValue());
            }
        } else if (cs->isAttributeSelector()) {
            builder.append('[');
            const AtomString& prefix = cs->attribute().prefix();
            if (!prefix.isEmpty()) {
                builder.append(prefix);
                builder.append('|');
            }
            builder.append(cs->attribute().localName());
            switch (cs->match()) {
                case CSSSelector::Exact:
                    builder.append('=');
                    break;
                case CSSSelector::Set:
                    // set has no operator or value, just the attrName
                    builder.append(']');
                    break;
                case CSSSelector::List:
                    builder.appendLiteral("~=");
                    break;
                case CSSSelector::Hyphen:
                    builder.appendLiteral("|=");
                    break;
                case CSSSelector::Begin:
                    builder.appendLiteral("^=");
                    break;
                case CSSSelector::End:
                    builder.appendLiteral("$=");
                    break;
                case CSSSelector::Contain:
                    builder.appendLiteral("*=");
                    break;
                default:
                    break;
            }
            if (cs->match() != CSSSelector::Set) {
                serializeString(cs->serializingValue(), builder);
                if (cs->attributeValueMatchingIsCaseInsensitive())
                    builder.appendLiteral(" i]");
                else
                    builder.append(']');
            }
        } else if (cs->match() == CSSSelector::PagePseudoClass) {
            switch (cs->pagePseudoClassType()) {
            case PagePseudoClassFirst:
                builder.appendLiteral(":first");
                break;
            case PagePseudoClassLeft:
                builder.appendLiteral(":left");
                break;
            case PagePseudoClassRight:
                builder.appendLiteral(":right");
                break;
            }
        }

        if (cs->relation() != CSSSelector::Subselector || !cs->tagHistory())
            break;
        cs = cs->tagHistory();
    }

    if (const CSSSelector* tagHistory = cs->tagHistory()) {
        switch (cs->relation()) {
        case CSSSelector::DescendantSpace:
            return tagHistory->selectorText(" " + builder.toString() + rightSide);
        case CSSSelector::Child:
            return tagHistory->selectorText(" > " + builder.toString() + rightSide);
        case CSSSelector::DirectAdjacent:
            return tagHistory->selectorText(" + " + builder.toString() + rightSide);
        case CSSSelector::IndirectAdjacent:
            return tagHistory->selectorText(" ~ " + builder.toString() + rightSide);
        case CSSSelector::Subselector:
            ASSERT_NOT_REACHED();
#if !ASSERT_ENABLED
            FALLTHROUGH;
#endif
        case CSSSelector::ShadowDescendant:
            return tagHistory->selectorText(builder.toString() + rightSide);
        }
    }
    return builder.toString() + rightSide;
}

void CSSSelector::setAttribute(const QualifiedName& value, bool convertToLowercase, AttributeMatchType matchType)
{
    createRareData();
    m_data.m_rareData->m_attribute = value;
    m_data.m_rareData->m_attributeCanonicalLocalName = convertToLowercase ? value.localName().convertToASCIILowercase() : value.localName();
    m_caseInsensitiveAttributeValueMatching = matchType == CaseInsensitive;
}
    
void CSSSelector::setArgument(const AtomString& value)
{
    createRareData();
    m_data.m_rareData->m_argument = value;
}

void CSSSelector::setArgumentList(std::unique_ptr<Vector<AtomString>> argumentList)
{
    createRareData();
    m_data.m_rareData->m_argumentList = WTFMove(argumentList);
}

void CSSSelector::setSelectorList(std::unique_ptr<CSSSelectorList> selectorList)
{
    createRareData();
    m_data.m_rareData->m_selectorList = WTFMove(selectorList);
}

void CSSSelector::setNth(int a, int b)
{
    createRareData();
    m_data.m_rareData->m_a = a;
    m_data.m_rareData->m_b = b;
}

bool CSSSelector::matchNth(int count) const
{
    ASSERT(m_hasRareData);
    return m_data.m_rareData->matchNth(count);
}

int CSSSelector::nthA() const
{
    ASSERT(m_hasRareData);
    return m_data.m_rareData->m_a;
}

int CSSSelector::nthB() const
{
    ASSERT(m_hasRareData);
    return m_data.m_rareData->m_b;
}

CSSSelector::RareData::RareData(AtomString&& value)
    : m_matchingValue(value)
    , m_serializingValue(value)
    , m_a(0)
    , m_b(0)
    , m_attribute(anyQName())
    , m_argument(nullAtom())
{
}

CSSSelector::RareData::~RareData() = default;

// a helper function for checking nth-arguments
bool CSSSelector::RareData::matchNth(int count)
{
    if (!m_a)
        return count == m_b;
    else if (m_a > 0) {
        if (count < m_b)
            return false;
        return (count - m_b) % m_a == 0;
    } else {
        if (count > m_b)
            return false;
        return (m_b - count) % (-m_a) == 0;
    }
}

} // namespace WebCore
