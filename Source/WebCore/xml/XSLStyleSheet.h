/*
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004, 2006, 2008, 2012 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(XSLT)

#include "ProcessingInstruction.h"
#include "StyleSheet.h"
#include <libxml/parser.h>
#include <libxslt/transform.h>
#include <wtf/Ref.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

class CachedResourceLoader;
class XSLImportRule;
    
class XSLStyleSheet final : public StyleSheet {
public:
    static Ref<XSLStyleSheet> create(XSLImportRule* parentImport, const String& originalURL, const URL& finalURL)
    {
        return adoptRef(*new XSLStyleSheet(parentImport, originalURL, finalURL));
    }
    static Ref<XSLStyleSheet> create(ProcessingInstruction* parentNode, const String& originalURL, const URL& finalURL)
    {
        return adoptRef(*new XSLStyleSheet(parentNode, originalURL, finalURL, false));
    }
    static Ref<XSLStyleSheet> createEmbedded(ProcessingInstruction* parentNode, const URL& finalURL)
    {
        return adoptRef(*new XSLStyleSheet(parentNode, finalURL.string(), finalURL, true));
    }

    // Taking an arbitrary node is unsafe, because owner node pointer can become stale.
    // XSLTProcessor ensures that the stylesheet doesn't outlive its parent, in part by not exposing it to JavaScript.
    static Ref<XSLStyleSheet> createForXSLTProcessor(Node* parentNode, const String& originalURL, const URL& finalURL)
    {
        return adoptRef(*new XSLStyleSheet(parentNode, originalURL, finalURL, false));
    }

    virtual ~XSLStyleSheet();

    bool parseString(const String&);
    
    void checkLoaded();
    
    const URL& finalURL() const { return m_finalURL; }

    void loadChildSheets();
    void loadChildSheet(const String& href);

    CachedResourceLoader* cachedResourceLoader();

    Document* ownerDocument();
    XSLStyleSheet* parentStyleSheet() const override { return m_parentStyleSheet; }
    void setParentStyleSheet(XSLStyleSheet* parent);

    xmlDocPtr document();
    xsltStylesheetPtr compileStyleSheet();
    xmlDocPtr locateStylesheetSubResource(xmlDocPtr parentDoc, const xmlChar* uri);

    void clearDocuments();

    void markAsProcessed();
    bool processed() const { return m_processed; }
    
    String type() const override { return "text/xml"; }
    bool disabled() const override { return m_isDisabled; }
    void setDisabled(bool b) override { m_isDisabled = b; }
    Node* ownerNode() const override { return m_ownerNode; }
    String href() const override { return m_originalURL; }
    String title() const override { return emptyString(); }

    void clearOwnerNode() override { m_ownerNode = nullptr; }
    URL baseURL() const override { return m_finalURL; }
    bool isLoading() const override;

private:
    XSLStyleSheet(Node* parentNode, const String& originalURL, const URL& finalURL, bool embedded);
    XSLStyleSheet(XSLImportRule* parentImport, const String& originalURL, const URL& finalURL);

    bool isXSLStyleSheet() const override { return true; }
    String debugDescription() const final;

    void clearXSLStylesheetDocument();

    Node* m_ownerNode;
    String m_originalURL;
    URL m_finalURL;
    bool m_isDisabled { false };

    Vector<std::unique_ptr<XSLImportRule>> m_children;

    bool m_embedded;
    bool m_processed;

    xmlDocPtr m_stylesheetDoc { nullptr };
    bool m_stylesheetDocTaken { false };
    bool m_compilationFailed { false };

    XSLStyleSheet* m_parentStyleSheet { nullptr };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::XSLStyleSheet)
    static bool isType(const WebCore::StyleSheet& styleSheet) { return styleSheet.isXSLStyleSheet(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(XSLT)
