/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 2000 Peter Kelly (pmk@post.com)

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

// qxml can't be included otherwhise :/
#undef QT_NO_CAST_ASCII

#ifndef APPLE_CHANGES
#include <qxml.h>
#endif

#include "kdom.h"
#include "KDOMLoader.h"
#include "DocumentImpl.h"
#include "CSSStyleSheetImpl.h"
#include "KDOMCachedStyleSheet.h"
#include "ProcessingInstructionImpl.h"

using namespace KDOM;

// TODO : maybe on some systems there is no qxml, think about moving this
// into the backends/. (Rob)
#ifndef APPLE_CHANGES
class XMLAttributeReader : public QXmlDefaultHandler
{
public:
	/* KDE 4: Make it const QString & */
	XMLAttributeReader(QString _attrString) { m_attrString = _attrString; }
	virtual ~XMLAttributeReader() {}
	QXmlAttributes readAttrs(bool &ok)
	{
		// parse xml file
		QXmlInputSource source;
		source.setData("<?xml version=\"1.0\"?><attrs "+m_attrString+" />");
		QXmlSimpleReader reader;
		reader.setContentHandler(this);
		ok = reader.parse(source);
		return attrs;
	}

	bool startElement(const QString &/*namespaceURI*/, const QString &localName, const QString&/*qName*/, const QXmlAttributes& atts)
	{
		if(localName == "attrs")
		{
			attrs = atts;
			return true;
		}

		return false; // we shouldn't have any other elements
	}

protected:
	QXmlAttributes attrs;
	QString m_attrString;
};
#endif // APPLE_CHANGES

ProcessingInstructionImpl::ProcessingInstructionImpl(DocumentPtr *doc, DOMStringImpl *target, DOMStringImpl *data) : NodeBaseImpl(doc)
{
	m_target = target;
	if(m_target)
		m_target->ref();

	m_data = data;
	if(m_data)
		m_data->ref();

	m_sheet = 0;
	m_cachedSheet = 0;
	m_localHref = 0;
}

ProcessingInstructionImpl::~ProcessingInstructionImpl()
{
	if(m_target)
		m_target->deref();
	if(m_data)
		m_data->deref();
	if(m_cachedSheet)
		m_cachedSheet->deref(this);
	if(m_sheet)
		m_sheet->deref();
}

DOMStringImpl *ProcessingInstructionImpl::nodeName() const
{
	return m_target;
}

unsigned short ProcessingInstructionImpl::nodeType() const
{
	return PROCESSING_INSTRUCTION_NODE;
}

DOMStringImpl *ProcessingInstructionImpl::target() const
{
	return m_target;
}

DOMStringImpl *ProcessingInstructionImpl::nodeValue() const
{
	return m_data;
}

void ProcessingInstructionImpl::setNodeValue(DOMStringImpl *nodeValue)
{
	setData(nodeValue);
}

DOMStringImpl *ProcessingInstructionImpl::textContent() const
{
	return nodeValue();
}

DOMStringImpl *ProcessingInstructionImpl::data() const
{
	return m_data;
}

void ProcessingInstructionImpl::setData(DOMStringImpl *_data)
{
	// NO_MODIFICATION_ALLOWED_ERR: Raised if this node is readonly
	if(isReadOnly())
		throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

	KDOM_SAFE_SET(m_data, _data);
}

NodeImpl *ProcessingInstructionImpl::cloneNode(bool, DocumentPtr *doc) const
{
	return doc->document()->createProcessingInstruction(target(), data());
}

DOMStringImpl *ProcessingInstructionImpl::localHref() const
{
	return m_localHref;
}

StyleSheetImpl *ProcessingInstructionImpl::sheet() const
{
	return m_sheet;
}

void ProcessingInstructionImpl::checkStyleSheet()
{
#ifndef APPLE_COMPILE_HACK
	if(m_target && DOMString(m_target) == "xml-stylesheet")
	{
		// see http://www.w3.org/TR/xml-stylesheet/
		// ### check that this occurs only in the prolog
		// ### support stylesheet included in a fragment of this (or another) document
		// ### make sure this gets called when adding from javascript
		XMLAttributeReader attrReader(DOMString(m_data).string());
		bool attrsOk;
		QXmlAttributes attrs = attrReader.readAttrs(attrsOk);
		if(!attrsOk) return;
		if(attrs.value("type") != "text/css") return;

		DOMString href = attrs.value("href");

		if(href.length() > 1)
		{
			if(href[0] == '#')
				KDOM_SAFE_SET(m_localHref, href.handle()->split(1));
			else
			{
				// ### some validation on the URL?
				// ### FIXME charset
				if(m_cachedSheet)
					m_cachedSheet->deref(this);
				m_cachedSheet = ownerDocument()->docLoader()->requestStyleSheet(KURL(ownerDocument()->documentKURI(), href.string()), QString::null);
				if(m_cachedSheet)
				{
					//before ref, because during the ref it might load!
					ownerDocument()->addPendingSheet();
					m_cachedSheet->ref(this);
				}
			}
		}
	}
#endif
}

void ProcessingInstructionImpl::setStyleSheet(DOMStringImpl *url, DOMStringImpl *sheet)
{
	if(m_sheet)
		m_sheet->deref();

	m_sheet = ownerDocument()->createCSSStyleSheet(this, url);
	if(m_sheet)
		m_sheet->parseString(sheet);

	if(m_cachedSheet)
		m_cachedSheet->deref(this);

	m_cachedSheet = 0;

	if(m_sheet)
		ownerDocument()->styleSheetLoaded();
}

void ProcessingInstructionImpl::setStyleSheet(CSSStyleSheetImpl *sheet)
{
	KDOM_SAFE_SET(m_sheet, sheet);
}

// vim:ts=4:noet
