/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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
 *
 */

#ifndef ProcessingInstruction_h
#define ProcessingInstruction_h

#include "CachedResourceHandle.h"
#include "CachedStyleSheetClient.h"
#include "CharacterData.h"

namespace WebCore {

class StyleSheet;
class CSSStyleSheet;

class ProcessingInstruction final : public CharacterData, private CachedStyleSheetClient {
public:
    static PassRefPtr<ProcessingInstruction> create(Document&, const String& target, const String& data);
    virtual ~ProcessingInstruction();

    const String& target() const { return m_target; }

    void setCreatedByParser(bool createdByParser) { m_createdByParser = createdByParser; }

    virtual void finishParsingChildren() override;

    const String& localHref() const { return m_localHref; }
    StyleSheet* sheet() const { return m_sheet.get(); }
    void setCSSStyleSheet(PassRefPtr<CSSStyleSheet>);

    bool isCSS() const { return m_isCSS; }
#if ENABLE(XSLT)
    bool isXSL() const { return m_isXSL; }
#endif

private:
    friend class CharacterData;
    ProcessingInstruction(Document&, const String& target, const String& data);

    virtual String nodeName() const override;
    virtual NodeType nodeType() const override;
    virtual PassRefPtr<Node> cloneNode(bool deep) override;

    virtual InsertionNotificationRequest insertedInto(ContainerNode&) override;
    virtual void removedFrom(ContainerNode&) override;

    void checkStyleSheet();
    virtual void setCSSStyleSheet(const String& href, const URL& baseURL, const String& charset, const CachedCSSStyleSheet*) override;
#if ENABLE(XSLT)
    virtual void setXSLStyleSheet(const String& href, const URL& baseURL, const String& sheet) override;
#endif

    bool isLoading() const;
    virtual bool sheetLoaded() override;

    virtual void addSubresourceAttributeURLs(ListHashSet<URL>&) const override;

    void parseStyleSheet(const String& sheet);

    String m_target;
    String m_localHref;
    String m_title;
    String m_media;
    CachedResourceHandle<CachedResource> m_cachedSheet;
    RefPtr<StyleSheet> m_sheet;
    bool m_loading;
    bool m_alternate;
    bool m_createdByParser;
    bool m_isCSS;
#if ENABLE(XSLT)
    bool m_isXSL;
#endif
};

inline bool isProcessingInstruction(const Node& node)
{
    return node.nodeType() == Node::PROCESSING_INSTRUCTION_NODE;
}

NODE_TYPE_CASTS(ProcessingInstruction)

} //namespace

#endif
