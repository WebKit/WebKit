/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef HTMLDocument_h
#define HTMLDocument_h

#include "CachedResourceClient.h"
#include "Document.h"
#include <wtf/HashCountedSet.h>

namespace WebCore {

class HTMLDocument : public Document, public CachedResourceClient {
public:
    static PassRefPtr<HTMLDocument> create(Frame* frame, const URL& url)
    {
        return adoptRef(new HTMLDocument(frame, url, HTMLDocumentClass));
    }

    static PassRefPtr<HTMLDocument> createSynthesizedDocument(Frame* frame, const URL& url)
    {
        return adoptRef(new HTMLDocument(frame, url, HTMLDocumentClass, Synthesized));
    }

    virtual ~HTMLDocument();

    int width();
    int height();

    String dir();
    void setDir(const String&);

    String designMode() const;
    void setDesignMode(const String&);

    Element* activeElement();
    bool hasFocus();

    const AtomicString& bgColor() const;
    void setBgColor(const String&);
    const AtomicString& fgColor() const;
    void setFgColor(const String&);
    const AtomicString& alinkColor() const;
    void setAlinkColor(const String&);
    const AtomicString& linkColor() const;
    void setLinkColor(const String&);
    const AtomicString& vlinkColor() const;
    void setVlinkColor(const String&);

    void clear();

    void captureEvents();
    void releaseEvents();

    Element* documentNamedItem(const AtomicStringImpl& name) const { return m_documentNamedItem.getElementByDocumentNamedItem(name, *this); }
    bool hasDocumentNamedItem(const AtomicStringImpl& name) const { return m_documentNamedItem.contains(name); }
    bool documentNamedItemContainsMultipleElements(const AtomicStringImpl& name) const { return m_documentNamedItem.containsMultiple(name); }
    void addDocumentNamedItem(const AtomicStringImpl&, Element&);
    void removeDocumentNamedItem(const AtomicStringImpl&, Element&);

    Element* windowNamedItem(const AtomicStringImpl& name) const { return m_windowNamedItem.getElementByWindowNamedItem(name, *this); }
    bool hasWindowNamedItem(const AtomicStringImpl& name) const { return m_windowNamedItem.contains(name); }
    bool windowNamedItemContainsMultipleElements(const AtomicStringImpl& name) const { return m_windowNamedItem.containsMultiple(name); }
    void addWindowNamedItem(const AtomicStringImpl&, Element&);
    void removeWindowNamedItem(const AtomicStringImpl&, Element&);

    static bool isCaseSensitiveAttribute(const QualifiedName&);

protected:
    HTMLDocument(Frame*, const URL&, DocumentClassFlags = 0, unsigned constructionFlags = 0);

private:
    virtual PassRefPtr<Element> createElement(const AtomicString& tagName, ExceptionCode&);

    virtual bool isFrameSet() const;
    virtual PassRefPtr<DocumentParser> createParser();

    virtual PassRefPtr<Document> cloneDocumentWithoutChildren() const override final;

    DocumentOrderedMap m_documentNamedItem;
    DocumentOrderedMap m_windowNamedItem;
};

inline bool isHTMLDocument(const Document& document) { return document.isHTMLDocument(); }
void isHTMLDocument(const HTMLDocument&); // Catch unnecessary runtime check of type known at compile time.

DOCUMENT_TYPE_CASTS(HTMLDocument)

} // namespace WebCore

#endif // HTMLDocument_h
