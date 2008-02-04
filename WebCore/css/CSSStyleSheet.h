/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef CSSStyleSheet_h
#define CSSStyleSheet_h

#include "StyleSheet.h"

namespace WebCore {

class CSSNamespace;
class CSSParser;
class CSSRule;
class CSSRuleList;
class DocLoader;
class Document;

typedef int ExceptionCode;

class CSSStyleSheet : public StyleSheet {
public:
    CSSStyleSheet(Node* parentNode, const String& href = String(), const String& charset = String());
    CSSStyleSheet(CSSStyleSheet* parentSheet, const String& href = String(), const String& charset = String());
    CSSStyleSheet(CSSRule* ownerRule, const String& href = String(), const String& charset = String());
    
    ~CSSStyleSheet();
    
    virtual bool isCSSStyleSheet() const { return true; }

    virtual String type() const { return "text/css"; }

    CSSRule* ownerRule() const;
    CSSRuleList* cssRules(bool omitCharsetRules = false);
    unsigned insertRule(const String& rule, unsigned index, ExceptionCode&);
    void deleteRule(unsigned index, ExceptionCode&);

    // IE Extensions
    CSSRuleList* rules() { return cssRules(true); }
    int addRule(const String& selector, const String& style, int index, ExceptionCode&);
    int addRule(const String& selector, const String& style, ExceptionCode&);
    void removeRule(unsigned index, ExceptionCode& ec) { deleteRule(index, ec); }

    void addNamespace(CSSParser*, const AtomicString& prefix, const AtomicString& uri);
    const AtomicString& determineNamespace(const AtomicString& prefix);
    
    virtual void styleSheetChanged();

    virtual bool parseString(const String&, bool strict = true);

    virtual bool isLoading();

    virtual void checkLoaded();
    DocLoader* docLoader();
    Document* doc() { return m_doc; }
    const String& charset() const { return m_charset; }

    bool loadCompleted() const { return m_loadCompleted; }

protected:
    Document* m_doc;
    CSSNamespace* m_namespaces;
    String m_charset;
    bool m_loadCompleted;
};

} // namespace

#endif
