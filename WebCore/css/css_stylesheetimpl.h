/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
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
 */

#ifndef CSS_css_stylesheetimpl_h_
#define CSS_css_stylesheetimpl_h_

#include "css_base.h"
#include <qptrlist.h>
#include <qvaluelist.h>

namespace WebCore {

class CSSParser;
class CSSRule;
class CSSRuleList;
class CSSStyleRule;
class CSSStyleSheet;
class CSSValue;
class CachedCSSStyleSheet;
class DocLoader;
class Document;
class MediaList;
class Node;
class StyleSheet;

typedef int ExceptionCode;

class StyleSheet : public StyleList {
public:
    StyleSheet(Node *ownerNode, String href = String());
    StyleSheet(StyleSheet *parentSheet, String href = String());
    StyleSheet(StyleBase *owner, String href = String());
    StyleSheet(CachedCSSStyleSheet *cached, String href = String());
    virtual ~StyleSheet();

    virtual bool isStyleSheet() const { return true; }

    virtual String type() const { return String(); }

    bool disabled() const { return m_disabled; }
    void setDisabled(bool disabled) { m_disabled = disabled; styleSheetChanged(); }

    Node *ownerNode() const { return m_parentNode; }
    StyleSheet *parentStyleSheet() const;
    String href() const { return m_strHref; }
    String title() const { return m_strTitle; }
    MediaList* media() const { return m_media.get(); }
    void setMedia(MediaList*);

    virtual bool isLoading() { return false; }

    virtual void styleSheetChanged() { }
    
protected:
    Node *m_parentNode;
    String m_strHref;
    String m_strTitle;
    RefPtr<MediaList> m_media;
    bool m_disabled;
};

class CSSStyleSheet : public StyleSheet
{
public:
    CSSStyleSheet(Node *parentNode, String href = String(), bool _implicit = false);
    CSSStyleSheet(CSSStyleSheet *parentSheet, String href = String());
    CSSStyleSheet(CSSRule *ownerRule, String href = String());
    
    ~CSSStyleSheet() { delete m_namespaces; }
    
    virtual bool isCSSStyleSheet() const { return true; }

    virtual String type() const { return "text/css"; }

    CSSRule *ownerRule() const;
    CSSRuleList *cssRules();
    unsigned insertRule(const String &rule, unsigned index, ExceptionCode&);
    void deleteRule(unsigned index, ExceptionCode&);
    unsigned addRule(const String &selector, const String &style, int index, ExceptionCode&);
    void removeRule(unsigned index, ExceptionCode& ec) { deleteRule(index, ec); }
    
    void addNamespace(CSSParser* p, const AtomicString& prefix, const AtomicString& uri);
    const AtomicString& determineNamespace(const AtomicString& prefix);
    
    virtual void styleSheetChanged();

    virtual bool parseString(const String&, bool strict = true);

    virtual bool isLoading();

    virtual void checkLoaded();
    DocLoader* docLoader();
    Document* doc() { return m_doc; }
    bool implicit() { return m_implicit; }

protected:
    Document* m_doc;
    bool m_implicit;
    CSSNamespace* m_namespaces;
};

// ----------------------------------------------------------------------------

class StyleSheetList : public Shared<StyleSheetList>
{
public:
    ~StyleSheetList();

    // the following two ignore implicit stylesheets
    unsigned length() const;
    StyleSheet* item(unsigned index);

    void add(StyleSheet* s);
    void remove(StyleSheet* s);

    DeprecatedPtrList<StyleSheet> styleSheets;
};

// ----------------------------------------------------------------------------

class MediaList : public StyleBase
{
public:
    MediaList() : StyleBase(0) {}
    MediaList(CSSStyleSheet* parentSheet) : StyleBase(parentSheet) {}
    MediaList(CSSStyleSheet* parentSheet, const String& media);
    MediaList(CSSRule* parentRule, const String& media);

    virtual bool isMediaList() { return true; }

    CSSStyleSheet* parentStyleSheet() const;
    CSSRule* parentRule() const;
    unsigned length() const { return m_lstMedia.count(); }
    String item(unsigned index) const { return m_lstMedia[index]; }
    void deleteMedium(const String& oldMedium);
    void appendMedium(const String& newMedium) { m_lstMedia.append(newMedium); }

    String mediaText() const;
    void setMediaText(const String&);

    /**
     * Check if the list contains either the requested medium, or the
     * catch-all "all" media type. Returns true when found, false otherwise.
     * Since not specifying media types should be treated as "all" according
     * to DOM specs, an empty list always returns true.
     *
     * _NOT_ part of the DOM!
     */
    bool contains(const String& medium) const;

protected:
    DeprecatedValueList<String> m_lstMedia;
};

} // namespace

#endif
