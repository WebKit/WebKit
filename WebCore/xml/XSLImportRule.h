/*
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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
 *
 */

#ifndef XSLImportRule_H
#define XSLImportRule_H

#ifdef KHTML_XSLT

#include "CachedResourceClient.h"
#include "StyleBase.h"
#include "XSLStyleSheet.h"

namespace WebCore {

class CachedXSLStyleSheet;

class XSLImportRule : public CachedResourceClient, public StyleBase {
public:
    XSLImportRule(StyleBase* parent, const String& href);
    virtual ~XSLImportRule();
    
    String href() const { return m_strHref; }
    XSLStyleSheet* styleSheet() const { return m_styleSheet.get(); }
    
    virtual bool isImportRule() { return true; }
    XSLStyleSheet* parentStyleSheet() const;
    
    // from CachedResourceClient
    virtual void setStyleSheet(const String& url, const String& sheet);
    
    bool isLoading();
    void loadSheet();
    
protected:
    String m_strHref;
    RefPtr<XSLStyleSheet> m_styleSheet;
    CachedXSLStyleSheet* m_cachedSheet;
    bool m_loading;
};

} // namespace WebCore

#endif // KHTML_XSLT

#endif // XSLImportRule_H
