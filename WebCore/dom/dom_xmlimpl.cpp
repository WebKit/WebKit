/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Apple Computer, Inc.
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

#include "config.h"
#include "dom_xmlimpl.h"

#include "CachedCSSStyleSheet.h"
#include "CachedXSLStyleSheet.h"
#include "DocLoader.h"
#include "DocumentImpl.h"
#include "css/css_stylesheetimpl.h"
#include "dom/dom_exception.h"
#include "dom_node.h"
#include "StringImpl.h"
#include "xml_tokenizer.h"

#ifdef KHTML_XSLT
#include "xsl_stylesheetimpl.h"
#endif

namespace WebCore {

EntityImpl::EntityImpl(DocumentImpl* doc) : ContainerNodeImpl(doc)
{
}

EntityImpl::EntityImpl(DocumentImpl* doc, const DOMString& name) : ContainerNodeImpl(doc), m_name(name.impl())
{
}

EntityImpl::EntityImpl(DocumentImpl* doc, const DOMString& publicId, const DOMString& systemId, const DOMString& notationName)
    : ContainerNodeImpl(doc), m_publicId(publicId.impl()), m_systemId(systemId.impl()), m_notationName(notationName.impl())
{
}

DOMString EntityImpl::nodeName() const
{
    return m_name.get();
}

unsigned short EntityImpl::nodeType() const
{
    return Node::ENTITY_NODE;
}

PassRefPtr<NodeImpl> EntityImpl::cloneNode(bool /*deep*/)
{
    // Spec says cloning Entity nodes is "implementation dependent". We do not support it.
    return 0;
}

// DOM Section 1.1.1
bool EntityImpl::childTypeAllowed(unsigned short type)
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
        result += m_name.get();
    }

    if (m_publicId && m_publicId->l != 0) {
        result += " PUBLIC \"";
        result += m_publicId.get();
        result += "\" \"";
        result += m_systemId.get();
        result += "\"";
    } else if (m_systemId && m_systemId->l != 0) {
        result += " SYSTEM \"";
        result += m_systemId.get();
        result += "\"";
    }

    if (m_notationName && m_notationName->l != 0) {
        result += " NDATA ";
        result += m_notationName.get();
    }

    result += ">";

    return result;
}

// -------------------------------------------------------------------------

EntityReferenceImpl::EntityReferenceImpl(DocumentImpl* doc) : ContainerNodeImpl(doc)
{
}

EntityReferenceImpl::EntityReferenceImpl(DocumentImpl* doc, DOMStringImpl* entityName)
    : ContainerNodeImpl(doc), m_entityName(entityName)
{
}

DOMString EntityReferenceImpl::nodeName() const
{
    return m_entityName.get();
}

unsigned short EntityReferenceImpl::nodeType() const
{
    return Node::ENTITY_REFERENCE_NODE;
}

PassRefPtr<NodeImpl> EntityReferenceImpl::cloneNode(bool deep)
{
    PassRefPtr<EntityReferenceImpl> clone = new EntityReferenceImpl(getDocument(), m_entityName.get());
    // ### make sure children are readonly
    // ### since we are a reference, should we clone children anyway (even if not deep?)
    if (deep)
        cloneChildNodes(clone.get());
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
    result += m_entityName.get();
    result += ";";

    return result;
}

// -------------------------------------------------------------------------

NotationImpl::NotationImpl(DocumentImpl* doc) : ContainerNodeImpl(doc)
{
}

NotationImpl::NotationImpl(DocumentImpl* doc, const DOMString& name, const DOMString& publicId, const DOMString& systemId)
    : ContainerNodeImpl(doc), m_name(name.impl()), m_publicId(publicId.impl()), m_systemId(systemId.impl())
{
}

DOMString NotationImpl::nodeName() const
{
    return m_name.get();
}

unsigned short NotationImpl::nodeType() const
{
    return Node::NOTATION_NODE;
}

PassRefPtr<NodeImpl> NotationImpl::cloneNode(bool /*deep*/)
{
    // Spec says cloning Notation nodes is "implementation dependent". We do not support it.
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

ProcessingInstructionImpl::ProcessingInstructionImpl(DocumentImpl* doc)
    : ContainerNodeImpl(doc), m_cachedSheet(0), m_loading(false)
{
#ifdef KHTML_XSLT
    m_isXSL = false;
#endif
}

ProcessingInstructionImpl::ProcessingInstructionImpl(DocumentImpl* doc, const DOMString& target, const DOMString& data)
    : ContainerNodeImpl(doc), m_target(target.impl()), m_data(data.impl()), m_cachedSheet(0), m_loading(false)
{
#ifdef KHTML_XSLT
    m_isXSL = false;
#endif
}

ProcessingInstructionImpl::~ProcessingInstructionImpl()
{
    if (m_cachedSheet)
        m_cachedSheet->deref(this);
}

void ProcessingInstructionImpl::setData(const DOMString& data, int &exceptioncode )
{
    // NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly.
    if (isReadOnly()) {
        exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return;
    }
    m_data = data.impl();
}

DOMString ProcessingInstructionImpl::nodeName() const
{
    return m_target.get();
}

unsigned short ProcessingInstructionImpl::nodeType() const
{
    return Node::PROCESSING_INSTRUCTION_NODE;
}

DOMString ProcessingInstructionImpl::nodeValue() const
{
    return m_data.get();
}

void ProcessingInstructionImpl::setNodeValue(const DOMString& nodeValue, int &exceptioncode)
{
    // NO_MODIFICATION_ALLOWED_ERR: taken care of by setData()
    setData(nodeValue, exceptioncode);
}

PassRefPtr<NodeImpl> ProcessingInstructionImpl::cloneNode(bool /*deep*/)
{
    // ### copy m_localHref
    return new ProcessingInstructionImpl(getDocument(), m_target.get(), m_data.get());
}

// DOM Section 1.1.1
bool ProcessingInstructionImpl::childTypeAllowed(unsigned short /*type*/)
{
    return false;
}

bool ProcessingInstructionImpl::checkStyleSheet()
{
    if (DOMString(m_target.get()) == "xml-stylesheet") {
        // see http://www.w3.org/TR/xml-stylesheet/
        // ### check that this occurs only in the prolog
        // ### support stylesheet included in a fragment of this (or another) document
        // ### make sure this gets called when adding from javascript
        bool attrsOk;
        const HashMap<DOMString, DOMString> attrs = parseAttributes(m_data.get(), attrsOk);
        if (!attrsOk)
            return true;
        HashMap<DOMString, DOMString>::const_iterator i = attrs.find("type");
        DOMString type;
        if (i != attrs.end())
            type = i->second;
        
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

        DOMString href = attrs.get("href");

        if (href.length() > 1) {
            if (href[0] == '#') {
                m_localHref = href.substring(1).impl();
#ifdef KHTML_XSLT
                // We need to make a synthetic XSLStyleSheetImpl that is embedded.  It needs to be able
                // to kick off import/include loads that can hang off some parent sheet.
                if (m_isXSL) {
                    PassRefPtr<XSLStyleSheetImpl> localSheet = new XSLStyleSheetImpl(this, m_localHref.get(), true);
                    localSheet->setDocument((xmlDocPtr)getDocument()->transformSource());
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
                if (getDocument()->frame()) {
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
#ifdef KHTML_XSLT
    if (m_isXSL)
        m_sheet = new XSLStyleSheetImpl(this, url);
    else
#endif
        m_sheet = new CSSStyleSheetImpl(this, url);
    m_sheet->parseString(sheet);
    if (m_cachedSheet)
        m_cachedSheet->deref(this);
    m_cachedSheet = 0;

    m_loading = false;

    // Tell the doc about the sheet.
    if (!isLoading() && m_sheet)
        getDocument()->stylesheetLoaded();
}

DOMString ProcessingInstructionImpl::toString() const
{
    DOMString result = "<?";
    result += m_target.get();
    result += " ";
    result += m_data.get();
    result += "?>";
    return result;
}

void ProcessingInstructionImpl::setStyleSheet(StyleSheetImpl* sheet)
{
    m_sheet = sheet;
}

} // namespace
