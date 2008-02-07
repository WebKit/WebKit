/*
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 *               2001 Andreas Schlapbach (schlpbch@iam.unibe.ch)
 *               2001-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

namespace WebCore {

void CSSSelector::print()
{
    if (m_tagHistory)
        m_tagHistory->print();
}

unsigned int CSSSelector::specificity()
{
    // FIXME: Pseudo-elements and pseudo-classes do not have the same specificity. This function
    // isn't quite correct.
    int s = (m_tag.localName() == starAtom ? 0 : 1);
    switch (m_match) {
        case Id:
            s += 0x10000;
            break;
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
            s += 0x100;
        case None:
            break;
    }

    if (m_tagHistory)
        s += m_tagHistory->specificity();

    // make sure it doesn't overflow
    return s & 0xffffff;
}

void CSSSelector::extractPseudoType() const
{
    if (m_match != PseudoClass && m_match != PseudoElement)
        return;

    static AtomicString active("active");
    static AtomicString after("after");
    static AtomicString anyLink("-webkit-any-link");
    static AtomicString autofill("-webkit-autofill");
    static AtomicString before("before");
    static AtomicString checked("checked");
    static AtomicString fileUploadButton("-webkit-file-upload-button");
    static AtomicString disabled("disabled");
    static AtomicString drag("-webkit-drag");
    static AtomicString dragAlias("-khtml-drag"); // was documented with this name in Apple documentation, so keep an alias
    static AtomicString empty("empty");
    static AtomicString enabled("enabled");
    static AtomicString firstChild("first-child");
    static AtomicString firstLetter("first-letter");
    static AtomicString firstLine("first-line");
    static AtomicString firstOfType("first-of-type");
    static AtomicString nthChild("nth-child(");
    static AtomicString nthOfType("nth-of-type(");
    static AtomicString nthLastChild("nth-last-child(");
    static AtomicString nthLastOfType("nth-last-of-type(");
    static AtomicString focus("focus");
    static AtomicString hover("hover");
    static AtomicString indeterminate("indeterminate");
    static AtomicString lastChild("last-child");
    static AtomicString lastOfType("last-of-type");
    static AtomicString link("link");
    static AtomicString lang("lang(");
    static AtomicString mediaControlsPanel("-webkit-media-controls-panel");
    static AtomicString mediaControlsMuteButton("-webkit-media-controls-mute-button");
    static AtomicString mediaControlsPlayButton("-webkit-media-controls-play-button");
    static AtomicString mediaControlsTimeDisplay("-webkit-media-controls-time-display");
    static AtomicString mediaControlsTimeline("-webkit-media-controls-timeline");
    static AtomicString mediaControlsSeekBackButton("-webkit-media-controls-seek-back-button");
    static AtomicString mediaControlsSeekForwardButton("-webkit-media-controls-seek-forward-button");
    static AtomicString mediaControlsFullscreenButton("-webkit-media-controls-fullscreen-button");
    static AtomicString notStr("not(");
    static AtomicString onlyChild("only-child");
    static AtomicString onlyOfType("only-of-type");
    static AtomicString root("root");
    static AtomicString searchCancelButton("-webkit-search-cancel-button");
    static AtomicString searchDecoration("-webkit-search-decoration");
    static AtomicString searchResultsDecoration("-webkit-search-results-decoration");
    static AtomicString searchResultsButton("-webkit-search-results-button");
    static AtomicString selection("selection");
    static AtomicString sliderThumb("-webkit-slider-thumb");
    static AtomicString target("target");
    static AtomicString visited("visited");

    bool element = false; // pseudo-element
    bool compat = false; // single colon compatbility mode

    m_pseudoType = PseudoUnknown;
    if (m_value == active)
        m_pseudoType = PseudoActive;
    else if (m_value == after) {
        m_pseudoType = PseudoAfter;
        element = true;
        compat = true;
    } else if (m_value == anyLink)
        m_pseudoType = PseudoAnyLink;
    else if (m_value == autofill)
        m_pseudoType = PseudoAutofill;
    else if (m_value == before) {
        m_pseudoType = PseudoBefore;
        element = true;
        compat = true;
    } else if (m_value == checked)
        m_pseudoType = PseudoChecked;
    else if (m_value == fileUploadButton) {
        m_pseudoType = PseudoFileUploadButton;
        element = true;
    } else if (m_value == disabled)
        m_pseudoType = PseudoDisabled;
    else if (m_value == drag || m_value == dragAlias)
        m_pseudoType = PseudoDrag;
    else if (m_value == enabled)
        m_pseudoType = PseudoEnabled;
    else if (m_value == empty)
        m_pseudoType = PseudoEmpty;
    else if (m_value == firstChild)
        m_pseudoType = PseudoFirstChild;
    else if (m_value == lastChild)
        m_pseudoType = PseudoLastChild;
    else if (m_value == lastOfType)
        m_pseudoType = PseudoLastOfType;
    else if (m_value == onlyChild)
        m_pseudoType = PseudoOnlyChild;
    else if (m_value == onlyOfType)
        m_pseudoType = PseudoOnlyOfType;
    else if (m_value == firstLetter) {
        m_pseudoType = PseudoFirstLetter;
        element = true;
        compat = true;
    } else if (m_value == firstLine) {
        m_pseudoType = PseudoFirstLine;
        element = true;
        compat = true;
    } else if (m_value == firstOfType)
        m_pseudoType = PseudoFirstOfType;
    else if (m_value == focus)
        m_pseudoType = PseudoFocus;
    else if (m_value == hover)
        m_pseudoType = PseudoHover;
    else if (m_value == indeterminate)
        m_pseudoType = PseudoIndeterminate;
    else if (m_value == link)
        m_pseudoType = PseudoLink;
    else if (m_value == lang)
        m_pseudoType = PseudoLang;
    else if (m_value == mediaControlsPanel) {
        m_pseudoType = PseudoMediaControlsPanel;
        element = true;
    } else if (m_value == mediaControlsMuteButton) {
        m_pseudoType = PseudoMediaControlsMuteButton;
        element = true;
    } else if (m_value == mediaControlsPlayButton) {
        m_pseudoType = PseudoMediaControlsPlayButton;
        element = true;
    } else if (m_value == mediaControlsTimeDisplay) {
        m_pseudoType = PseudoMediaControlsTimeDisplay;
        element = true;
    } else if (m_value == mediaControlsTimeline) {
        m_pseudoType = PseudoMediaControlsTimeline;
        element = true;
    } else if (m_value == mediaControlsSeekBackButton) {
        m_pseudoType = PseudoMediaControlsSeekBackButton;
        element = true;
    } else if (m_value == mediaControlsSeekForwardButton) {
        m_pseudoType = PseudoMediaControlsSeekForwardButton;
        element = true;
    } else if (m_value == mediaControlsFullscreenButton) {
        m_pseudoType = PseudoMediaControlsFullscreenButton;
        element = true;
    } else if (m_value == notStr)
        m_pseudoType = PseudoNot;
    else if (m_value == nthChild)
        m_pseudoType = PseudoNthChild;
    else if (m_value == nthOfType)
        m_pseudoType = PseudoNthOfType;
    else if (m_value == nthLastChild)
        m_pseudoType = PseudoNthLastChild;
    else if (m_value == nthLastOfType)
        m_pseudoType = PseudoNthLastOfType;
    else if (m_value == root)
        m_pseudoType = PseudoRoot;
    else if (m_value == searchCancelButton) {
        m_pseudoType = PseudoSearchCancelButton;
        element = true;
    } else if (m_value == searchDecoration) {
        m_pseudoType = PseudoSearchDecoration;
        element = true;
    } else if (m_value == searchResultsDecoration) {
        m_pseudoType = PseudoSearchResultsDecoration;
        element = true;
    } else if (m_value == searchResultsButton) {
        m_pseudoType = PseudoSearchResultsButton;
        element = true;
    }  else if (m_value == selection) {
        m_pseudoType = PseudoSelection;
        element = true;
    } else if (m_value == sliderThumb) {
        m_pseudoType = PseudoSliderThumb;
        element = true;
    } else if (m_value == target)
        m_pseudoType = PseudoTarget;
    else if (m_value == visited)
        m_pseudoType = PseudoVisited;

    if (m_match == PseudoClass && element) {
        if (!compat) 
            m_pseudoType = PseudoUnknown;
        else 
           m_match = PseudoElement;
    } else if (m_match == PseudoElement && !element)
        m_pseudoType = PseudoUnknown;
}

bool CSSSelector::operator==(const CSSSelector& other)
{
    const CSSSelector* sel1 = this;
    const CSSSelector* sel2 = &other;

    while (sel1 && sel2) {
        if (sel1->m_tag != sel2->m_tag || sel1->m_attr != sel2->m_attr ||
             sel1->relation() != sel2->relation() || sel1->m_match != sel2->m_match ||
             sel1->m_value != sel2->m_value ||
             sel1->pseudoType() != sel2->pseudoType() ||
             sel1->m_argument != sel2->m_argument)
            return false;
        sel1 = sel1->m_tagHistory;
        sel2 = sel2->m_tagHistory;
    }

    if (sel1 || sel2)
        return false;

    return true;
}

String CSSSelector::selectorText() const
{
    String str = "";

    const AtomicString& prefix = m_tag.prefix();
    const AtomicString& localName = m_tag.localName();
    if (m_match == CSSSelector::None || !prefix.isNull() || localName != starAtom) {
        if (prefix.isNull())
            str = localName;
        else
            str = prefix + "|" + localName;
    }

    const CSSSelector* cs = this;
    while (true) {
        if (cs->m_match == CSSSelector::Id) {
            str += "#";
            str += cs->m_value;
        } else if (cs->m_match == CSSSelector::Class) {
            str += ".";
            str += cs->m_value;
        } else if (cs->m_match == CSSSelector::PseudoClass) {
            str += ":";
            str += cs->m_value;
            if (cs->pseudoType() == PseudoNot) {
                if (CSSSelector* subSel = cs->m_simpleSelector)
                    str += subSel->selectorText();
                str += ")";
            } else if (cs->pseudoType() == PseudoLang) {
                str += cs->m_argument;
                str += ")";
            }
        } else if (cs->m_match == CSSSelector::PseudoElement) {
            str += "::";
            str += cs->m_value;
        } else if (cs->hasAttribute()) {
            str += "[";
            const AtomicString& prefix = cs->m_attr.prefix();
            if (!prefix.isNull())
                str += prefix + "|";
            str += cs->m_attr.localName();
            switch (cs->m_match) {
                case CSSSelector::Exact:
                    str += "=";
                    break;
                case CSSSelector::Set:
                    // set has no operator or value, just the attrName
                    str += "]";
                    break;
                case CSSSelector::List:
                    str += "~=";
                    break;
                case CSSSelector::Hyphen:
                    str += "|=";
                    break;
                case CSSSelector::Begin:
                    str += "^=";
                    break;
                case CSSSelector::End:
                    str += "$=";
                    break;
                case CSSSelector::Contain:
                    str += "*=";
                    break;
                default:
                    break;
            }
            if (cs->m_match != CSSSelector::Set) {
                str += "\"";
                str += cs->m_value;
                str += "\"]";
            }
        }
        if (cs->relation() != CSSSelector::SubSelector || !cs->m_tagHistory)
            break;
        cs = cs->m_tagHistory;
    }

    if (cs->m_tagHistory) {
        String tagHistoryText = cs->m_tagHistory->selectorText();
        if (cs->relation() == CSSSelector::DirectAdjacent)
            str = tagHistoryText + " + " + str;
        else if (cs->relation() == CSSSelector::IndirectAdjacent)
            str = tagHistoryText + " ~ " + str;
        else if (cs->relation() == CSSSelector::Child)
            str = tagHistoryText + " > " + str;
        else
            // Descendant
            str = tagHistoryText + " " + str;
    }

    return str;
}

} // namespace WebCore
