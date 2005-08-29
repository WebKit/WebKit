/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
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

#include "dom/dom_exception.h"

#include "xml/dom_xmlimpl.h"
#include "xml/dom_docimpl.h"
#include "xml/dom_stringimpl.h"
#include "css/css_stylesheetimpl.h"
#ifdef KHTML_XSLT
#include "xsl_stylesheetimpl.h"
#endif
#include "misc/loader.h"
#include "xml/xml_tokenizer.h"

using khtml::parseAttributes;

namespace DOM {

EntityImpl::EntityImpl(DocumentPtr *doc) : ContainerNodeImpl(doc)
{
    m_publicId = 0;
    m_systemId = 0;
    m_notationName = 0;
    m_name = 0;
}

EntityImpl::EntityImpl(DocumentPtr *doc, DOMString _name) : ContainerNodeImpl(doc)
{
    m_publicId = 0;
    m_systemId = 0;
    m_notationName = 0;
    m_name = _name.impl();
    if (m_name)
        m_name->ref();
}

EntityImpl::EntityImpl(DocumentPtr *doc, DOMString _publicId, DOMString _systemId, DOMString _notationName) : ContainerNodeImpl(doc)
{
    m_publicId = _publicId.impl();
    if (m_publicId)
        m_publicId->ref();
    m_systemId = _systemId.impl();
    if (m_systemId)
        m_systemId->ref();
    m_notationName = _notationName.impl();
    if (m_notationName)
        m_notationName->ref();
    m_name = 0;
}


EntityImpl::~EntityImpl()
{
    if (m_publicId)
        m_publicId->deref();
    if (m_systemId)
        m_systemId->deref();
    if (m_notationName)
        m_notationName->deref();
    if (m_name)
        m_name->deref();
}

DOMString EntityImpl::publicId() const
{
    return m_publicId;
}

DOMString EntityImpl::systemId() const
{
    return m_systemId;
}

DOMString EntityImpl::notationName() const
{
    return m_notationName;
}

DOMString EntityImpl::nodeName() const
{
    return m_name;
}

unsigned short EntityImpl::nodeType() const
{
    return Node::ENTITY_NODE;
}

NodeImpl *EntityImpl::cloneNode ( bool /*deep*/)
{
    // Spec says cloning Document nodes is "implementation dependent"
    // so we do not support it...
    return 0;
}

// DOM Section 1.1.1
bool EntityImpl::childTypeAllowed( unsigned short type )
{
    switch (type) {
        case Node::ELEMENT_NODE:
        case Node::PROCESSING_INSTRUCTION_NODE:
        case Node::COMMENT_NODE:
        case Node::TEXT_NODE:
        case Node::CDATA_SECTION_NODE:
        case Node::ENTITY_REFERENCE_NODE:
            return true;
            break;
        default:
            return false;
    }
}

DOMString EntityImpl::toString() const
{
    DOMString result = "<!ENTITY' ";

    if (m_name && m_name->l != 0) {
	result += " ";
	result += m_name;
    }

    if (m_publicId && m_publicId->l != 0) {
	result += " PUBLIC \"";
	result += m_publicId;
	result += "\" \"";
	result += m_systemId;
	result += "\"";
    } else if (m_systemId && m_systemId->l != 0) {
	result += " SYSTEM \"";
	result += m_systemId;
	result += "\"";
    }

    if (m_notationName && m_notationName->l != 0) {
	result += " NDATA ";
	result += m_notationName;
    }

    result += ">";

    return result;
}


// -------------------------------------------------------------------------

EntityReferenceImpl::EntityReferenceImpl(DocumentPtr *doc) : ContainerNodeImpl(doc)
{
    m_entityName = 0;
}

EntityReferenceImpl::EntityReferenceImpl(DocumentPtr *doc, DOMStringImpl *_entityName) : ContainerNodeImpl(doc)
{
    m_entityName = _entityName;
    if (m_entityName)
        m_entityName->ref();
}

EntityReferenceImpl::~EntityReferenceImpl()
{
    if (m_entityName)
        m_entityName->deref();
}

DOMString EntityReferenceImpl::nodeName() const
{
    return m_entityName;
}

unsigned short EntityReferenceImpl::nodeType() const
{
    return Node::ENTITY_REFERENCE_NODE;
}

NodeImpl *EntityReferenceImpl::cloneNode ( bool deep )
{
    EntityReferenceImpl *clone = new EntityReferenceImpl(docPtr(),m_entityName);
    // ### make sure children are readonly
    // ### since we are a reference, should we clone children anyway (even if not deep?)
    if (deep)
        cloneChildNodes(clone);
    return clone;
}

// DOM Section 1.1.1
bool EntityReferenceImpl::childTypeAllowed( unsigned short type )
{
    switch (type) {
        case Node::ELEMENT_NODE:
        case Node::PROCESSING_INSTRUCTION_NODE:
        case Node::COMMENT_NODE:
        case Node::TEXT_NODE:
        case Node::CDATA_SECTION_NODE:
        case Node::ENTITY_REFERENCE_NODE:
            return true;
            break;
        default:
            return false;
    }
}

DOMString EntityReferenceImpl::toString() const
{
    DOMString result = "&";
    result += m_entityName;
    result += ";";

    return result;
}

// -------------------------------------------------------------------------

NotationImpl::NotationImpl(DocumentPtr *doc) : ContainerNodeImpl(doc)
{
    m_publicId = 0;
    m_systemId = 0;
    m_name = 0;
}

NotationImpl::NotationImpl(DocumentPtr *doc, DOMString _name, DOMString _publicId, DOMString _systemId) : ContainerNodeImpl(doc)
{
    m_name = _name.impl();
    if (m_name)
        m_name->ref();
    m_publicId = _publicId.impl();
    if (m_publicId)
        m_publicId->ref();
    m_systemId = _systemId.impl();
    if (m_systemId)
        m_systemId->ref();
}

NotationImpl::~NotationImpl()
{
    if (m_name)
        m_name->deref();
    if (m_publicId)
        m_publicId->deref();
    if (m_systemId)
        m_systemId->deref();
}

DOMString NotationImpl::publicId() const
{
    return m_publicId;
}

DOMString NotationImpl::systemId() const
{
    return m_systemId;
}

DOMString NotationImpl::nodeName() const
{
    return m_name;
}

unsigned short NotationImpl::nodeType() const
{
    return Node::NOTATION_NODE;
}

NodeImpl *NotationImpl::cloneNode ( bool /*deep*/)
{
    // Spec says cloning Document nodes is "implementation dependent"
    // so we do not support it...
    return 0;
}

// DOM Section 1.1.1
bool NotationImpl::childTypeAllowed( unsigned short /*type*/ )
{
    return false;
}

// -------------------------------------------------------------------------

// ### need a way of updating these properly whenever child nodes of the processing instruction
// change or are added/removed

ProcessingInstructionImpl::ProcessingInstructionImpl(DocumentPtr *doc) : ContainerNodeImpl(doc)
{
    m_target = 0;
    m_data = 0;
    m_localHref = 0;
    m_sheet = 0;
    m_cachedSheet = 0;
    m_loading = false;
#ifdef KHTML_XSLT
    m_isXSL = false;
#endif
}

ProcessingInstructionImpl::ProcessingInstructionImpl(DocumentPtr *doc, DOMString _target, DOMString _data) : ContainerNodeImpl(doc)
{
    m_target = _target.impl();
    if (m_target)
        m_target->ref();
    m_data = _data.impl();
    if (m_data)
        m_data->ref();
    m_sheet = 0;
    m_cachedSheet = 0;
    m_localHref = 0;
#ifdef KHTML_XSLT
    m_isXSL = false;
#endif
}

ProcessingInstructionImpl::~ProcessingInstructionImpl()
{
    if (m_target)
        m_target->deref();
    if (m_data)
        m_data->deref();
    if (m_cachedSheet)
	m_cachedSheet->deref(this);
    if (m_sheet)
	m_sheet->deref();
}

DOMString ProcessingInstructionImpl::target() const
{
    return m_target;
}

DOMString ProcessingInstructionImpl::data() const
{
    return m_data;
}

void ProcessingInstructionImpl::setData( const DOMString &_data, int &exceptioncode )
{
    // NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly.
    if (isReadOnly()) {
        exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    if (m_data)
        m_data->deref();
    m_data = _data.impl();
    if (m_data)
        m_data->ref();
}

DOMString ProcessingInstructionImpl::nodeName() const
{
    return m_target;
}

unsigned short ProcessingInstructionImpl::nodeType() const
{
    return Node::PROCESSING_INSTRUCTION_NODE;
}

DOMString ProcessingInstructionImpl::nodeValue() const
{
    return m_data;
}

void ProcessingInstructionImpl::setNodeValue( const DOMString &_nodeValue, int &exceptioncode )
{
    // NO_MODIFICATION_ALLOWED_ERR: taken care of by setData()
    setData(_nodeValue, exceptioncode);
}

NodeImpl *ProcessingInstructionImpl::cloneNode ( bool /*deep*/)
{
    // ### copy m_localHref
    return new ProcessingInstructionImpl(docPtr(),m_target,m_data);
}

DOMString ProcessingInstructionImpl::localHref() const
{
    return m_localHref;
}

// DOM Section 1.1.1
bool ProcessingInstructionImpl::childTypeAllowed( unsigned short /*type*/ )
{
    return false;
}

bool ProcessingInstructionImpl::checkStyleSheet()
{
    if (m_target && DOMString(m_target) == "xml-stylesheet") {
        // see http://www.w3.org/TR/xml-stylesheet/
        // ### check that this occurs only in the prolog
        // ### support stylesheet included in a fragment of this (or another) document
        // ### make sure this gets called when adding from javascript
        bool attrsOk;
        const QMap<QString, QString> attrs = parseAttributes(m_data, attrsOk);
        if (!attrsOk)
            return true;
        QMap<QString, QString>::ConstIterator i = attrs.find("type");
        QString type;
        if (i != attrs.end())
            type = *i;
        
        bool isCSS = type.isEmpty() || type == "text/css";
#ifdef KHTML_XSLT
        m_isXSL = (type == "text/xml" || type == "text/xsl" || type == "application/xml" ||
                   type == "application/xhtml+xml" || type == "application/rss+xml" || type == "application/atom=xml");
        if (!isCSS && !m_isXSL)
#else
        if (!isCSS)
#endif
            return true;

#ifdef KHTML_XSLT
        if (m_isXSL)
            getDocument()->tokenizer()->setTransformSource(getDocument());
#endif

        i = attrs.find("href");
        QString href;
        if (i != attrs.end())
            href = *i;

        if (href.length()>1)
        {
            if (href[0]=='#')
            {
                DOMString newLocalHref = href.mid(1);
                if (m_localHref)
                    m_localHref->deref();
                m_localHref = newLocalHref.impl();
                if (m_localHref)
                    m_localHref->ref();
#ifdef KHTML_XSLT
                // We need to make a synthetic XSLStyleSheetImpl that is embedded.  It needs to be able
                // to kick off import/include loads that can hang off some parent sheet.
                if (m_isXSL) {
                    if (m_sheet)
                        m_sheet->deref();
                    XSLStyleSheetImpl* localSheet = new XSLStyleSheetImpl(this, m_localHref, true);
                    localSheet->setDocument((xmlDocPtr)getDocument()->transformSource());
                    localSheet->ref();
                    localSheet->loadChildSheets();
                    m_sheet = localSheet;
                    m_loading = false;
                }                    
                return !m_isXSL;
#endif
            }
            else
            {
                // ### some validation on the URL?
                // ### FIXME charset
		if (getDocument()->part()) {
		    m_loading = true;
		    getDocument()->addPendingSheet();
		    if (m_cachedSheet) m_cachedSheet->deref(this);
#ifdef KHTML_XSLT
                    if (m_isXSL)
                        m_cachedSheet = getDocument()->docLoader()->requestXSLStyleSheet(getDocument()->completeURL(href));
                    else
#endif
                    m_cachedSheet = getDocument()->docLoader()->requestStyleSheet(getDocument()->completeURL(href), QString::null);
		    if (m_cachedSheet)
			m_cachedSheet->ref( this );
#ifdef KHTML_XSLT
                    return !m_isXSL;
#endif
		}
            }

        }
    }
    
    return true;
}

StyleSheetImpl* ProcessingInstructionImpl::sheet() const
{
    return m_sheet;
}

bool ProcessingInstructionImpl::isLoading() const
{
    if (m_loading)
        return true;
    if (!m_sheet)
        return false;
    return m_sheet->isLoading();
}

void ProcessingInstructionImpl::sheetLoaded()
{
    if (!isLoading())
        getDocument()->stylesheetLoaded();
}

void ProcessingInstructionImpl::setStyleSheet(const DOMString &url, const DOMString &sheet)
{
    if (m_sheet)
	m_sheet->deref();
#ifdef KHTML_XSLT
    if (m_isXSL)
        m_sheet = new XSLStyleSheetImpl(this, url);
    else
#endif
        m_sheet = new CSSStyleSheetImpl(this, url);
    m_sheet->ref();
    m_sheet->parseString(sheet);
    if (m_cachedSheet)
	m_cachedSheet->deref(this);
    m_cachedSheet = 0;

    m_loading = false;

    // Tell the doc about the sheet.
    if (!isLoading() && m_sheet)
        getDocument()->stylesheetLoaded();
}

void ProcessingInstructionImpl::setStyleSheet(CSSStyleSheetImpl* sheet)
{
    if (m_sheet)
        m_sheet->deref();
    m_sheet = sheet;
    if (m_sheet)
        m_sheet->ref();
}

DOMString ProcessingInstructionImpl::toString() const
{
    DOMString result = "<?";
    result += m_target;
    result += " ";
    result += m_data;
    result += ">";
    return result;
}

} // namespace
