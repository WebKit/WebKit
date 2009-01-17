/*
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 *               2001 Andreas Schlapbach (schlpbch@iam.unibe.ch)
 *               2001-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Smith (catfish.man@gmail.com)
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

#include "wtf/Assertions.h"
#include "HTMLNames.h"

#include <wtf/StdLibExtras.h>

namespace WebCore {
    
using namespace HTMLNames;

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

    if (CSSSelector* tagHistory = this->tagHistory())
        s += tagHistory->specificity();

    // make sure it doesn't overflow
    return s & 0xffffff;
}

void CSSSelector::extractPseudoType() const
{
    if (m_match != PseudoClass && m_match != PseudoElement)
        return;

    DEFINE_STATIC_LOCAL(AtomicString, active, ("active"));
    DEFINE_STATIC_LOCAL(AtomicString, after, ("after"));
    DEFINE_STATIC_LOCAL(AtomicString, anyLink, ("-webkit-any-link"));
    DEFINE_STATIC_LOCAL(AtomicString, autofill, ("-webkit-autofill"));
    DEFINE_STATIC_LOCAL(AtomicString, before, ("before"));
    DEFINE_STATIC_LOCAL(AtomicString, checked, ("checked"));
    DEFINE_STATIC_LOCAL(AtomicString, fileUploadButton, ("-webkit-file-upload-button"));
    DEFINE_STATIC_LOCAL(AtomicString, disabled, ("disabled"));
    DEFINE_STATIC_LOCAL(AtomicString, readOnly, ("read-only"));
    DEFINE_STATIC_LOCAL(AtomicString, readWrite, ("read-write"));
    DEFINE_STATIC_LOCAL(AtomicString, drag, ("-webkit-drag"));
    DEFINE_STATIC_LOCAL(AtomicString, dragAlias, ("-khtml-drag")); // was documented with this name in Apple documentation, so keep an alia
    DEFINE_STATIC_LOCAL(AtomicString, empty, ("empty"));
    DEFINE_STATIC_LOCAL(AtomicString, enabled, ("enabled"));
    DEFINE_STATIC_LOCAL(AtomicString, firstChild, ("first-child"));
    DEFINE_STATIC_LOCAL(AtomicString, firstLetter, ("first-letter"));
    DEFINE_STATIC_LOCAL(AtomicString, firstLine, ("first-line"));
    DEFINE_STATIC_LOCAL(AtomicString, firstOfType, ("first-of-type"));
    DEFINE_STATIC_LOCAL(AtomicString, fullPageMedia, ("-webkit-full-page-media"));
    DEFINE_STATIC_LOCAL(AtomicString, nthChild, ("nth-child("));
    DEFINE_STATIC_LOCAL(AtomicString, nthOfType, ("nth-of-type("));
    DEFINE_STATIC_LOCAL(AtomicString, nthLastChild, ("nth-last-child("));
    DEFINE_STATIC_LOCAL(AtomicString, nthLastOfType, ("nth-last-of-type("));
    DEFINE_STATIC_LOCAL(AtomicString, focus, ("focus"));
    DEFINE_STATIC_LOCAL(AtomicString, hover, ("hover"));
    DEFINE_STATIC_LOCAL(AtomicString, indeterminate, ("indeterminate"));
    DEFINE_STATIC_LOCAL(AtomicString, inputPlaceholder, ("-webkit-input-placeholder"));
    DEFINE_STATIC_LOCAL(AtomicString, lastChild, ("last-child"));
    DEFINE_STATIC_LOCAL(AtomicString, lastOfType, ("last-of-type"));
    DEFINE_STATIC_LOCAL(AtomicString, link, ("link"));
    DEFINE_STATIC_LOCAL(AtomicString, lang, ("lang("));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsPanel, ("-webkit-media-controls-panel"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsMuteButton, ("-webkit-media-controls-mute-button"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsPlayButton, ("-webkit-media-controls-play-button"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsTimeline, ("-webkit-media-controls-timeline"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsSeekBackButton, ("-webkit-media-controls-seek-back-button"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsSeekForwardButton, ("-webkit-media-controls-seek-forward-button"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsFullscreenButton, ("-webkit-media-controls-fullscreen-button"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsTimelineContainer, ("-webkit-media-controls-timeline-container"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsCurrentTimeDisplay, ("-webkit-media-controls-current-time-display"));
    DEFINE_STATIC_LOCAL(AtomicString, mediaControlsTimeRemainingDisplay, ("-webkit-media-controls-time-remaining-display"));
    DEFINE_STATIC_LOCAL(AtomicString, notStr, ("not("));
    DEFINE_STATIC_LOCAL(AtomicString, onlyChild, ("only-child"));
    DEFINE_STATIC_LOCAL(AtomicString, onlyOfType, ("only-of-type"));
    DEFINE_STATIC_LOCAL(AtomicString, resizer, ("-webkit-resizer"));
    DEFINE_STATIC_LOCAL(AtomicString, root, ("root"));
    DEFINE_STATIC_LOCAL(AtomicString, scrollbar, ("-webkit-scrollbar"));
    DEFINE_STATIC_LOCAL(AtomicString, scrollbarButton, ("-webkit-scrollbar-button"));
    DEFINE_STATIC_LOCAL(AtomicString, scrollbarCorner, ("-webkit-scrollbar-corner"));
    DEFINE_STATIC_LOCAL(AtomicString, scrollbarThumb, ("-webkit-scrollbar-thumb"));
    DEFINE_STATIC_LOCAL(AtomicString, scrollbarTrack, ("-webkit-scrollbar-track"));
    DEFINE_STATIC_LOCAL(AtomicString, scrollbarTrackPiece, ("-webkit-scrollbar-track-piece"));
    DEFINE_STATIC_LOCAL(AtomicString, searchCancelButton, ("-webkit-search-cancel-button"));
    DEFINE_STATIC_LOCAL(AtomicString, searchDecoration, ("-webkit-search-decoration"));
    DEFINE_STATIC_LOCAL(AtomicString, searchResultsDecoration, ("-webkit-search-results-decoration"));
    DEFINE_STATIC_LOCAL(AtomicString, searchResultsButton, ("-webkit-search-results-button"));
    DEFINE_STATIC_LOCAL(AtomicString, selection, ("selection"));
    DEFINE_STATIC_LOCAL(AtomicString, sliderThumb, ("-webkit-slider-thumb"));
    DEFINE_STATIC_LOCAL(AtomicString, target, ("target"));
    DEFINE_STATIC_LOCAL(AtomicString, visited, ("visited"));
    DEFINE_STATIC_LOCAL(AtomicString, windowInactive, ("window-inactive"));
    DEFINE_STATIC_LOCAL(AtomicString, decrement, ("decrement"));
    DEFINE_STATIC_LOCAL(AtomicString, increment, ("increment"));
    DEFINE_STATIC_LOCAL(AtomicString, start, ("start"));
    DEFINE_STATIC_LOCAL(AtomicString, end, ("end"));
    DEFINE_STATIC_LOCAL(AtomicString, horizontal, ("horizontal"));
    DEFINE_STATIC_LOCAL(AtomicString, vertical, ("vertical"));
    DEFINE_STATIC_LOCAL(AtomicString, doubleButton, ("double-button"));
    DEFINE_STATIC_LOCAL(AtomicString, singleButton, ("single-button"));
    DEFINE_STATIC_LOCAL(AtomicString, noButton, ("no-button"));
    DEFINE_STATIC_LOCAL(AtomicString, cornerPresent, ("corner-present"));

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
    else if (m_value == readOnly)
        m_pseudoType = PseudoReadOnly;
    else if (m_value == readWrite)
        m_pseudoType = PseudoReadWrite;
    else if (m_value == drag || m_value == dragAlias)
        m_pseudoType = PseudoDrag;
    else if (m_value == enabled)
        m_pseudoType = PseudoEnabled;
    else if (m_value == empty)
        m_pseudoType = PseudoEmpty;
    else if (m_value == firstChild)
        m_pseudoType = PseudoFirstChild;
    else if (m_value == fullPageMedia)
        m_pseudoType = PseudoFullPageMedia;
    else if (m_value == inputPlaceholder) {
        m_pseudoType = PseudoInputPlaceholder;
        element = true;
    } else if (m_value == lastChild)
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
    } else if (m_value == mediaControlsCurrentTimeDisplay) {
        m_pseudoType = PseudoMediaControlsCurrentTimeDisplay;
        element = true;
    } else if (m_value == mediaControlsTimeRemainingDisplay) {
        m_pseudoType = PseudoMediaControlsTimeRemainingDisplay;
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
    } else if (m_value == mediaControlsTimelineContainer) {
        m_pseudoType = PseudoMediaControlsTimelineContainer;
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
    else if (m_value == windowInactive)
        m_pseudoType = PseudoWindowInactive;
    else if (m_value == decrement)
        m_pseudoType = PseudoDecrement;
    else if (m_value == increment)
        m_pseudoType = PseudoIncrement;
    else if (m_value == start)
        m_pseudoType = PseudoStart;
    else if (m_value == end)
        m_pseudoType = PseudoEnd;
    else if (m_value == horizontal)
        m_pseudoType = PseudoHorizontal;
    else if (m_value == vertical)
        m_pseudoType = PseudoVertical;
    else if (m_value == doubleButton)
        m_pseudoType = PseudoDoubleButton;
    else if (m_value == singleButton)
        m_pseudoType = PseudoSingleButton;
    else if (m_value == noButton)
        m_pseudoType = PseudoNoButton;
    else if (m_value == scrollbarCorner) {
        element = true;
        m_pseudoType = PseudoScrollbarCorner;
    } else if (m_value == resizer) {
        element = true;
        m_pseudoType = PseudoResizer;
    } else if (m_value == scrollbar) {
        element = true;
        m_pseudoType = PseudoScrollbar;
    } else if (m_value == scrollbarButton) {
        element = true;
        m_pseudoType = PseudoScrollbarButton;
    } else if (m_value == scrollbarCorner) {
        element = true;
        m_pseudoType = PseudoScrollbarCorner;
    } else if (m_value == scrollbarThumb) {
        element = true;
        m_pseudoType = PseudoScrollbarThumb;
    } else if (m_value == scrollbarTrack) {
        element = true;
        m_pseudoType = PseudoScrollbarTrack;
    } else if (m_value == scrollbarTrackPiece) {
        element = true;
        m_pseudoType = PseudoScrollbarTrackPiece;
    } else if (m_value == cornerPresent)
         m_pseudoType = PseudoCornerPresent;
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
        if (sel1->m_tag != sel2->m_tag || sel1->attribute() != sel2->attribute() ||
             sel1->relation() != sel2->relation() || sel1->m_match != sel2->m_match ||
             sel1->m_value != sel2->m_value ||
             sel1->pseudoType() != sel2->pseudoType() ||
             sel1->argument() != sel2->argument())
            return false;
        sel1 = sel1->tagHistory();
        sel2 = sel2->tagHistory();
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
                if (CSSSelector* subSel = cs->simpleSelector())
                    str += subSel->selectorText();
                str += ")";
            } else if (cs->pseudoType() == PseudoLang
                    || cs->pseudoType() == PseudoNthChild
                    || cs->pseudoType() == PseudoNthLastChild
                    || cs->pseudoType() == PseudoNthOfType
                    || cs->pseudoType() == PseudoNthLastOfType) {
                str += cs->argument();
                str += ")";
            }
        } else if (cs->m_match == CSSSelector::PseudoElement) {
            str += "::";
            str += cs->m_value;
        } else if (cs->hasAttribute()) {
            str += "[";
            const AtomicString& prefix = cs->attribute().prefix();
            if (!prefix.isNull())
                str += prefix + "|";
            str += cs->attribute().localName();
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
        if (cs->relation() != CSSSelector::SubSelector || !cs->tagHistory())
            break;
        cs = cs->tagHistory();
    }

    if (CSSSelector* tagHistory = cs->tagHistory()) {
        String tagHistoryText = tagHistory->selectorText();
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
    
void CSSSelector::setTagHistory(CSSSelector* tagHistory) 
{ 
    if (m_hasRareData) 
        m_data.m_rareData->m_tagHistory.set(tagHistory); 
    else 
        m_data.m_tagHistory = tagHistory; 
}

const QualifiedName& CSSSelector::attribute() const
{ 
    switch (m_match) {
    case Id:
        return idAttr;
    case Class:
        return classAttr;
    default:
        return m_hasRareData ? m_data.m_rareData->m_attribute : anyQName();
    }
}

void CSSSelector::setAttribute(const QualifiedName& value) 
{ 
    createRareData(); 
    m_data.m_rareData->m_attribute = value; 
}
    
void CSSSelector::setArgument(const AtomicString& value) 
{ 
    createRareData(); 
    m_data.m_rareData->m_argument = value; 
}

void CSSSelector::setSimpleSelector(CSSSelector* value)
{
    createRareData(); 
    m_data.m_rareData->m_simpleSelector.set(value); 
}

bool CSSSelector::parseNth()
{
    if (!m_hasRareData)
        return false;
    if (m_parsedNth)
        return true;
    m_parsedNth = m_data.m_rareData->parseNth();
    return m_parsedNth;
}

bool CSSSelector::matchNth(int count)
{
    ASSERT(m_hasRareData);
    return m_data.m_rareData->matchNth(count);
}

// a helper function for parsing nth-arguments
bool CSSSelector::RareData::parseNth()
{    
    const String& argument = m_argument;
    
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
        int n = argument.find('n');
        if (n != -1) {
            if (argument[0] == '-') {
                if (n == 1)
                    m_a = -1; // -n == -1n
                else
                    m_a = argument.substring(0, n).toInt();
            } else if (!n)
                m_a = 1; // n == 1n
            else
                m_a = argument.substring(0, n).toInt();
            
            int p = argument.find('+', n);
            if (p != -1)
                m_b = argument.substring(p + 1, argument.length() - p - 1).toInt();
            else {
                p = argument.find('-', n);
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
