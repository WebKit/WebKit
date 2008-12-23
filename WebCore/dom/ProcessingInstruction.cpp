/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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
#include "MediaList.h"

namespace WebCore {

ProcessingInstruction::ProcessingInstruction(Document* doc)
    : ContainerNode(doc)
    , m_cachedSheet(0)
    , m_loading(false)
    , m_alternate(false)
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
    , m_alternate(false)
#if ENABLE(XSLT)
    , m_isXSL(false)
#endif
{
}

ProcessingInstruction::~ProcessingInstruction()
{
    if (m_cachedSheet)
        m_cachedSheet->removeClient(this);
}

void ProcessingInstruction::setData(const String& data, ExceptionCode&)
{
    int oldLength = m_data.length();
    m_data = data;
    document()->textRemoved(this, 0, oldLength);
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

void ProcessingInstruction::checkStyleSheet()
{
    if (m_target == "xml-stylesheet" && document()->frame() && parentNode() == document()) {
        // see http://www.w3.org/TR/xml-stylesheet/
        // ### support stylesheet included in a fragment of this (or another) document
        // ### make sure this gets called when adding from javascript
        bool attrsOk;
        const HashMap<String, String> attrs = parseAttributes(m_data, attrsOk);
        if (!attrsOk)
            return;
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
            return;

        String href = attrs.get("href");
        String alternate = attrs.get("alternate");
        m_alternate = alternate == "yes";
        m_title = attrs.get("title");
        m_media = attrs.get("media");

        if (href.length() > 1 && href[0] == '#') {
            m_localHref = href.substring(1);
#if ENABLE(XSLT)
            // We need to make a synthetic XSLStyleSheet that is embedded.  It needs to be able
            // to kick off import/include loads that can hang off some parent sheet.
            if (m_isXSL) {
                m_sheet = XSLStyleSheet::createEmbedded(this, m_localHref);
                m_loading = false;
            }
#endif
        } else {
            m_loading = true;
            document()->addPendingSheet();
            if (m_cachedSheet)
                m_cachedSheet->removeClient(this);
#if ENABLE(XSLT)
            if (m_isXSL)
                m_cachedSheet = document()->docLoader()->requestXSLStyleSheet(document()->completeURL(href).string());
            else
#endif
            {
                String charset = attrs.get("charset");
                if (charset.isEmpty())
                    charset = document()->frame()->loader()->encoding();

                m_cachedSheet = document()->docLoader()->requestCSSStyleSheet(document()->completeURL(href).string(), charset);
            }
            if (m_cachedSheet)
                m_cachedSheet->addClient(this);
        }
    }
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

void ProcessingInstruction::setCSSStyleSheet(const String& url, const String& charset, const CachedCSSStyleSheet* sheet)
{
#if ENABLE(XSLT)
    ASSERT(!m_isXSL);
#endif
    RefPtr<CSSStyleSheet> newSheet = CSSStyleSheet::create(this, url, charset);
    m_sheet = newSheet;
    parseStyleSheet(sheet->sheetText());
    newSheet->setTitle(m_title);
    newSheet->setMedia(MediaList::create(newSheet.get(), m_media));
    newSheet->setDisabled(m_alternate);
}

#if ENABLE(XSLT)
void ProcessingInstruction::setXSLStyleSheet(const String& url, const String& sheet)
{
    ASSERT(m_isXSL);
    m_sheet = XSLStyleSheet::create(this, url);
    parseStyleSheet(sheet);
}
#endif

void ProcessingInstruction::parseStyleSheet(const String& sheet)
{
    m_sheet->parseString(sheet, true);
    if (m_cachedSheet)
        m_cachedSheet->removeClient(this);
    m_cachedSheet = 0;

    m_loading = false;
    m_sheet->checkLoaded();
}

void ProcessingInstruction::setCSSStyleSheet(PassRefPtr<CSSStyleSheet> sheet)
{
    ASSERT(!m_cachedSheet);
    ASSERT(!m_loading);
    m_sheet = sheet;
    m_sheet->setTitle(m_title);
    m_sheet->setDisabled(m_alternate);
}

bool ProcessingInstruction::offsetInCharacters() const
{
    return true;
}

int ProcessingInstruction::maxCharacterOffset() const 
{
    return static_cast<int>(m_data.length());
}

void ProcessingInstruction::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    if (!sheet())
        return;
    
    addSubresourceURL(urls, sheet()->baseURL());
}

void ProcessingInstruction::insertedIntoDocument()
{
    ContainerNode::insertedIntoDocument();
    document()->addStyleSheetCandidateNode(this, m_createdByParser);
    checkStyleSheet();
}

void ProcessingInstruction::removedFromDocument()
{
    ContainerNode::removedFromDocument();

    if (document()->renderer())
        document()->removeStyleSheetCandidateNode(this);

    // FIXME: It's terrible to do a synchronous update of the style selector just because a <style> or <link> element got removed.
    if (m_cachedSheet)
        document()->updateStyleSelector();
}

void ProcessingInstruction::finishParsingChildren()
{
    m_createdByParser = false;
    ContainerNode::finishParsingChildren();
}

} // namespace
