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
#include "misc/htmlhashes.h"
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

void StyleBaseImpl::setParsedValue(int propId, const CSSValueImpl *parsedValue,
				   bool important, bool nonCSSHint, QPtrList<CSSProperty> *propList)
{
    QPtrListIterator<CSSProperty> propIt(*propList);
    propIt.toLast(); // just remove the top one - not sure what should happen if we have multiple instances of the property
    while (propIt.current() &&
           ( propIt.current()->m_id != propId || propIt.current()->nonCSSHint != nonCSSHint ||
             propIt.current()->m_bImportant != important) )
        --propIt;
    if (propIt.current())
        propList->removeRef(propIt.current());

    CSSProperty *prop = new CSSProperty();
    prop->m_id = propId;
    prop->setValue((CSSValueImpl *) parsedValue);
    prop->m_bImportant = important;
    prop->nonCSSHint = nonCSSHint;

    propList->append(prop);
#ifdef CSS_DEBUG
    kdDebug( 6080 ) << "added property: " << getPropertyName(propId).string()
                    // non implemented yet << ", value: " << parsedValue->cssText().string()
                    << " important: " << prop->m_bImportant
                    << " nonCSS: " << prop->nonCSSHint << endl;
#endif
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
    kdDebug( 6080 ) << "[Selector: tag = " <<       tag << ", attr = \"" << attr << "\", match = \"" << match
		    << "\" value = \"" << value.string().latin1() << "\" relation = " << (int)relation
		    << "]" << endl;
    if ( tagHistory )
        tagHistory->print();
    kdDebug( 6080 ) << "    specificity = " << specificity() << endl;
}

unsigned int CSSSelector::specificity()
{
    if ( nonCSSHint )
        return 0;

    int s = ((tag == -1) ? 0 : 1);
    switch(match)
    {
    case Id:
	s += 0x10000;
	break;
    case Exact:
    case Set:
    case List:
    case Hyphen:
    case Pseudo:
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
    if (match != Pseudo)
        return;
    _pseudoType = PseudoOther;
    if (!value.isEmpty()) {
        value = value.lower();
        switch (value[0]) {
            case 'a':
                if (value == "active")
                    _pseudoType = PseudoActive;
                else if (value == "after")
                    _pseudoType = PseudoAfter;
                break;
            case 'b':
                if (value == "before")
                    _pseudoType = PseudoBefore;
                break;
            case 'e':
                if (value == "empty")
                    _pseudoType = PseudoEmpty;
                break;
            case 'f':
                if (value == "first-child")
                    _pseudoType = PseudoFirstChild;
                else if (value == "first-letter")
                    _pseudoType = PseudoFirstLetter;
                else if (value == "first-line")
                    _pseudoType = PseudoFirstLine;
                else if (value == "focus")
                    _pseudoType = PseudoFocus;
                break;
            case 'h':
                if (value == "hover")
                    _pseudoType = PseudoHover;
                break;
            case 'l':
                if (value == "link")
                    _pseudoType = PseudoLink;
                else if (value == "lang(")
                    _pseudoType = PseudoLang;
                else if (value == "last-child")
                    _pseudoType = PseudoLastChild;
                break;
            case 'n':
                if (value == "not(")
                    _pseudoType = PseudoNot;
                break;
            case 'o':
                if (value == "only-child")
                    _pseudoType = PseudoOnlyChild;
                break;
            case 'r':
                if (value == "root")
                    _pseudoType = PseudoRoot;
                break;
            case 's':
                if (value == "selection")
                    _pseudoType = PseudoSelection;
                break;
            case 't':
                if (value == "target")
                    _pseudoType = PseudoTarget;
                break;
            case 'v':
                if (value == "visited")
                    _pseudoType = PseudoVisited;
                break;
        }
    }

    value = DOMString();
}


bool CSSSelector::operator == ( const CSSSelector &other )
{
    const CSSSelector *sel1 = this;
    const CSSSelector *sel2 = &other;

    while ( sel1 && sel2 ) {
	if ( sel1->tag != sel2->tag || sel1->attr != sel2->attr ||
	     sel1->relation != sel2->relation || sel1->match != sel2->match ||
	     sel1->nonCSSHint != sel2->nonCSSHint ||
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
    DOMString str;
    const CSSSelector* cs = this;
    if ( cs->tag == -1 && cs->attr == ATTR_ID && cs->match == CSSSelector::Exact )
    {
        str = "#";
        str += cs->value;
    }
    else if ( cs->tag == -1 && cs->attr == ATTR_CLASS && cs->match == CSSSelector::List )
    {
        str = ".";
        str += cs->value;
    }
    else if ( cs->tag == -1 && cs->match == CSSSelector::Pseudo )
    {
        str = ":";
        str += cs->value;
    }
    else
    {
        if ( cs->tag == -1 )
            str = "*";
        else
            str = getTagName( cs->tag );
        if ( cs->attr == ATTR_ID && cs->match == CSSSelector::Exact )
        {
            str += "#";
            str += cs->value;
        }
        else if ( cs->attr == ATTR_CLASS && cs->match == CSSSelector::List )
        {
            str += ".";
            str += cs->value;
        }
        else if ( cs->match == CSSSelector::Pseudo )
        {
            str += ":";
            str += cs->value;
        }
        // optional attribute
        if ( cs->attr ) {
            DOMString attrName = getAttrName( cs->attr );
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
            str += cs->value;
            str += "\"]";
        }
    }
    if ( cs->tagHistory ) {
        DOMString tagHistoryText = cs->tagHistory->selectorText();
        if ( cs->relation == Sibling )
            str = tagHistoryText + " + " + str;
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
