/*
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 *               2001 Andreas Schlapbach (schlpbch@iam.unibe.ch)
 *               2001-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc. All rights reserved.
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

#include "CSSOMUtils.h"
#include "CSSSelectorList.h"
#include "HTMLNames.h"
#include "SelectorPseudoTypeMap.h"
#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicStringHash.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

using namespace HTMLNames;

void CSSSelector::createRareData()
{
    ASSERT(m_match != Tag);
    if (m_hasRareData)
        return;
    // Move the value to the rare data stucture.
    m_data.m_rareData = RareData::create(adoptRef(m_data.m_value)).leakRef();
    m_hasRareData = true;
}

unsigned CSSSelector::specificity() const
{
    // make sure the result doesn't overflow
    static const unsigned maxValueMask = 0xffffff;
    static const unsigned idMask = 0xff0000;
    static const unsigned classMask = 0xff00;
    static const unsigned elementMask = 0xff;

    if (isForPage())
        return specificityForPage() & maxValueMask;

    unsigned total = 0;
    unsigned temp = 0;

    for (const CSSSelector* selector = this; selector; selector = selector->tagHistory()) {
        temp = total + selector->specificityForOneSelector();
        // Clamp each component to its max in the case of overflow.
        if ((temp & idMask) < (total & idMask))
            total |= idMask;
        else if ((temp & classMask) < (total & classMask))
            total |= classMask;
        else if ((temp & elementMask) < (total & elementMask))
            total |= elementMask;
        else
            total = temp;
    }
    return total;
}

inline unsigned CSSSelector::specificityForOneSelector() const
{
    // FIXME: Pseudo-elements and pseudo-classes do not have the same specificity. This function
    // isn't quite correct.
    switch (m_match) {
    case Id:
        return 0x10000;
    case Exact:
    case Class:
    case Set:
    case List:
    case Hyphen:
    case PseudoClass:
    case PseudoElement:
    case Contain:
    case Begin:
    case End:
        // FIXME: PsuedoAny should base the specificity on the sub-selectors.
        // See http://lists.w3.org/Archives/Public/www-style/2010Sep/0530.html
        if (pseudoType() == PseudoNot && selectorList())
            return selectorList()->first()->specificityForOneSelector();
        return 0x100;
    case Tag:
        return (tagQName().localName() != starAtom) ? 1 : 0;
    case Unknown:
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

unsigned CSSSelector::specificityForPage() const
{
    // See http://dev.w3.org/csswg/css3-page/#cascading-and-page-context
    unsigned s = 0;

    for (const CSSSelector* component = this; component; component = component->tagHistory()) {
        switch (component->m_match) {
        case Tag:
            s += tagQName().localName() == starAtom ? 0 : 4;
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

PseudoId CSSSelector::pseudoId(PseudoType type)
{
    switch (type) {
    case PseudoFirstLine:
        return FIRST_LINE;
    case PseudoFirstLetter:
        return FIRST_LETTER;
    case PseudoSelection:
        return SELECTION;
    case PseudoBefore:
        return BEFORE;
    case PseudoAfter:
        return AFTER;
    case PseudoScrollbar:
        return SCROLLBAR;
    case PseudoScrollbarButton:
        return SCROLLBAR_BUTTON;
    case PseudoScrollbarCorner:
        return SCROLLBAR_CORNER;
    case PseudoScrollbarThumb:
        return SCROLLBAR_THUMB;
    case PseudoScrollbarTrack:
        return SCROLLBAR_TRACK;
    case PseudoScrollbarTrackPiece:
        return SCROLLBAR_TRACK_PIECE;
    case PseudoResizer:
        return RESIZER;
#if ENABLE(FULLSCREEN_API)
    case PseudoFullScreen:
        return FULL_SCREEN;
    case PseudoFullScreenDocument:
        return FULL_SCREEN_DOCUMENT;
    case PseudoFullScreenAncestor:
        return FULL_SCREEN_ANCESTOR;
    case PseudoAnimatingFullScreenTransition:
        return ANIMATING_FULL_SCREEN_TRANSITION;
#endif
    case PseudoUnknown:
    case PseudoEmpty:
    case PseudoFirstChild:
    case PseudoFirstOfType:
    case PseudoLastChild:
    case PseudoLastOfType:
    case PseudoOnlyChild:
    case PseudoOnlyOfType:
    case PseudoNthChild:
    case PseudoNthOfType:
    case PseudoNthLastChild:
    case PseudoNthLastOfType:
    case PseudoLink:
    case PseudoVisited:
    case PseudoAny:
    case PseudoAnyLink:
    case PseudoAutofill:
    case PseudoHover:
    case PseudoDrag:
    case PseudoFocus:
    case PseudoActive:
    case PseudoChecked:
    case PseudoEnabled:
    case PseudoFullPageMedia:
    case PseudoDefault:
    case PseudoDisabled:
    case PseudoOptional:
    case PseudoRequired:
    case PseudoReadOnly:
    case PseudoReadWrite:
    case PseudoValid:
    case PseudoInvalid:
    case PseudoIndeterminate:
    case PseudoTarget:
    case PseudoLang:
    case PseudoNot:
    case PseudoRoot:
    case PseudoScope:
    case PseudoScrollbarBack:
    case PseudoScrollbarForward:
    case PseudoWindowInactive:
    case PseudoCornerPresent:
    case PseudoDecrement:
    case PseudoIncrement:
    case PseudoHorizontal:
    case PseudoVertical:
    case PseudoStart:
    case PseudoEnd:
    case PseudoDoubleButton:
    case PseudoSingleButton:
    case PseudoNoButton:
    case PseudoInRange:
    case PseudoOutOfRange:
    case PseudoUserAgentCustomElement:
    case PseudoWebKitCustomElement:
#if ENABLE(VIDEO_TRACK)
    case PseudoCue:
    case PseudoFuture:
    case PseudoPast:
#endif
        return NOPSEUDO;
    }

    ASSERT_NOT_REACHED();
    return NOPSEUDO;
}

CSSSelector::PseudoType CSSSelector::parsePseudoElementType(const String& name)
{
    if (name.isNull())
        return PseudoUnknown;

    PseudoType type = parsePseudoElementString(*name.impl());
    if (type == PseudoUnknown) {
        if (name.startsWith("-webkit-"))
            type = PseudoWebKitCustomElement;

        if (name.startsWith("x-"))
            type = PseudoUserAgentCustomElement;
    }
    return type;
}


bool CSSSelector::operator==(const CSSSelector& other) const
{
    const CSSSelector* sel1 = this;
    const CSSSelector* sel2 = &other;

    while (sel1 && sel2) {
        if (sel1->attribute() != sel2->attribute()
            || sel1->relation() != sel2->relation()
            || sel1->m_match != sel2->m_match
            || sel1->value() != sel2->value()
            || sel1->pseudoType() != sel2->pseudoType()
            || sel1->argument() != sel2->argument()) {
            return false;
        }
        if (sel1->m_match == Tag) {
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

static void appendPseudoClassFunctionTail(StringBuilder& str, const CSSSelector* selector)
{
    switch (selector->pseudoType()) {
    case CSSSelector::PseudoLang:
    case CSSSelector::PseudoNthChild:
    case CSSSelector::PseudoNthLastChild:
    case CSSSelector::PseudoNthOfType:
    case CSSSelector::PseudoNthLastOfType:
        str.append(selector->argument());
        str.append(')');
        break;
    default:
        break;
    }

}

String CSSSelector::selectorText(const String& rightSide) const
{
    StringBuilder str;

    if (m_match == CSSSelector::Tag && !m_tagIsForNamespaceRule) {
        if (tagQName().prefix().isNull())
            str.append(tagQName().localName());
        else {
            str.append(tagQName().prefix().string());
            str.append('|');
            str.append(tagQName().localName());
        }
    }

    const CSSSelector* cs = this;
    while (true) {
        if (cs->m_match == CSSSelector::Id) {
            str.append('#');
            serializeIdentifier(cs->value(), str);
        } else if (cs->m_match == CSSSelector::Class) {
            str.append('.');
            serializeIdentifier(cs->value(), str);
        } else if (cs->m_match == CSSSelector::PseudoClass) {
            switch (cs->pseudoType()) {
#if ENABLE(FULLSCREEN_API)
            case CSSSelector::PseudoAnimatingFullScreenTransition:
                str.appendLiteral(":-webkit-animating-full-screen-transition");
                break;
#endif
            case CSSSelector::PseudoAny: {
                str.appendLiteral(":-webkit-any(");
                const CSSSelector* firstSubSelector = cs->selectorList()->first();
                for (const CSSSelector* subSelector = firstSubSelector; subSelector; subSelector = CSSSelectorList::next(subSelector)) {
                    if (subSelector != firstSubSelector)
                        str.append(',');
                    str.append(subSelector->selectorText());
                }
                str.append(')');
                break;
            }
            case CSSSelector::PseudoAnyLink:
                str.appendLiteral(":-webkit-any-link");
                break;
            case CSSSelector::PseudoAutofill:
                str.appendLiteral(":-webkit-autofill");
                break;
            case CSSSelector::PseudoDrag:
                str.appendLiteral(":-webkit-drag");
                break;
            case CSSSelector::PseudoFullPageMedia:
                str.appendLiteral(":-webkit-full-page-media");
                break;
#if ENABLE(FULLSCREEN_API)
            case CSSSelector::PseudoFullScreen:
                str.appendLiteral(":-webkit-full-screen");
                break;
            case CSSSelector::PseudoFullScreenAncestor:
                str.appendLiteral(":-webkit-full-screen-ancestor");
                break;
            case CSSSelector::PseudoFullScreenDocument:
                str.appendLiteral(":-webkit-full-screen-document");
                break;
#endif
            case CSSSelector::PseudoActive:
                str.appendLiteral(":active");
                break;
            case CSSSelector::PseudoChecked:
                str.appendLiteral(":checked");
                break;
            case CSSSelector::PseudoCornerPresent:
                str.appendLiteral(":corner-present");
                break;
            case CSSSelector::PseudoDecrement:
                str.appendLiteral(":decrement");
                break;
            case CSSSelector::PseudoDefault:
                str.appendLiteral(":default");
                break;
            case CSSSelector::PseudoDisabled:
                str.appendLiteral(":disabled");
                break;
            case CSSSelector::PseudoDoubleButton:
                str.appendLiteral(":double-button");
                break;
            case CSSSelector::PseudoEmpty:
                str.appendLiteral(":empty");
                break;
            case CSSSelector::PseudoEnabled:
                str.appendLiteral(":enabled");
                break;
            case CSSSelector::PseudoEnd:
                str.appendLiteral(":end");
                break;
            case CSSSelector::PseudoFirstChild:
                str.appendLiteral(":first-child");
                break;
            case CSSSelector::PseudoFirstOfType:
                str.appendLiteral(":first-of-type");
                break;
            case CSSSelector::PseudoFocus:
                str.appendLiteral(":focus");
                break;
#if ENABLE(VIDEO_TRACK)
            case CSSSelector::PseudoFuture:
                str.appendLiteral(":future");
                break;
#endif
            case CSSSelector::PseudoHorizontal:
                str.appendLiteral(":horizontal");
                break;
            case CSSSelector::PseudoHover:
                str.appendLiteral(":hover");
                break;
            case CSSSelector::PseudoInRange:
                str.appendLiteral(":in-range");
                break;
            case CSSSelector::PseudoIncrement:
                str.appendLiteral(":increment");
                break;
            case CSSSelector::PseudoIndeterminate:
                str.appendLiteral(":indeterminate");
                break;
            case CSSSelector::PseudoInvalid:
                str.appendLiteral(":invalid");
                break;
            case CSSSelector::PseudoLang:
                str.appendLiteral(":lang(");
                appendPseudoClassFunctionTail(str, cs);
                break;
            case CSSSelector::PseudoLastChild:
                str.appendLiteral(":last-child");
                break;
            case CSSSelector::PseudoLastOfType:
                str.appendLiteral(":last-of-type");
                break;
            case CSSSelector::PseudoLink:
                str.appendLiteral(":link");
                break;
            case CSSSelector::PseudoNoButton:
                str.appendLiteral(":no-button");
                break;
            case CSSSelector::PseudoNot:
                str.appendLiteral(":not(");
                if (const CSSSelectorList* selectorList = cs->selectorList())
                    str.append(selectorList->first()->selectorText());
                str.append(')');
                break;
            case CSSSelector::PseudoNthChild:
                str.appendLiteral(":nth-child(");
                appendPseudoClassFunctionTail(str, cs);
                break;
            case CSSSelector::PseudoNthLastChild:
                str.appendLiteral(":nth-last-child(");
                appendPseudoClassFunctionTail(str, cs);
                break;
            case CSSSelector::PseudoNthLastOfType:
                str.appendLiteral(":nth-last-of-type(");
                appendPseudoClassFunctionTail(str, cs);
                break;
            case CSSSelector::PseudoNthOfType:
                str.appendLiteral(":nth-of-type(");
                appendPseudoClassFunctionTail(str, cs);
                break;
            case CSSSelector::PseudoOnlyChild:
                str.appendLiteral(":only-child");
                break;
            case CSSSelector::PseudoOnlyOfType:
                str.appendLiteral(":only-of-type");
                break;
            case CSSSelector::PseudoOptional:
                str.appendLiteral(":optional");
                break;
            case CSSSelector::PseudoOutOfRange:
                str.appendLiteral(":out-of-range");
                break;
#if ENABLE(VIDEO_TRACK)
            case CSSSelector::PseudoPast:
                str.appendLiteral(":past");
                break;
#endif
            case CSSSelector::PseudoReadOnly:
                str.appendLiteral(":read-only");
                break;
            case CSSSelector::PseudoReadWrite:
                str.appendLiteral(":read-write");
                break;
            case CSSSelector::PseudoRequired:
                str.appendLiteral(":required");
                break;
            case CSSSelector::PseudoRoot:
                str.appendLiteral(":root");
                break;
            case CSSSelector::PseudoScope:
                str.appendLiteral(":scope");
                break;
            case CSSSelector::PseudoSingleButton:
                str.appendLiteral(":single-button");
                break;
            case CSSSelector::PseudoStart:
                str.appendLiteral(":start");
                break;
            case CSSSelector::PseudoTarget:
                str.appendLiteral(":target");
                break;
            case CSSSelector::PseudoValid:
                str.appendLiteral(":valid");
                break;
            case CSSSelector::PseudoVertical:
                str.appendLiteral(":vertical");
                break;
            case CSSSelector::PseudoVisited:
                str.appendLiteral(":visited");
                break;
            case CSSSelector::PseudoWindowInactive:
                str.appendLiteral(":window-inactive");
                break;
            default:
                ASSERT_NOT_REACHED();
            }
        } else if (cs->m_match == CSSSelector::PseudoElement) {
            str.appendLiteral("::");
            str.append(cs->value());
        } else if (cs->isAttributeSelector()) {
            str.append('[');
            const AtomicString& prefix = cs->attribute().prefix();
            if (!prefix.isNull()) {
                str.append(prefix);
                str.append('|');
            }
            str.append(cs->attribute().localName());
            switch (cs->m_match) {
                case CSSSelector::Exact:
                    str.append('=');
                    break;
                case CSSSelector::Set:
                    // set has no operator or value, just the attrName
                    str.append(']');
                    break;
                case CSSSelector::List:
                    str.appendLiteral("~=");
                    break;
                case CSSSelector::Hyphen:
                    str.appendLiteral("|=");
                    break;
                case CSSSelector::Begin:
                    str.appendLiteral("^=");
                    break;
                case CSSSelector::End:
                    str.appendLiteral("$=");
                    break;
                case CSSSelector::Contain:
                    str.appendLiteral("*=");
                    break;
                default:
                    break;
            }
            if (cs->m_match != CSSSelector::Set) {
                serializeString(cs->value(), str);
                str.append(']');
            }
        } else if (cs->m_match == CSSSelector::PagePseudoClass) {
            switch (cs->pagePseudoClassType()) {
            case PagePseudoClassFirst:
                str.appendLiteral(":first");
                break;
            case PagePseudoClassLeft:
                str.appendLiteral(":left");
                break;
            case PagePseudoClassRight:
                str.appendLiteral(":right");
                break;
            }
        }

        if (cs->relation() != CSSSelector::SubSelector || !cs->tagHistory())
            break;
        cs = cs->tagHistory();
    }

    if (const CSSSelector* tagHistory = cs->tagHistory()) {
        switch (cs->relation()) {
        case CSSSelector::Descendant:
            return tagHistory->selectorText(" " + str.toString() + rightSide);
        case CSSSelector::Child:
            return tagHistory->selectorText(" > " + str.toString() + rightSide);
        case CSSSelector::DirectAdjacent:
            return tagHistory->selectorText(" + " + str.toString() + rightSide);
        case CSSSelector::IndirectAdjacent:
            return tagHistory->selectorText(" ~ " + str.toString() + rightSide);
        case CSSSelector::SubSelector:
            ASSERT_NOT_REACHED();
#if ASSERT_DISABLED
            FALLTHROUGH;
#endif
        case CSSSelector::ShadowDescendant:
            return tagHistory->selectorText(str.toString() + rightSide);
        }
    }
    return str.toString() + rightSide;
}

void CSSSelector::setAttribute(const QualifiedName& value, bool isCaseInsensitive)
{
    createRareData();
    m_data.m_rareData->m_attribute = value;
    m_data.m_rareData->m_attributeCanonicalLocalName = isCaseInsensitive ? value.localName().lower() : value.localName();
}

void CSSSelector::setArgument(const AtomicString& value)
{
    createRareData();
    m_data.m_rareData->m_argument = value;
}

void CSSSelector::setSelectorList(std::unique_ptr<CSSSelectorList> selectorList)
{
    createRareData();
    m_data.m_rareData->m_selectorList = std::move(selectorList);
}

bool CSSSelector::parseNth() const
{
    if (!m_hasRareData)
        return false;
    if (m_parsedNth)
        return true;
    m_parsedNth = m_data.m_rareData->parseNth();
    return m_parsedNth;
}

bool CSSSelector::matchNth(int count) const
{
    ASSERT(m_hasRareData);
    return m_data.m_rareData->matchNth(count);
}

CSSSelector::RareData::RareData(PassRefPtr<AtomicStringImpl> value)
    : m_value(value.leakRef())
    , m_a(0)
    , m_b(0)
    , m_attribute(anyQName())
    , m_argument(nullAtom)
{
}

CSSSelector::RareData::~RareData()
{
    if (m_value)
        m_value->deref();
}

// a helper function for parsing nth-arguments
bool CSSSelector::RareData::parseNth()
{
    String argument = m_argument.lower();

    if (argument.isEmpty())
        return false;

    m_a = 0;
    m_b = 0;
    if (argument == "odd") {
        m_a = 2;
        m_b = 1;
    } else if (argument == "even") {
        m_a = 2;
        m_b = 0;
    } else {
        size_t n = argument.find('n');
        if (n != notFound) {
            if (argument[0] == '-') {
                if (n == 1)
                    m_a = -1; // -n == -1n
                else
                    m_a = argument.substring(0, n).toInt();
            } else if (!n)
                m_a = 1; // n == 1n
            else
                m_a = argument.substring(0, n).toInt();

            size_t p = argument.find('+', n);
            if (p != notFound)
                m_b = argument.substring(p + 1, argument.length() - p - 1).toInt();
            else {
                p = argument.find('-', n);
                if (p != notFound)
                    m_b = -argument.substring(p + 1, argument.length() - p - 1).toInt();
            }
        } else
            m_b = argument.toInt();
    }
    return true;
}

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
