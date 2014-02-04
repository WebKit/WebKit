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
        case PseudoClass:
            switch (component->pseudoType()) {
            case PseudoFirstPage:
                s += 2;
                break;
            case PseudoLeftPage:
            case PseudoRightPage:
                s += 1;
                break;
            case PseudoNotParsed:
                break;
            default:
                ASSERT_NOT_REACHED();
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
    case PseudoFirstPage:
    case PseudoLeftPage:
    case PseudoRightPage:
    case PseudoInRange:
    case PseudoOutOfRange:
    case PseudoUserAgentCustomElement:
    case PseudoWebKitCustomElement:
#if ENABLE(VIDEO_TRACK)
    case PseudoCue:
    case PseudoFutureCue:
    case PseudoPastCue:
#endif
#if ENABLE(IFRAME_SEAMLESS)
    case PseudoSeamlessDocument:
#endif
        return NOPSEUDO;
    case PseudoNotParsed:
        ASSERT_NOT_REACHED();
        return NOPSEUDO;
    }

    ASSERT_NOT_REACHED();
    return NOPSEUDO;
}

static NEVER_INLINE void populatePseudoTypeByNameMap(HashMap<AtomicString, CSSSelector::PseudoType>& map)
{
    struct TableEntry {
        const char* name;
        unsigned nameLength;
        CSSSelector::PseudoType type;
    };

    // Could use strlen in this macro but not all compilers can constant-fold it.
#define TABLE_ENTRY(name, type) { name, sizeof(name) - 1, CSSSelector::type },

    static const TableEntry table[] = {
        TABLE_ENTRY("-khtml-drag", PseudoDrag)
        TABLE_ENTRY("-webkit-any(", PseudoAny)
        TABLE_ENTRY("-webkit-any-link", PseudoAnyLink)
        TABLE_ENTRY("-webkit-autofill", PseudoAutofill)
        TABLE_ENTRY("-webkit-drag", PseudoDrag)
        TABLE_ENTRY("-webkit-full-page-media", PseudoFullPageMedia)
        TABLE_ENTRY("-webkit-resizer", PseudoResizer)
        TABLE_ENTRY("-webkit-scrollbar", PseudoScrollbar)
        TABLE_ENTRY("-webkit-scrollbar-button", PseudoScrollbarButton)
        TABLE_ENTRY("-webkit-scrollbar-corner", PseudoScrollbarCorner)
        TABLE_ENTRY("-webkit-scrollbar-thumb", PseudoScrollbarThumb)
        TABLE_ENTRY("-webkit-scrollbar-track", PseudoScrollbarTrack)
        TABLE_ENTRY("-webkit-scrollbar-track-piece", PseudoScrollbarTrackPiece)
        TABLE_ENTRY("active", PseudoActive)
        TABLE_ENTRY("after", PseudoAfter)
        TABLE_ENTRY("before", PseudoBefore)
        TABLE_ENTRY("checked", PseudoChecked)
        TABLE_ENTRY("corner-present", PseudoCornerPresent)
        TABLE_ENTRY("decrement", PseudoDecrement)
        TABLE_ENTRY("default", PseudoDefault)
        TABLE_ENTRY("disabled", PseudoDisabled)
        TABLE_ENTRY("double-button", PseudoDoubleButton)
        TABLE_ENTRY("empty", PseudoEmpty)
        TABLE_ENTRY("enabled", PseudoEnabled)
        TABLE_ENTRY("end", PseudoEnd)
        TABLE_ENTRY("first", PseudoFirstPage)
        TABLE_ENTRY("first-child", PseudoFirstChild)
        TABLE_ENTRY("first-letter", PseudoFirstLetter)
        TABLE_ENTRY("first-line", PseudoFirstLine)
        TABLE_ENTRY("first-of-type", PseudoFirstOfType)
        TABLE_ENTRY("focus", PseudoFocus)
        TABLE_ENTRY("horizontal", PseudoHorizontal)
        TABLE_ENTRY("hover", PseudoHover)
        TABLE_ENTRY("in-range", PseudoInRange)
        TABLE_ENTRY("increment", PseudoIncrement)
        TABLE_ENTRY("indeterminate", PseudoIndeterminate)
        TABLE_ENTRY("invalid", PseudoInvalid)
        TABLE_ENTRY("lang(", PseudoLang)
        TABLE_ENTRY("last-child", PseudoLastChild)
        TABLE_ENTRY("last-of-type", PseudoLastOfType)
        TABLE_ENTRY("left", PseudoLeftPage)
        TABLE_ENTRY("link", PseudoLink)
        TABLE_ENTRY("no-button", PseudoNoButton)
        TABLE_ENTRY("not(", PseudoNot)
        TABLE_ENTRY("nth-child(", PseudoNthChild)
        TABLE_ENTRY("nth-last-child(", PseudoNthLastChild)
        TABLE_ENTRY("nth-last-of-type(", PseudoNthLastOfType)
        TABLE_ENTRY("nth-of-type(", PseudoNthOfType)
        TABLE_ENTRY("only-child", PseudoOnlyChild)
        TABLE_ENTRY("only-of-type", PseudoOnlyOfType)
        TABLE_ENTRY("optional", PseudoOptional)
        TABLE_ENTRY("out-of-range", PseudoOutOfRange)
        TABLE_ENTRY("read-only", PseudoReadOnly)
        TABLE_ENTRY("read-write", PseudoReadWrite)
        TABLE_ENTRY("required", PseudoRequired)
        TABLE_ENTRY("right", PseudoRightPage)
        TABLE_ENTRY("root", PseudoRoot)
        TABLE_ENTRY("scope", PseudoScope)
        TABLE_ENTRY("selection", PseudoSelection)
        TABLE_ENTRY("single-button", PseudoSingleButton)
        TABLE_ENTRY("start", PseudoStart)
        TABLE_ENTRY("target", PseudoTarget)
        TABLE_ENTRY("valid", PseudoValid)
        TABLE_ENTRY("vertical", PseudoVertical)
        TABLE_ENTRY("visited", PseudoVisited)
        TABLE_ENTRY("window-inactive", PseudoWindowInactive)

#if ENABLE(FULLSCREEN_API)
        TABLE_ENTRY("-webkit-animating-full-screen-transition", PseudoAnimatingFullScreenTransition)
        TABLE_ENTRY("-webkit-full-screen", PseudoFullScreen)
        TABLE_ENTRY("-webkit-full-screen-ancestor", PseudoFullScreenAncestor)
        TABLE_ENTRY("-webkit-full-screen-document", PseudoFullScreenDocument)
#endif

#if ENABLE(IFRAME_SEAMLESS)
        TABLE_ENTRY("-webkit-seamless-document", PseudoSeamlessDocument)
#endif

#if ENABLE(VIDEO_TRACK)
        TABLE_ENTRY("cue(", PseudoCue)
        TABLE_ENTRY("future", PseudoFutureCue)
        TABLE_ENTRY("past", PseudoPastCue)
#endif
    };

#undef TABLE_ENTRY

    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(table); ++i)
        map.add(AtomicString(table[i].name, table[i].nameLength, AtomicString::ConstructFromLiteral), table[i].type);
}

CSSSelector::PseudoType CSSSelector::parsePseudoType(const AtomicString& name)
{
    if (name.isNull())
        return PseudoUnknown;

    static NeverDestroyed<HashMap<AtomicString, CSSSelector::PseudoType>> types;
    if (types.get().isEmpty())
        populatePseudoTypeByNameMap(types);
    if (PseudoType type = types.get().get(name))
        return type;

    if (name.startsWith("-webkit-"))
        return PseudoWebKitCustomElement;

    // FIXME: This is strange. Why would all strings that start with "cue" be "user agent custom"?
    if (name.startsWith("x-") || name.startsWith("cue"))
        return PseudoUserAgentCustomElement;

    return PseudoUnknown;
}

void CSSSelector::extractPseudoType() const
{
    if (m_match != PseudoClass && m_match != PseudoElement && m_match != PagePseudoClass)
        return;

    m_pseudoType = parsePseudoType(value());

    bool element = false; // pseudo-element
    bool compat = false; // single colon compatbility mode
    bool isPagePseudoClass = false; // Page pseudo-class

    switch (m_pseudoType) {
    case PseudoAfter:
    case PseudoBefore:
#if ENABLE(VIDEO_TRACK)
    case PseudoCue:
#endif
    case PseudoFirstLetter:
    case PseudoFirstLine:
        compat = true;
        FALLTHROUGH;
    case PseudoResizer:
    case PseudoScrollbar:
    case PseudoScrollbarCorner:
    case PseudoScrollbarButton:
    case PseudoScrollbarThumb:
    case PseudoScrollbarTrack:
    case PseudoScrollbarTrackPiece:
    case PseudoSelection:
    case PseudoUserAgentCustomElement:
    case PseudoWebKitCustomElement:
        element = true;
        break;
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
    case PseudoScope:
    case PseudoValid:
    case PseudoInvalid:
    case PseudoIndeterminate:
    case PseudoTarget:
    case PseudoLang:
    case PseudoNot:
    case PseudoRoot:
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
    case PseudoNotParsed:
#if ENABLE(FULLSCREEN_API)
    case PseudoFullScreen:
    case PseudoFullScreenDocument:
    case PseudoFullScreenAncestor:
    case PseudoAnimatingFullScreenTransition:
#endif
#if ENABLE(IFRAME_SEAMLESS)
    case PseudoSeamlessDocument:
#endif
    case PseudoInRange:
    case PseudoOutOfRange:
#if ENABLE(VIDEO_TRACK)
    case PseudoFutureCue:
    case PseudoPastCue:
#endif
        break;
    case PseudoFirstPage:
    case PseudoLeftPage:
    case PseudoRightPage:
        isPagePseudoClass = true;
        break;
    }

    bool matchPagePseudoClass = (m_match == PagePseudoClass);
    if (matchPagePseudoClass != isPagePseudoClass)
        m_pseudoType = PseudoUnknown;
    else if (m_match == PseudoClass && element) {
        if (!compat)
            m_pseudoType = PseudoUnknown;
        else
           m_match = PseudoElement;
    } else if (m_match == PseudoElement && !element)
        m_pseudoType = PseudoUnknown;
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
        } else if (cs->m_match == CSSSelector::PseudoClass || cs->m_match == CSSSelector::PagePseudoClass) {
            str.append(':');
            str.append(cs->value());

            switch (cs->pseudoType()) {
            case PseudoNot:
                if (const CSSSelectorList* selectorList = cs->selectorList())
                    str.append(selectorList->first()->selectorText());
                str.append(')');
                break;
            case PseudoLang:
            case PseudoNthChild:
            case PseudoNthLastChild:
            case PseudoNthOfType:
            case PseudoNthLastOfType:
                str.append(cs->argument());
                str.append(')');
                break;
            case PseudoAny: {
                const CSSSelector* firstSubSelector = cs->selectorList()->first();
                for (const CSSSelector* subSelector = firstSubSelector; subSelector; subSelector = CSSSelectorList::next(subSelector)) {
                    if (subSelector != firstSubSelector)
                        str.append(',');
                    str.append(subSelector->selectorText());
                }
                str.append(')');
                break;
            }
            default:
                break;
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

void CSSSelector::setSelectorList(PassOwnPtr<CSSSelectorList> selectorList)
{
    createRareData();
    m_data.m_rareData->m_selectorList = selectorList;
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
