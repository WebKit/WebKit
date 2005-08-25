/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml css code by:
    Copyright(C) 1999-2003 Lars Knoll(knoll@kde.org)
                 1999 Waldo Bastian(bastian@kde.org)
                 2001 Andreas Schlapbach(schlpbch@iam.unibe.ch)
                 2001-2003 Dirk Mueller(mueller@kde.org)
                 2002 Apple Computer, Inc.
                 2004 Allan Sandfeld Jensen (kde@carewolf.com)

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

// #define CSS_DEBUG - Prepare for heavy debug output :)

#include <kurl.h>
#include <kdebug.h>

#include "kdomcss.h"
#include "domattrs.h"
#include "KDOMCSSParser.h"
#include <kdom/css/impl/cssvalues.h>
#include "DocumentImpl.h"
#include "CDFInterface.h"
#include <kdom/css/impl/cssproperties.h>
#include "StyleBaseImpl.h"
#include "CSSStyleSheetImpl.h"
#include "CSSStyleDeclarationImpl.h"

using namespace KDOM;

StyleBaseImpl::StyleBaseImpl() : TreeShared<StyleBaseImpl>()
{
	m_parent = 0;
	m_hasInlinedDecl = false;
	m_strictParsing = true;
	m_multiLength = false;
}

StyleBaseImpl::StyleBaseImpl(StyleBaseImpl *p) : TreeShared<StyleBaseImpl>()
{
	m_parent = p;
	m_hasInlinedDecl = false;
	m_strictParsing = m_parent ? m_parent->useStrictParsing() : true;
	m_multiLength = false;
}

StyleBaseImpl::~StyleBaseImpl()
{
}

void StyleBaseImpl::checkLoaded() const
{
	if(m_parent)
		m_parent->checkLoaded();
}

void StyleBaseImpl::setParent(StyleBaseImpl *parent)
{
	m_parent = parent;
}

bool StyleBaseImpl::parseString(DOMStringImpl *, bool)
{
	return false;
}

void StyleBaseImpl::setStrictParsing(bool b)
{
	m_strictParsing = b;
}

bool StyleBaseImpl::useStrictParsing() const
{
	return m_strictParsing;
}

StyleSheetImpl *StyleBaseImpl::stylesheet()
{
	StyleBaseImpl *b = this;
	while(b && !b->isStyleSheet())
		b = b->m_parent;
		
	return static_cast<StyleSheetImpl *>(b);
}

KURL StyleBaseImpl::baseURL()
{
	// try to find the style sheet. If found look for its url.
	// If it has none, look for the parentsheet, or the parentNode and
	// try to find out about their url
	StyleSheetImpl *sheet = stylesheet();

	if(!sheet)
		return KURL();

	if(sheet->href())
		return KURL(sheet->href()->string());

	// find parent
	if(sheet->parent())
		return sheet->parent()->baseURL();

	if(!sheet->ownerNode())
		return KURL();

	return sheet->ownerNode()->ownerDocument()->documentKURI();
}

void StyleBaseImpl::setParsedValue(int propId, const CSSValueImpl *parsedValue, bool important, bool nonCSSHint, QPtrList<CSSProperty> *propList)
{
	QPtrListIterator<CSSProperty> propIt(*propList);
	propIt.toLast(); // just remove the top one - not sure what should happen if we have multiple instances of the property

	while(propIt.current() &&
		  (propIt.current()->m_id != propId || propIt.current()->m_nonCSSHint != nonCSSHint ||
		  propIt.current()->m_important != important))
		--propIt;

	if(propIt.current())
		propList->removeRef(propIt.current());

	CSSProperty *prop = new CSSProperty();
	prop->m_id = propId;
	prop->setValue(const_cast<CSSValueImpl *>(parsedValue));

	prop->m_important = important;
	prop->m_nonCSSHint = nonCSSHint;

	propList->append(prop);

#ifdef CSS_DEBUG
	kdDebug(6080) << "added property: " << getPropertyName(propId).string()
	// non implemented yet << ", value: " << parsedValue->cssText().string()
					<< " important: " << prop->m_important
					<< " nonCSS: " << prop->m_nonCSSHint << endl;
#endif
}

CSSParser *StyleBaseImpl::createCSSParser(bool strictParsing) const
{
	return new CSSParser(strictParsing);
}

StyleListImpl::StyleListImpl() : StyleBaseImpl()
{
	m_lstChildren = 0;
}

StyleListImpl::StyleListImpl(StyleBaseImpl *parent) : StyleBaseImpl(parent)
{
	m_lstChildren = 0;
}

StyleListImpl::~StyleListImpl()
{
	StyleBaseImpl *n;

	if(!m_lstChildren)
		return;

	for(n = m_lstChildren->first(); n != 0; n = m_lstChildren->next())
	{
		n->setParent(0);

		if(!n->refCount())
			delete n;
	}

	delete m_lstChildren;
}

unsigned long StyleListImpl::length() const
{
	return m_lstChildren->count();
}

StyleBaseImpl *StyleListImpl::item(unsigned long num) const
{
	return m_lstChildren->at(num);
}

void StyleListImpl::append(StyleBaseImpl *item)
{
	m_lstChildren->append(item);
}

CSSSelector::CSSSelector(CDFInterface *interface) : value(0), tagHistory(0), simpleSelector(0), string_arg(0),
													attr(0), tag(anyQName), relation(Descendant), match(None),
													nonCSSHint(false), pseudoId(0), _pseudoType(PseudoNotParsed),
													m_interface(interface)
{
}

CSSSelector::~CSSSelector()
{
	if(value)
		value->deref();

	if(string_arg)
		string_arg->deref();

	delete tagHistory;
	delete simpleSelector;
}

void CSSSelector::print()
{
#ifndef APPLE_CHANGES
	kdDebug(6080) << "[Selector: tag = " << QString::number(tag, 16)
				  << ", attr = \"" << attr << "\", match = \"" << match
				  << "\" value = \"" << DOMString(value).string().latin1()
				  << "\" relation = " << (int) relation	<< "]" << endl;
#endif

	if(tagHistory)
		tagHistory->print();

	kdDebug(6080) << "    specificity = " << specificity() << endl;
}

unsigned int CSSSelector::specificity() const
{
	if(nonCSSHint)
		return 0;

	int s = ((localNamePart(tag) == anyLocalName) ? 0 : 1);
	switch(match)
	{
		case Id:
		{
			s += 0x10000;
			break;
		}
		case Exact:
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

CSSSelector::PseudoType CSSSelector::pseudoType() const
{
	if(_pseudoType == PseudoNotParsed)
		extractPseudoType();

	return _pseudoType;
}

void CSSSelector::extractPseudoType() const
{
	if(match != PseudoClass && match != PseudoElement)
		return;

	_pseudoType = PseudoOther;

	bool compat = false;
	bool element = false;
	if(value && !value->isEmpty())
	{
		value = value->lower();
		DOMString _value(value);
		switch(_value[0].latin1())
		{
			case 'a':
			{
				if(_value =="active")
					_pseudoType = PseudoActive;
				else if(_value =="after")
				{
					_pseudoType = PseudoAfter;
					element = compat = true;
				}

				break;
			}
			case 'b':
			{
				if(_value =="before")
				{
					_pseudoType = PseudoBefore;
					element = compat = true;
				}

				break;
			}
			case 'c':
			{
				if(_value =="checked")
					_pseudoType = PseudoChecked;
				else if(_value =="contains(")
					_pseudoType = PseudoContains;

				break;
			}
			case 'd':
			{
				if(_value =="disabled")
					_pseudoType = PseudoDisabled;

				break;
			}
			case 'e':
			{
				if(_value =="empty")
					_pseudoType = PseudoEmpty;
				else if(_value =="enabled")
					_pseudoType = PseudoEnabled;

				break;
			}
			case 'f':
			{
				if(_value =="first-child")
					_pseudoType = PseudoFirstChild;
				else if(_value =="first-letter")
				{
					_pseudoType = PseudoFirstLetter;
					element = compat = true;
				}
				else if(_value =="first-line")
				{
					_pseudoType = PseudoFirstLine;
					element = compat = true;
				}
				else if(_value =="first-of-type")
					_pseudoType = PseudoFirstOfType;
				else if(_value =="focus")
					_pseudoType = PseudoFocus;

				break;
			}
			case 'h':
			{
				if(_value =="hover")
					_pseudoType = PseudoHover;

				break;
			}
			case 'i':
			{
				if(_value =="indeterminate")
					_pseudoType = PseudoIndeterminate;

				break;
			}
			case 'l':
			{
				if(_value =="link")
					_pseudoType = PseudoLink;
				else if(_value =="lang(")
					_pseudoType = PseudoLang;
				else if(_value =="last-child")
					_pseudoType = PseudoLastChild;
				else if(_value =="last-of-type")
					_pseudoType = PseudoLastOfType;

				break;
			}
			case 'n':
			{
				if(_value =="not(")
					_pseudoType = PseudoNot;
				else if(_value =="nth-child(")
					_pseudoType = PseudoNthChild;
				else if(_value =="nth-last-child(")
					_pseudoType = PseudoNthLastChild;
				else if(_value =="nth-of-type(")
					_pseudoType = PseudoNthOfType;
				else if(_value =="nth-last-of-type(")
					_pseudoType = PseudoNthLastOfType;
				break;
			}
			case 'o':
			{
				if(_value =="only-child")
					_pseudoType = PseudoOnlyChild;
				else if(_value =="only-of-type")
					_pseudoType = PseudoOnlyOfType;

				break;
			}
			case 'r':
			{
				if(_value =="root")
					_pseudoType = PseudoRoot;

				break;
			}
			case 's':
			{
				if(_value =="selection")
				{
					_pseudoType = PseudoSelection;
					element = true;
				}

				break;
			}
			case 't':
			{
				if(_value =="target")
					_pseudoType = PseudoTarget;

				break;
			}
			case 'v':
			{
				if(_value =="visited")
					_pseudoType = PseudoVisited;

				break;
			}
		}
	}

	if(match == PseudoClass && element)
	{
		if(!compat)
			_pseudoType = PseudoOther;
		else
			match = PseudoElement;
	}
	else if(match == PseudoElement && !element)
		_pseudoType = PseudoOther;

	value = 0;
}

bool CSSSelector::operator==(const CSSSelector &other) const
{
	const CSSSelector *sel1 = this;
	const CSSSelector *sel2 = &other;

	while(sel1 && sel2)
	{
		if(sel1->tag != sel2->tag || sel1->attr != sel2->attr ||
		   sel1->relation != sel2->relation || sel1->match != sel2->match ||
		   sel1->nonCSSHint != sel2->nonCSSHint ||
		   DOMString(sel1->value) != DOMString(sel2->value) ||
		   sel1->pseudoType() != sel2->pseudoType() ||
		   DOMString(sel1->string_arg) != DOMString(sel2->string_arg))
		{
			return false;
		}
				
		sel1 = sel1->tagHistory;
		sel2 = sel2->tagHistory;
	}
	
	if(sel1 || sel2)
		return false;

	return true;
}

DOMStringImpl *CSSSelector::selectorText() const
{
	// FIXME: Support namespaces when dumping the selector text.  This requires preserving
	// the original namespace prefix used. Ugh. -dwh
	DOMString str;
	const CSSSelector *cs = this;
	Q_UINT16 tag = localNamePart(cs->tag);
	if(tag == anyLocalName && cs->attr == ATTR_ID && cs->match == CSSSelector::Id)
	{
		str = "#";
		str += DOMString(cs->value);
	}
	else if(tag == anyLocalName && cs->attr == ATTR_CLASS && cs->match == CSSSelector::List)
	{
		str = ".";
		str += DOMString(cs->value);
	}
	else if(tag == anyLocalName && cs->match == CSSSelector::PseudoClass)
	{
		str = ":";
		str += DOMString(cs->value);
	}
	else if(tag == anyLocalName && cs->match == CSSSelector::PseudoElement)
	{
		str = "::";
		str += DOMString(cs->value);
	}
	else
	{
		if(tag == anyLocalName)
			str = "*";
		else if(tag != anyLocalName)
			str = m_interface->getTagName(cs->tag);

		if(cs->attr == ATTR_ID && cs->match == CSSSelector::Id)
		{
			str += "#";
			str += DOMString(cs->value);
		}
		else if(cs->attr == ATTR_CLASS && cs->match == CSSSelector::List)
		{
			str += ".";
			str += DOMString(cs->value);
		}
		else if(cs->match == CSSSelector::PseudoClass)
		{
			str += ":";
			str += DOMString(cs->value);
		}
		else if(cs->match == CSSSelector::PseudoElement)
		{
			str += "::";
			str += DOMString(cs->value);
		}
		else if(cs->attr) // optional attribute
		{
			DOMString attrName = m_interface->getAttrName(cs->attr);
			str += "[";
			str += attrName;
			switch(cs->match)
			{
				case CSSSelector::Exact:
				{
					str += "=";
					break;
				}
				case CSSSelector::Set:
				{
					str += " "; // ## correct?
					break;
				}
				case CSSSelector::List:
				{
					str += "~=";
					break;
				}
				case CSSSelector::Hyphen:
				{
					str += "|=";
					break;
				}
				case CSSSelector::Begin:
				{
					str += "^=";
					break;
				}
				case CSSSelector::End:
				{
					str += "$=";
					break;
				}
				case CSSSelector::Contain:
				{
					str += "*=";
					break;
				}
				default:
					kdWarning(6080) << "Unhandled case in CSSStyleRuleImpl::selectorText : match=" << cs->match << endl;
			}

			str += "\"";
			str += DOMString(cs->value);
			str += "\"]";
		}
	}

	if(cs->tagHistory)
	{
		DOMString tagHistoryText(cs->tagHistory->selectorText());
		if(cs->relation == DirectAdjacent)
			str = tagHistoryText + " + " + str;
		else if(cs->relation == IndirectAdjacent)
			str = tagHistoryText + " ~ " + str;
		else if(cs->relation == Child)
			str = tagHistoryText + " > " + str;
		else if(cs->relation == SubSelector)
			str += tagHistoryText; // the ":" is provided by selectorText()
		else // Descendant
			str = tagHistoryText + " " + str;
	}

	return str.handle()->copy();
}

// vim:ts=4:noet
