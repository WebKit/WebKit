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
class CSSRuleImpl;
class CSSRuleListImpl;
class CSSStyleRuleImpl;
class CSSStyleSheet;
class CSSValueImpl;
class CachedCSSStyleSheet;
class DocLoader;
class DocumentImpl;
class MediaListImpl;
class NodeImpl;
class StyleSheet;

typedef int ExceptionCode;

class StyleSheetImpl : public StyleListImpl {
public:
    StyleSheetImpl(NodeImpl *ownerNode, String href = String());
    StyleSheetImpl(StyleSheetImpl *parentSheet, String href = String());
    StyleSheetImpl(StyleBaseImpl *owner, String href = String());
    StyleSheetImpl(CachedCSSStyleSheet *cached, String href = String());
    virtual ~StyleSheetImpl();

    virtual bool isStyleSheet() const { return true; }

    virtual String type() const { return String(); }

    bool disabled() const { return m_disabled; }
    void setDisabled(bool disabled) { m_disabled = disabled; styleSheetChanged(); }

    NodeImpl *ownerNode() const { return m_parentNode; }
    StyleSheetImpl *parentStyleSheet() const;
    String href() const { return m_strHref; }
    String title() const { return m_strTitle; }
    MediaListImpl* media() const { return m_media.get(); }
    void setMedia(MediaListImpl*);

    virtual bool isLoading() { return false; }

    virtual void styleSheetChanged() { }
    
protected:
    NodeImpl *m_parentNode;
    String m_strHref;
    String m_strTitle;
    RefPtr<MediaListImpl> m_media;
    bool m_disabled;
};

class CSSStyleSheetImpl : public StyleSheetImpl
{
public:
    CSSStyleSheetImpl(NodeImpl *parentNode, String href = String(), bool _implicit = false);
    CSSStyleSheetImpl(CSSStyleSheetImpl *parentSheet, String href = String());
    CSSStyleSheetImpl(CSSRuleImpl *ownerRule, String href = String());
    
    ~CSSStyleSheetImpl() { delete m_namespaces; }
    
    virtual bool isCSSStyleSheet() const { return true; }

    virtual String type() const { return "text/css"; }

    CSSRuleImpl *ownerRule() const;
    CSSRuleListImpl *cssRules();
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
    DocumentImpl* doc() { return m_doc; }
    bool implicit() { return m_implicit; }

protected:
    DocumentImpl* m_doc;
    bool m_implicit;
    CSSNamespace* m_namespaces;
};

// ----------------------------------------------------------------------------

class StyleSheetListImpl : public Shared<StyleSheetListImpl>
{
public:
    ~StyleSheetListImpl();

    // the following two ignore implicit stylesheets
    unsigned length() const;
    StyleSheetImpl* item(unsigned index);

    void add(StyleSheetImpl* s);
    void remove(StyleSheetImpl* s);

    QPtrList<StyleSheetImpl> styleSheets;
};

// ----------------------------------------------------------------------------

class MediaListImpl : public StyleBaseImpl
{
public:
    MediaListImpl() : StyleBaseImpl(0) {}
    MediaListImpl(CSSStyleSheetImpl* parentSheet) : StyleBaseImpl(parentSheet) {}
    MediaListImpl(CSSStyleSheetImpl* parentSheet, const String& media);
    MediaListImpl(CSSRuleImpl* parentRule, const String& media);

    virtual bool isMediaList() { return true; }

    CSSStyleSheetImpl* parentStyleSheet() const;
    CSSRuleImpl* parentRule() const;
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
    QValueList<String> m_lstMedia;
};

} // namespace

#endif
