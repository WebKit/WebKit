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
#include "Document.h"
#include "ExceptionCode.h"
#include "StringImpl.h"
#include "css_stylesheetimpl.h"
#include "xml_tokenizer.h"

#if KHTML_XSLT
#include "XSLStyleSheet.h"
#endif

namespace WebCore {

Entity::Entity(Document* doc) : ContainerNode(doc)
{
}

Entity::Entity(Document* doc, const String& name) : ContainerNode(doc), m_name(name.impl())
{
}

Entity::Entity(Document* doc, const String& publicId, const String& systemId, const String& notationName)
    : ContainerNode(doc), m_publicId(publicId.impl()), m_systemId(systemId.impl()), m_notationName(notationName.impl())
{
}

String Entity::nodeName() const
{
    return m_name.get();
}

Node::NodeType Entity::nodeType() const
{
    return ENTITY_NODE;
}

PassRefPtr<Node> Entity::cloneNode(bool /*deep*/)
{
    // Spec says cloning Entity nodes is "implementation dependent". We do not support it.
    return 0;
}

// DOM Section 1.1.1
bool Entity::childTypeAllowed(NodeType type)
{
    switch (type) {
        case ELEMENT_NODE:
        case PROCESSING_INSTRUCTION_NODE:
        case COMMENT_NODE:
        case TEXT_NODE:
        case CDATA_SECTION_NODE:
        case ENTITY_REFERENCE_NODE:
            return true;
            break;
        default:
            return false;
    }
}

String Entity::toString() const
{
    String result = "<!ENTITY' ";

    if (m_name && m_name->length()) {
        result += " ";
        result += m_name.get();
    }

    if (m_publicId && m_publicId->length()) {
        result += " PUBLIC \"";
        result += m_publicId.get();
        result += "\" \"";
        result += m_systemId.get();
        result += "\"";
    } else if (m_systemId && m_systemId->length()) {
        result += " SYSTEM \"";
        result += m_systemId.get();
        result += "\"";
    }

    if (m_notationName && m_notationName->length()) {
        result += " NDATA ";
        result += m_notationName.get();
    }

    result += ">";

    return result;
}

// -------------------------------------------------------------------------

EntityReference::EntityReference(Document* doc) : ContainerNode(doc)
{
}

EntityReference::EntityReference(Document* doc, StringImpl* entityName)
    : ContainerNode(doc), m_entityName(entityName)
{
}

String EntityReference::nodeName() const
{
    return m_entityName.get();
}

Node::NodeType EntityReference::nodeType() const
{
    return ENTITY_REFERENCE_NODE;
}

PassRefPtr<Node> EntityReference::cloneNode(bool deep)
{
    RefPtr<EntityReference> clone = new EntityReference(getDocument(), m_entityName.get());
    // ### make sure children are readonly
    // ### since we are a reference, should we clone children anyway (even if not deep?)
    if (deep)
        cloneChildNodes(clone.get());
    return clone.release();
}

// DOM Section 1.1.1
bool EntityReference::childTypeAllowed(NodeType type)
{
    switch (type) {
        case ELEMENT_NODE:
        case PROCESSING_INSTRUCTION_NODE:
        case COMMENT_NODE:
        case TEXT_NODE:
        case CDATA_SECTION_NODE:
        case ENTITY_REFERENCE_NODE:
            return true;
            break;
        default:
            return false;
    }
}

String EntityReference::toString() const
{
    String result = "&";
    result += m_entityName.get();
    result += ";";

    return result;
}

// -------------------------------------------------------------------------

Notation::Notation(Document* doc) : ContainerNode(doc)
{
}

Notation::Notation(Document* doc, const String& name, const String& publicId, const String& systemId)
    : ContainerNode(doc), m_name(name.impl()), m_publicId(publicId.impl()), m_systemId(systemId.impl())
{
}

String Notation::nodeName() const
{
    return m_name.get();
}

Node::NodeType Notation::nodeType() const
{
    return NOTATION_NODE;
}

PassRefPtr<Node> Notation::cloneNode(bool /*deep*/)
{
    // Spec says cloning Notation nodes is "implementation dependent". We do not support it.
    return 0;
}

// DOM Section 1.1.1
bool Notation::childTypeAllowed(NodeType)
{
    return false;
}

// -------------------------------------------------------------------------

// ### need a way of updating these properly whenever child nodes of the processing instruction
// change or are added/removed

ProcessingInstruction::ProcessingInstruction(Document* doc)
    : ContainerNode(doc), m_cachedSheet(0), m_loading(false)
{
#if KHTML_XSLT
    m_isXSL = false;
#endif
}

ProcessingInstruction::ProcessingInstruction(Document* doc, const String& target, const String& data)
    : ContainerNode(doc), m_target(target.impl()), m_data(data.impl()), m_cachedSheet(0), m_loading(false)
{
#if KHTML_XSLT
    m_isXSL = false;
#endif
}

ProcessingInstruction::~ProcessingInstruction()
{
    if (m_cachedSheet)
        m_cachedSheet->deref(this);
}

void ProcessingInstruction::setData(const String& data, ExceptionCode& ec)
{
    // NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly.
    if (isReadOnly()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }
    m_data = data.impl();
}

String ProcessingInstruction::nodeName() const
{
    return m_target.get();
}

Node::NodeType ProcessingInstruction::nodeType() const
{
    return PROCESSING_INSTRUCTION_NODE;
}

String ProcessingInstruction::nodeValue() const
{
    return m_data.get();
}

void ProcessingInstruction::setNodeValue(const String& nodeValue, ExceptionCode& ec)
{
    // NO_MODIFICATION_ALLOWED_ERR: taken care of by setData()
    setData(nodeValue, ec);
}

PassRefPtr<Node> ProcessingInstruction::cloneNode(bool /*deep*/)
{
    // ### copy m_localHref
    return new ProcessingInstruction(getDocument(), m_target.get(), m_data.get());
}

// DOM Section 1.1.1
bool ProcessingInstruction::childTypeAllowed(NodeType)
{
    return false;
}

bool ProcessingInstruction::checkStyleSheet()
{
    if (String(m_target.get()) == "xml-stylesheet") {
        // see http://www.w3.org/TR/xml-stylesheet/
        // ### check that this occurs only in the prolog
        // ### support stylesheet included in a fragment of this (or another) document
        // ### make sure this gets called when adding from javascript
        bool attrsOk;
        const HashMap<String, String> attrs = parseAttributes(m_data.get(), attrsOk);
        if (!attrsOk)
            return true;
        HashMap<String, String>::const_iterator i = attrs.find("type");
        String type;
        if (i != attrs.end())
            type = i->second;
        
        bool isCSS = type.isEmpty() || type == "text/css";
#if KHTML_XSLT
        m_isXSL = (type == "text/xml" || type == "text/xsl" || type == "application/xml" ||
                   type == "application/xhtml+xml" || type == "application/rss+xml" || type == "application/atom=xml");
        if (!isCSS && !m_isXSL)
#else
        if (!isCSS)
#endif
            return true;

        String href = attrs.get("href");

        if (href.length() > 1) {
            if (href[0] == '#') {
                m_localHref = href.substring(1).impl();
#if KHTML_XSLT
                // We need to make a synthetic XSLStyleSheet that is embedded.  It needs to be able
                // to kick off import/include loads that can hang off some parent sheet.
                if (m_isXSL) {
                    m_sheet = new XSLStyleSheet(this, m_localHref.get(), true);
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
                    if (m_cachedSheet)
                        m_cachedSheet->deref(this);
#if KHTML_XSLT
                    if (m_isXSL)
                        m_cachedSheet = getDocument()->docLoader()->requestXSLStyleSheet(getDocument()->completeURL(href));
                    else
#endif
                    m_cachedSheet = getDocument()->docLoader()->requestStyleSheet(getDocument()->completeURL(href), DeprecatedString::null);
                    if (m_cachedSheet)
                        m_cachedSheet->ref( this );
#if KHTML_XSLT
                    return !m_isXSL;
#endif
                }
            }

        }
    }
    
    return true;
}

bool ProcessingInstruction::isLoading() const
{
    if (m_loading)
        return true;
    if (!m_sheet)
        return false;
    return m_sheet->isLoading();
}

void ProcessingInstruction::sheetLoaded()
{
    if (!isLoading())
        getDocument()->stylesheetLoaded();
}

void ProcessingInstruction::setStyleSheet(const String &url, const String &sheet)
{
#if KHTML_XSLT
    if (m_isXSL)
        m_sheet = new XSLStyleSheet(this, url);
    else
#endif
        m_sheet = new CSSStyleSheet(this, url);
    m_sheet->parseString(sheet);
    if (m_cachedSheet)
        m_cachedSheet->deref(this);
    m_cachedSheet = 0;

    m_loading = false;

    // Tell the doc about the sheet.
    if (!isLoading() && m_sheet)
        getDocument()->stylesheetLoaded();
}

String ProcessingInstruction::toString() const
{
    String result = "<?";
    result += m_target.get();
    result += " ";
    result += m_data.get();
    result += "?>";
    return result;
}

void ProcessingInstruction::setStyleSheet(StyleSheet* sheet)
{
    m_sheet = sheet;
}

bool ProcessingInstruction::offsetInCharacters() const
{
    return true;
}

} // namespace
