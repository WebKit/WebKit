/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 *               1999 Waldo Bastian (bastian@kde.org)
 *               2001 Andreas Schlapbach (schlpbch@iam.unibe.ch)
 *               2001-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

//#define CSS_DEBUG

#include "kdebug.h"

#include "css_base.h"

#ifdef CSS_DEBUG
#include "cssproperties.h"
#endif

#include "css_stylesheetimpl.h"
#include "xml/dom_docimpl.h"
#include "css_valueimpl.h"
using namespace DOM;

void StyleBaseImpl::checkLoaded()
{
    if(m_parent) m_parent->checkLoaded();
}

StyleSheetImpl* StyleBaseImpl::stylesheet()
{
    StyleBaseImpl *b = this;
    while(b && !b->isStyleSheet())
        b = b->m_parent;
    return static_cast<StyleSheetImpl *>(b);
}

DOMString StyleBaseImpl::baseURL()
{
    // try to find the style sheet. If found look for its url.
    // If it has none, look for the parentsheet, or the parentNode and
    // try to find out about their url

    StyleSheetImpl *sheet = stylesheet();

    if(!sheet) return DOMString();

    if(!sheet->href().isNull())
        return sheet->href();

    // find parent
    if(sheet->parent()) return sheet->parent()->baseURL();

    if(!sheet->ownerNode()) return DOMString();

    DocumentImpl *doc = sheet->ownerNode()->getDocument();

    return doc->baseURL();
}

// ------------------------------------------------------------------------------

StyleListImpl::~StyleListImpl()
{
    StyleBaseImpl *n;

    if(!m_lstChildren) return;

    for( n = m_lstChildren->first(); n != 0; n = m_lstChildren->next() )
    {
        n->setParent(0);
        if( !n->refCount() ) delete n;
    }
    delete m_lstChildren;
}

// --------------------------------------------------------------------------------

void CSSSelector::print(void)
{
    kdDebug( 6080 ) << "[Selector: tag = " <<       tag.localName().string() << ", attr = \"" << attr.localName().string() << "\", match = \"" << match
		    << "\" value = \"" << value.string().latin1() << "\" relation = " << (int)relation
		    << "]" << endl;
    if ( tagHistory )
        tagHistory->print();
    kdDebug( 6080 ) << "    specificity = " << specificity() << endl;
}

unsigned int CSSSelector::specificity()
{
    // FIXME: Pseudo-elements and pseudo-classes do not have the same specificity. This function
    // isn't quite correct.
    int s = (tag.localName() == starAtom ? 0 : 1);
    switch(match)
    {
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
    if(tagHistory)
        s += tagHistory->specificity();
    // make sure it doesn't overflow
    return s & 0xffffff;
}

void CSSSelector::extractPseudoType() const
{
    if (match != PseudoClass && match != PseudoElement)
        return;
    
    static AtomicString active("active");
    static AtomicString after("after");
    static AtomicString anyLink("-khtml-any-link");
    static AtomicString before("before");
    static AtomicString drag("-khtml-drag");
    static AtomicString empty("empty");
    static AtomicString firstChild("first-child");
    static AtomicString firstLetter("first-letter");
    static AtomicString firstLine("first-line");
    static AtomicString focus("focus");
    static AtomicString hover("hover");
    static AtomicString link("link");
    static AtomicString lang("lang(");
    static AtomicString lastChild("last-child");
    static AtomicString notStr("not(");
    static AtomicString onlyChild("only-child");
    static AtomicString root("root");
    static AtomicString selection("selection");
    static AtomicString target("target");
    static AtomicString visited("visited");
    bool element = false;	// pseudo-element
    bool compat = false;	// single colon compatbility mode
    
    _pseudoType = PseudoOther;
    if (value == active)
        _pseudoType = PseudoActive;
    else if (value == after) {
        _pseudoType = PseudoAfter;
        element = compat = true;
    } else if (value == anyLink)
        _pseudoType = PseudoAnyLink;
    else if (value == before) {
        _pseudoType = PseudoBefore;
        element = compat = true;
    } else if (value == drag)
        _pseudoType = PseudoDrag;
    else if (value == empty)
        _pseudoType = PseudoEmpty;
    else if (value == firstChild)
        _pseudoType = PseudoFirstChild;
    else if (value == firstLetter) {
        _pseudoType = PseudoFirstLetter;
        element = compat = true;
    } else if (value == firstLine) {
        _pseudoType = PseudoFirstLine;
        element = compat = true;
    } else if (value == focus)
        _pseudoType = PseudoFocus;
    else if (value == hover)
        _pseudoType = PseudoHover;
    else if (value == link)
        _pseudoType = PseudoLink;
    else if (value == lang)
        _pseudoType = PseudoLang;
    else if (value == lastChild)
        _pseudoType = PseudoLastChild;
    else if (value == notStr)
        _pseudoType = PseudoNot;
    else if (value == onlyChild)
        _pseudoType = PseudoOnlyChild;
    else if (value == root)
        _pseudoType = PseudoRoot;
    else if (value == selection) {
        _pseudoType = PseudoSelection;
        element = true;
    } else if (value == target)
        _pseudoType = PseudoTarget;
    else if (value == visited)
        _pseudoType = PseudoVisited;
    
    if (match == PseudoClass && element)
        if (!compat) 
            _pseudoType = PseudoOther;
        else 
           match = PseudoElement;
    else if (match == PseudoElement && !element)
        _pseudoType = PseudoOther;
    value = nullAtom;
}


bool CSSSelector::operator == ( const CSSSelector &other )
{
    const CSSSelector *sel1 = this;
    const CSSSelector *sel2 = &other;

    while ( sel1 && sel2 ) {
	if ( sel1->tag != sel2->tag || sel1->attr != sel2->attr ||
	     sel1->relation != sel2->relation || sel1->match != sel2->match ||
	     sel1->value != sel2->value ||
             sel1->pseudoType() != sel2->pseudoType())
	    return false;
	sel1 = sel1->tagHistory;
	sel2 = sel2->tagHistory;
    }
    if ( sel1 || sel2 )
	return false;
    return true;
}

DOMString CSSSelector::selectorText() const
{
    // FIXME: Support namespaces when dumping the selector text. -dwh
    DOMString str;
    const CSSSelector* cs = this;
    const AtomicString& localName = cs->tag.localName();
    if (localName == starAtom && cs->match == CSSSelector::Id)
    {
        str = "#";
        str += cs->value.string();
    }
    else if (localName == starAtom && cs->match == CSSSelector::Class)
    {
        str = ".";
        str += cs->value.string();
    }
    else if (localName == starAtom  && cs->match == CSSSelector::PseudoClass)
    {
        str = ":";
        str += cs->value.string();
    }
    else if (localName == starAtom && cs->match == CSSSelector::PseudoElement)
    {
        str = "::";
        str += cs->value.string();
    }
    else
    {
        if (localName == starAtom)
            str = "*";
        else
            str = localName;
        if (cs->match == CSSSelector::Id)
        {
            str += "#";
            str += cs->value.string();
        }
        else if (cs->match == CSSSelector::Class)
        {
            str += ".";
            str += cs->value.string();
        }
        else if (cs->match == CSSSelector::PseudoClass)
        {
            str += ":";
            str += cs->value.string();
        }
        else if (cs->match == CSSSelector::PseudoElement)
        {
            str += "::";
            str += cs->value.string();
        }
        // optional attribute
        if (cs->hasAttribute()) {
            // FIXME: Add support for dumping namespaces.
            DOMString attrName = cs->attr.localName();
            str += "[";
            str += attrName;
            switch (cs->match) {
            case CSSSelector::Exact:
                str += "=";
                break;
            case CSSSelector::Set:
                str += " "; /// ## correct?
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
                kdWarning(6080) << "Unhandled case in CSSStyleRuleImpl::selectorText : match=" << cs->match << endl;
            }
            str += "\"";
            str += cs->value.string();
            str += "\"]";
        }
    }
    if ( cs->tagHistory ) {
        DOMString tagHistoryText = cs->tagHistory->selectorText();
		if ( cs->relation == DirectAdjacent )
            str = tagHistoryText + " + " + str;
        else if ( cs->relation == IndirectAdjacent )
            str = tagHistoryText + " ~ " + str;
        else if ( cs->relation == Child )
            str = tagHistoryText + " > " + str;
        else if ( cs->relation == SubSelector )
            str += tagHistoryText; // the ":" is provided by selectorText()
        else // Descendant
            str = tagHistoryText + " " + str;
    }
    return str;
}

// ----------------------------------------------------------------------------
