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
#include "ProcessingInstruction.h"

#include "CSSStyleSheet.h"
#include "CachedCSSStyleSheet.h"
#include "CachedXSLStyleSheet.h"
#include "Document.h"
#include "DocLoader.h"
#include "ExceptionCode.h"
#include "XSLStyleSheet.h"
#include "xml_tokenizer.h" // for parseAttributes()

namespace WebCore {

ProcessingInstruction::ProcessingInstruction(Document* doc)
    : ContainerNode(doc)
    , m_cachedSheet(0)
    , m_loading(false)
#if KHTML_XSLT
    , m_isXSL(false)
#endif
{
}

ProcessingInstruction::ProcessingInstruction(Document* doc, const String& target, const String& data)
    : ContainerNode(doc)
    , m_target(target.impl())
    , m_data(data.impl())
    , m_cachedSheet(0)
    , m_loading(false)
#if KHTML_XSLT
    , m_isXSL(false)
#endif
{
}

ProcessingInstruction::~ProcessingInstruction()
{
    if (m_cachedSheet)
        m_cachedSheet->deref(this);
}

void ProcessingInstruction::setData(const String& data, ExceptionCode& ec)
{
    // NO_MODIFICATION_ALLOWED_ERR: Raised when the node is readonly.
    if (isReadOnlyNode()) {
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
    return new ProcessingInstruction(document(), m_target.get(), m_data.get());
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
                if (document()->frame()) {
                    m_loading = true;
                    document()->addPendingSheet();
                    if (m_cachedSheet)
                        m_cachedSheet->deref(this);
#if KHTML_XSLT
                    if (m_isXSL)
                        m_cachedSheet = document()->docLoader()->requestXSLStyleSheet(document()->completeURL(href));
                    else
#endif
                    m_cachedSheet = document()->docLoader()->requestStyleSheet(document()->completeURL(href), DeprecatedString::null);
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
        document()->stylesheetLoaded();
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
        document()->stylesheetLoaded();
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
