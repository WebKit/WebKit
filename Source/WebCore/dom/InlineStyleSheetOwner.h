/*
 * Copyright (C) 2006, 2007 Rob Buis
 * Copyright (C) 2013 Apple, Inc. All rights reserved.
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

#ifndef InlineStyleSheetOwner_h
#define InlineStyleSheetOwner_h

#include "CSSStyleSheet.h"
#include <wtf/text/TextPosition.h>

namespace WebCore {

class Document;
class Element;

class InlineStyleSheetOwner {
public:
    InlineStyleSheetOwner(Document&, bool createdByParser);
    ~InlineStyleSheetOwner();

    void setContentType(const AtomicString& contentType) { m_contentType = contentType; }
    void setMedia(const AtomicString& media) { m_media = media; }

    CSSStyleSheet* sheet() const { return m_sheet.get(); }

    bool isLoading() const;
    bool sheetLoaded(Document&);
    void startLoadingDynamicSheet(Document&);

    void insertedIntoDocument(Document&, Element&);
    void removedFromDocument(Document&, Element&);
    void clearDocumentData(Document&, Element&);
    void childrenChanged(Element&);
    void finishParsingChildren(Element&);

private:
    void createSheet(Element&, const String& text);
    void createSheetFromTextContents(Element&);
    void clearSheet();

    bool m_isParsingChildren;
    bool m_loading;
    WTF::OrdinalNumber m_startLineNumber;
    AtomicString m_contentType;
    AtomicString m_media;
    RefPtr<CSSStyleSheet> m_sheet;
};

}

#endif
