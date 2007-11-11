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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "config.h"
#include "ProcessingInstruction.h"

#include "CSSStyleSheet.h"
#include "CachedCSSStyleSheet.h"
#include "CachedXSLStyleSheet.h"
#include "Document.h"
#include "DocLoader.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "XSLStyleSheet.h"
#include "XMLTokenizer.h" // for parseAttributes()

namespace WebCore {

ProcessingInstruction::ProcessingInstruction(Document* doc)
    : ContainerNode(doc)
    , m_cachedSheet(0)
    , m_loading(false)
#if ENABLE(XSLT)
    , m_isXSL(false)
#endif
{
}

ProcessingInstruction::ProcessingInstruction(Document* doc, const String& target, const String& data)
    : ContainerNode(doc)
    , m_target(target)
    , m_data(data)
    , m_cachedSheet(0)
    , m_loading(false)
#if ENABLE(XSLT)
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
    m_data = data;
}

String ProcessingInstruction::nodeName() const
{
    return m_target;
}

Node::NodeType ProcessingInstruction::nodeType() const
{
    return PROCESSING_INSTRUCTION_NODE;
}

String ProcessingInstruction::nodeValue() const
{
    return m_data;
}

void ProcessingInstruction::setNodeValue(const String& nodeValue, ExceptionCode& ec)
{
    // NO_MODIFICATION_ALLOWED_ERR: taken care of by setData()
    setData(nodeValue, ec);
}

PassRefPtr<Node> ProcessingInstruction::cloneNode(bool /*deep*/)
{
    // ### copy m_localHref
    return new ProcessingInstruction(document(), m_target, m_data);
}

// DOM Section 1.1.1
bool ProcessingInstruction::childTypeAllowed(NodeType)
{
    return false;
}

bool ProcessingInstruction::checkStyleSheet()
{
    if (m_target == "xml-stylesheet") {
        // see http://www.w3.org/TR/xml-stylesheet/
        // ### support stylesheet included in a fragment of this (or another) document
        // ### make sure this gets called when adding from javascript
        bool attrsOk;
        const HashMap<String, String> attrs = parseAttributes(m_data, attrsOk);
        if (!attrsOk)
            return true;
        HashMap<String, String>::const_iterator i = attrs.find("type");
        String type;
        if (i != attrs.end())
            type = i->second;

        bool isCSS = type.isEmpty() || type == "text/css";
#if ENABLE(XSLT)
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
                m_localHref = href.substring(1);
#if ENABLE(XSLT)
                // We need to make a synthetic XSLStyleSheet that is embedded.  It needs to be able
                // to kick off import/include loads that can hang off some parent sheet.
                if (m_isXSL) {
                    m_sheet = new XSLStyleSheet(this, m_localHref, true);
                    m_loading = false;
                }
                return !m_isXSL;
#endif
            }
            else
            {
                // FIXME: some validation on the URL?
                if (document()->frame()) {
                    m_loading = true;
                    document()->addPendingSheet();
                    if (m_cachedSheet)
                        m_cachedSheet->deref(this);
#if ENABLE(XSLT)
                    if (m_isXSL)
                        m_cachedSheet = document()->docLoader()->requestXSLStyleSheet(document()->completeURL(href));
                    else
#endif
                    {
                        String charset = attrs.get("charset");
                        if (charset.isEmpty())
                            charset = document()->frame()->loader()->encoding();

                        m_cachedSheet = document()->docLoader()->requestCSSStyleSheet(document()->completeURL(href), charset);
                    }
                    if (m_cachedSheet)
                        m_cachedSheet->ref(this);
#if ENABLE(XSLT)
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

bool ProcessingInstruction::sheetLoaded()
{
    if (!isLoading()) {
        document()->removePendingSheet();
        return true;
    }
    return false;
}

void ProcessingInstruction::setCSSStyleSheet(const String& url, const String& charset, const String& sheet)
{
#if ENABLE(XSLT)
    ASSERT(!m_isXSL);
#endif
    m_sheet = new CSSStyleSheet(this, url, charset);
    parseStyleSheet(sheet);
}

#if ENABLE(XSLT)
void ProcessingInstruction::setXSLStyleSheet(const String& url, const String& sheet)
{
    ASSERT(m_isXSL);
    m_sheet = new XSLStyleSheet(this, url);
    parseStyleSheet(sheet);
}
#endif

void ProcessingInstruction::parseStyleSheet(const String& sheet)
{
    m_sheet->parseString(sheet, true);
    if (m_cachedSheet)
        m_cachedSheet->deref(this);
    m_cachedSheet = 0;

    m_loading = false;
    m_sheet->checkLoaded();
}

String ProcessingInstruction::toString() const
{
    String result = "<?";
    result += m_target;
    result += " ";
    result += m_data;
    result += "?>";
    return result;
}

void ProcessingInstruction::setCSSStyleSheet(CSSStyleSheet* sheet)
{
    ASSERT(!m_cachedSheet);
    ASSERT(!m_loading);
    m_sheet = sheet;
}

bool ProcessingInstruction::offsetInCharacters() const
{
    return true;
}

int ProcessingInstruction::maxCharacterOffset() const 
{
    return static_cast<int>(m_data.length());
}

} // namespace
