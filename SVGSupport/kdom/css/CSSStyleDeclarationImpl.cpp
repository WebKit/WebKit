/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

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

#include "DOMString.h"
#include "KDOMCSSParser.h"
#include "CSSRuleImpl.h"
#include "DocumentImpl.h"
#include "CSSValueImpl.h"
#include "CDFInterface.h"
#include <kdom/css/impl/cssproperties.h>
#include "CSSStyleSheetImpl.h"
#include "CSSPrimitiveValueImpl.h"
#include "CSSStyleDeclarationImpl.h"

using namespace KDOM;

CSSStyleDeclarationImpl::CSSStyleDeclarationImpl(CDFInterface *interface, CSSRuleImpl *parent)
: StyleBaseImpl(parent)
{
	m_node = 0;
	m_lstValues = 0;
	m_interface = interface;
}

CSSStyleDeclarationImpl::CSSStyleDeclarationImpl(CDFInterface *interface, CSSRuleImpl *parent, QPtrList<CSSProperty> *lstValues)
: StyleBaseImpl(parent)
{
	m_node = 0;
	m_lstValues = lstValues;
	m_interface = interface;
}

CSSStyleDeclarationImpl::~CSSStyleDeclarationImpl()
{
	delete m_lstValues;

	// Important! We don't use refcounting for m_node,
	// to avoid cyclic references (see ElementImpl)...
}

CSSStyleDeclarationImpl &CSSStyleDeclarationImpl::operator=(const CSSStyleDeclarationImpl& o)
{
	// don't attach it to the same node, just leave the current m_node value
	delete m_lstValues;
	m_lstValues = 0;

	if(o.m_lstValues)
	{
		m_lstValues = new QPtrList<CSSProperty>();
		m_lstValues->setAutoDelete(true);

		QPtrListIterator<CSSProperty> lstValuesIt(*o.m_lstValues);
		for(lstValuesIt.toFirst(); lstValuesIt.current(); ++lstValuesIt)
			m_lstValues->append(new CSSProperty(*lstValuesIt.current()));
	}

	return *this;
}

DOMStringImpl *CSSStyleDeclarationImpl::cssText() const
{
	if(!m_lstValues)
		return 0;

	DOMStringImpl *result = new DOMStringImpl();

	QPtrListIterator<CSSProperty> lstValuesIt(*m_lstValues);
		
	CSSProperty *current;
	for(lstValuesIt.toFirst(); (current = lstValuesIt.current()); ++lstValuesIt)
		result->append(current->cssText(*this));

	return result;
}

void CSSStyleDeclarationImpl::setCssText(DOMStringImpl *cssText)
{
	if(m_lstValues)
		m_lstValues->clear();
	else
	{
		m_lstValues = new QPtrList<CSSProperty>();
		m_lstValues->setAutoDelete(true);
	}

	CSSParser *parser = createCSSParser(m_strictParsing);
	if(!parser)
		return;
	
	parser->parseDeclaration(this, cssText, false);
	setChanged();
	delete parser;
}

DOMStringImpl *CSSStyleDeclarationImpl::get4Values(const int *properties) const
{
	DOMStringImpl *ret = 0;
	for(int i = 0; i < 4; ++i)
	{
		CSSValueImpl *value = getPropertyCSSValue(properties[i]);
		if(!value) // apparently all 4 properties must be specified.
			return 0;

		if(ret)
			ret->append(" ");

		if(!ret)
			ret = new DOMStringImpl();

		ret->append(value->cssText());
	}

	return ret;
}

DOMStringImpl *CSSStyleDeclarationImpl::getShortHandValue(const int *properties, int number) const
{
	DOMStringImpl *ret = 0;
	for(int i = 0; i < number; ++i)
	{
		CSSValueImpl *value = getPropertyCSSValue(properties[i]);
		if(value)
		{ // TODO provide default value if !value
			if(ret)
				ret->append(" ");

			if(!ret)
				ret = new DOMStringImpl();
			
			ret->append(value->cssText());
		}
	}

	return ret;
}

DOMStringImpl *CSSStyleDeclarationImpl::getPropertyValue(int propertyID) const
{
	if(!m_lstValues)
		return 0;
		
	CSSValueImpl *value = getPropertyCSSValue(propertyID);
	if(value)
		return value->cssText();

	// Shorthand and 4-values properties
	switch(propertyID)
	{
	case CSS_PROP_BACKGROUND_POSITION:
	{
		// ## Is this correct? The code in cssparser.cpp is confusing
		const int properties[2] = { CSS_PROP_BACKGROUND_POSITION_X,
									CSS_PROP_BACKGROUND_POSITION_Y };
		return getShortHandValue(properties, 2);
	}
	case CSS_PROP_BACKGROUND:
	{
		const int properties[5] = { CSS_PROP_BACKGROUND_IMAGE,
									CSS_PROP_BACKGROUND_REPEAT,
									CSS_PROP_BACKGROUND_ATTACHMENT,
									CSS_PROP_BACKGROUND_POSITION,
									CSS_PROP_BACKGROUND_COLOR };
		return getShortHandValue(properties, 5);
	}
	case CSS_PROP_BORDER:
	{
		const int properties[3] = { CSS_PROP_BORDER_WIDTH,
									CSS_PROP_BORDER_STYLE,
									CSS_PROP_BORDER_COLOR };
		return getShortHandValue(properties, 3);
	}
	case CSS_PROP_BORDER_TOP:
	{
		const int properties[3] = { CSS_PROP_BORDER_TOP_WIDTH,
									CSS_PROP_BORDER_TOP_STYLE,
									CSS_PROP_BORDER_TOP_COLOR};
		return getShortHandValue(properties, 3);
	}
	case CSS_PROP_BORDER_RIGHT:
	{
		const int properties[3] = { CSS_PROP_BORDER_RIGHT_WIDTH,
									CSS_PROP_BORDER_RIGHT_STYLE,
									CSS_PROP_BORDER_RIGHT_COLOR};
		return getShortHandValue(properties, 3);
	}
	case CSS_PROP_BORDER_BOTTOM:
	{
		const int properties[3] = { CSS_PROP_BORDER_BOTTOM_WIDTH,
									CSS_PROP_BORDER_BOTTOM_STYLE,
									CSS_PROP_BORDER_BOTTOM_COLOR};
		return getShortHandValue(properties, 3);
	}
	case CSS_PROP_BORDER_LEFT:
	{
		const int properties[3] = { CSS_PROP_BORDER_LEFT_WIDTH,
									CSS_PROP_BORDER_LEFT_STYLE,
									CSS_PROP_BORDER_LEFT_COLOR};
		return getShortHandValue(properties, 3);
	}
	case CSS_PROP_OUTLINE:
	{
		const int properties[3] = { CSS_PROP_OUTLINE_WIDTH,
									CSS_PROP_OUTLINE_STYLE,
									CSS_PROP_OUTLINE_COLOR };
		return getShortHandValue(properties, 3);
	}
	case CSS_PROP_BORDER_COLOR:
	{
		const int properties[4] = { CSS_PROP_BORDER_TOP_COLOR,
									CSS_PROP_BORDER_RIGHT_COLOR,
									CSS_PROP_BORDER_BOTTOM_COLOR,
									CSS_PROP_BORDER_LEFT_COLOR };
		return get4Values(properties);
	}
	case CSS_PROP_BORDER_WIDTH:
	{
		const int properties[4] = { CSS_PROP_BORDER_TOP_WIDTH,
									CSS_PROP_BORDER_RIGHT_WIDTH,
									CSS_PROP_BORDER_BOTTOM_WIDTH,
									CSS_PROP_BORDER_LEFT_WIDTH };
		return get4Values(properties);
	}
	case CSS_PROP_BORDER_STYLE:
	{
		const int properties[4] = { CSS_PROP_BORDER_TOP_STYLE,
									CSS_PROP_BORDER_RIGHT_STYLE,
									CSS_PROP_BORDER_BOTTOM_STYLE,
									CSS_PROP_BORDER_LEFT_STYLE };
		return get4Values(properties);
	}
	case CSS_PROP_MARGIN:
	{
		const int properties[4] = { CSS_PROP_MARGIN_TOP,
									CSS_PROP_MARGIN_RIGHT,
									CSS_PROP_MARGIN_BOTTOM,
									CSS_PROP_MARGIN_LEFT };
		return get4Values(properties);
	}
	case CSS_PROP_PADDING:
	{
		const int properties[4] = { CSS_PROP_PADDING_TOP,
									CSS_PROP_PADDING_RIGHT,
									CSS_PROP_PADDING_BOTTOM,
									CSS_PROP_PADDING_LEFT };
		return get4Values(properties);
	}
	case CSS_PROP_LIST_STYLE:
	{
		const int properties[3] = { CSS_PROP_LIST_STYLE_TYPE,
									CSS_PROP_LIST_STYLE_POSITION,
									CSS_PROP_LIST_STYLE_IMAGE };
		return getShortHandValue(properties, 3);
	}
	}
	
	return 0;
}

DOMStringImpl *CSSStyleDeclarationImpl::getPropertyValue(DOMStringImpl *propertyName) const
{
	if(!m_lstValues || !propertyName)
		return 0;

	int propertyId = (m_interface ? m_interface->getPropertyID(propertyName->string().ascii(),
															   propertyName->length()) : 0);

	return getPropertyValue(propertyId);
}

CSSValueImpl *CSSStyleDeclarationImpl::getPropertyCSSValue(int propertyID) const
{
	if(!m_lstValues)
		return 0;

	QPtrListIterator<CSSProperty> lstValuesIt(*m_lstValues);
	CSSProperty *current;
	for(lstValuesIt.toLast(); (current = lstValuesIt.current()); --lstValuesIt)
	{
		if(current->m_id == propertyID && !current->m_nonCSSHint)
			return current->value();
	}

	return 0;
}

CSSValueImpl *CSSStyleDeclarationImpl::getPropertyCSSValue(DOMStringImpl *propertyName) const
{
	if(!m_lstValues || !propertyName)
		return 0;

	int propertyId = (m_interface ? m_interface->getPropertyID(propertyName->string().ascii(),
															   propertyName->length()) : 0);

	return getPropertyCSSValue(propertyId);
}

DOMStringImpl *CSSStyleDeclarationImpl::removeProperty(DOMStringImpl *propertyName)
{
	if(!m_lstValues || !propertyName)
		return 0;

	int propertyId = (m_interface ? m_interface->getPropertyID(propertyName->string().ascii(),
															   propertyName->length()) : 0);

	return removeProperty(propertyId);
}

DOMStringImpl *CSSStyleDeclarationImpl::removeProperty(int propertyID, bool nonCSSHint)
{
	if(!m_lstValues)
		return 0;

	DOMStringImpl *value = 0;

	QPtrListIterator<CSSProperty> lstValuesIt(*m_lstValues);
	CSSProperty *current;
	for(lstValuesIt.toLast(); (current = lstValuesIt.current()); --lstValuesIt)
	{
		if(current->m_id == propertyID && nonCSSHint == current->m_nonCSSHint)
		{
			value = current->value()->cssText();
			m_lstValues->removeRef(current);
			setChanged();
			break;
		}
	}

	return value;
}

bool CSSStyleDeclarationImpl::getPropertyPriority(int propertyID) const
{
	if(!m_lstValues)
		return false;

	QPtrListIterator<CSSProperty> lstValuesIt(*m_lstValues);
		
	CSSProperty *current;
	for(lstValuesIt.toFirst(); (current = lstValuesIt.current()); ++lstValuesIt)
	{
		if(propertyID == current->m_id)
			return current->m_important;
	}

	return false;
}

DOMStringImpl *CSSStyleDeclarationImpl::getPropertyPriority(DOMStringImpl *propertyName) const
{
	if(!m_lstValues || !propertyName)
		return 0;

	int propertyId = (m_interface ? m_interface->getPropertyID(propertyName->string().ascii(),
															   propertyName->length()) : 0);

	if(propertyId && getPropertyPriority(propertyId))
		return new DOMStringImpl("important");

	return 0;
}

void CSSStyleDeclarationImpl::setProperty(int id, int value, bool important, bool nonCSSHint)
{
	if(!m_lstValues)
	{
        m_lstValues = new QPtrList<CSSProperty>();
		m_lstValues->setAutoDelete(true);
	}

	removeProperty(id, nonCSSHint);

	CSSValueImpl *cssValue = new CSSPrimitiveValueImpl(m_interface, value);
	setParsedValue(id, cssValue, important, nonCSSHint, m_lstValues);
	setChanged();
}

bool CSSStyleDeclarationImpl::setProperty(int propertyID, DOMStringImpl *value, bool important, bool nonCSSHint)
{
	if(!m_lstValues)
	{
		m_lstValues = new QPtrList<CSSProperty>();
		m_lstValues->setAutoDelete(true);
	}

	CSSParser *parser = createCSSParser(m_strictParsing);
	if(!parser)
		return false;

	bool success = parser->parseValue(this, propertyID, value, important, nonCSSHint);
	if(!success)
	{
		QString qProp = QString::fromLatin1((m_interface ? m_interface->getPropertyName(propertyID) : "N/A"));
		kdDebug(6080) << "CSSStyleDeclarationImpl::setProperty invalid property: [" << qProp << "] value: [" << DOMString(value).string() << "]"<< endl;
	}
	else
		setChanged();

	delete parser;
	return success;
}

void CSSStyleDeclarationImpl::setProperty(DOMStringImpl *propertyName, DOMStringImpl *value, DOMStringImpl *priority)
{
	int propertyId = (m_interface && propertyName ? m_interface->getPropertyID(propertyName->string().ascii(),
																			   propertyName->length()) : 0);

	if(!propertyId)
		return;

	bool important = false;

	QString str = (priority ? priority->string() : QString::null);
	if(str.find(QString::fromLatin1("important"), 0, false) != -1)
		important = true;

	setProperty(propertyId, value, important);
}

void CSSStyleDeclarationImpl::setProperty(DOMStringImpl *propertyString)
{
	if(!m_lstValues)
	{
		m_lstValues = new QPtrList<CSSProperty>();
		m_lstValues->setAutoDelete(true);
	}

	CSSParser *parser = createCSSParser(m_strictParsing);
	if(!parser)
		return;

	parser->parseDeclaration(this, propertyString, false);
	setChanged();
	delete parser;
}

void CSSStyleDeclarationImpl::setLengthProperty(int propertyID, DOMStringImpl *value, bool important, bool nonCSSHint, bool multiLength)
{
	bool parseMode = m_strictParsing;
	m_strictParsing = false;
	m_multiLength = multiLength;

	setProperty(propertyID, value, important, nonCSSHint);
	m_strictParsing = parseMode;
	m_multiLength = false;
}

unsigned long CSSStyleDeclarationImpl::length() const
{
	return (m_lstValues ? m_lstValues->count() : 0);
}

DOMStringImpl *CSSStyleDeclarationImpl::item(unsigned long index) const
{
	if(m_lstValues && index < m_lstValues->count() && m_lstValues->at(index))
		return new DOMStringImpl(m_interface->getPropertyName(m_lstValues->at(index)->m_id));

	return 0;
}

CSSRuleImpl *CSSStyleDeclarationImpl::parentRule() const
{
	if(m_parent && m_parent->isRule())
		return static_cast<CSSRuleImpl *>(m_parent);

	return 0;
}

void CSSStyleDeclarationImpl::setChanged()
{
	if(m_node)
	{
		m_node->setChanged();
		return;
	}

	// ### quick&dirty hack for KDE 3.0... make this MUCH better! (Dirk)
	for(StyleBaseImpl* stylesheet = this; stylesheet; stylesheet = stylesheet->parent())
	{
		if(stylesheet->isCSSStyleSheet())
		{
			static_cast<CSSStyleSheetImpl *>(stylesheet)->doc()->updateStyleSelector();
			break;
		}
	}
}

bool CSSStyleDeclarationImpl::parseString(DOMStringImpl *, bool)
{
	kdDebug() << "WARNING: CSSStyleDeclarationImpl::parseString, unimplemented, was called" << endl;
	return false;
}

void CSSStyleDeclarationImpl::removeCSSHints()
{
	if(!m_lstValues)
		return;

	for(int i = (int) m_lstValues->count() - 1; i >= 0; i--)
	{
		if(!m_lstValues->at(i)->m_nonCSSHint)
			m_lstValues->remove(i);
	}
}

CSSProperty::CSSProperty()
{
	m_id = -1;
	m_value = 0;
	m_important = false;
	m_nonCSSHint = false;
}

CSSProperty::CSSProperty(const CSSProperty &other)
{
	m_id = other.m_id;
	m_value = other.m_value;
	m_important = other.m_important;
	m_nonCSSHint = other.m_nonCSSHint;
	
	if(m_value)
		m_value->ref();
}

CSSProperty::~CSSProperty()
{
	if(m_value)
		m_value->deref();
}

void CSSProperty::setValue(CSSValueImpl *val)
{
	KDOM_SAFE_SET(m_value, val);
}

CSSValueImpl *CSSProperty::value() const
{
	return m_value;
}

DOMStringImpl *CSSProperty::cssText(const CSSStyleDeclarationImpl &decl) const
{
	return new DOMStringImpl(QString::fromLatin1(decl.interface()->getPropertyName(m_id)) +
							 QString::fromLatin1(" ") + DOMString(m_value->cssText()).string() +
							 (m_important ? QString::fromLatin1(" !important") : QString::null) +
							 QString::fromLatin1("; "));
}

// vim:ts=4:noet
