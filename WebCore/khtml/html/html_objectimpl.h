/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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
#ifndef HTML_OBJECTIMPL_H
#define HTML_OBJECTIMPL_H

#include "html_imageimpl.h"
#include "xml/dom_stringimpl.h"

#if __APPLE__
#include <JavaScriptCore/runtime.h>
#else
namespace KJS { namespace Bindings { class Instance; } }
#endif

class QStringList;

namespace DOM {

class HTMLFormElementImpl;

class HTMLAppletElementImpl : public HTMLElementImpl
{
public:
    HTMLAppletElementImpl(DocumentImpl *doc);
    ~HTMLAppletElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }
    virtual bool checkDTD(const NodeImpl* newChild);

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl *token);
    
    virtual bool rendererIsNeeded(khtml::RenderStyle *);
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);
    virtual void closeRenderer();
    virtual void detach();
    
    DOMString align() const;
    void setAlign(const DOMString &);

    DOMString alt() const;
    void setAlt(const DOMString &);

    DOMString archive() const;
    void setArchive(const DOMString &);

    DOMString code() const;
    void setCode(const DOMString &);

    DOMString codeBase() const;
    void setCodeBase(const DOMString &);

    DOMString height() const;
    void setHeight(const DOMString &);

    DOMString hspace() const;
    void setHspace(const DOMString &);

    DOMString name() const;
    void setName(const DOMString &);

    DOMString object() const;
    void setObject(const DOMString &);

    DOMString vspace() const;
    void setVspace(const DOMString &);

    DOMString width() const;
    void setWidth(const DOMString &);

    virtual bool allParamsAvailable();
    void setupApplet() const;

    KJS::Bindings::Instance *getAppletInstance() const;

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

private:
    DOMString oldNameAttr;
    DOMString oldIdAttr;

    mutable KJS::Bindings::Instance *appletInstance;

    bool m_allParamsAvailable;
};

// -------------------------------------------------------------------------

class HTMLEmbedElementImpl : public HTMLElementImpl
{
public:
    HTMLEmbedElementImpl(DocumentImpl *doc);
    ~HTMLEmbedElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 0; }
    virtual bool checkDTD(const NodeImpl* newChild);

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl *attr);

    virtual void attach();
    virtual void detach();
    virtual bool rendererIsNeeded(khtml::RenderStyle *);
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    
    virtual bool isURLAttribute(AttributeImpl *attr) const;

    KJS::Bindings::Instance *getEmbedInstance() const;

    QString url;
    QString pluginPage;
    QString serviceType;

private:
    DOMString oldNameAttr;

    mutable KJS::Bindings::Instance *embedInstance;
};

// -------------------------------------------------------------------------

class HTMLObjectElementImpl : public HTMLElementImpl
{
public:
    HTMLObjectElementImpl(DocumentImpl *doc);
    ~HTMLObjectElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 7; }
    virtual bool checkDTD(const NodeImpl* newChild);

    HTMLFormElementImpl *form() const;

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttributeImpl *token);

    virtual void attach();
    virtual bool rendererIsNeeded(khtml::RenderStyle *);
    virtual khtml::RenderObject *createRenderer(RenderArena *, khtml::RenderStyle *);
    virtual void closeRenderer();
    virtual void detach();
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    
    virtual void recalcStyle(StyleChange ch);
    virtual void childrenChanged();

    DocumentImpl* contentDocument() const;
    
    virtual bool isURLAttribute(AttributeImpl *attr) const;

    bool isImageType();

    void renderFallbackContent();

    DOMString code() const;
    void setCode(const DOMString &);

    DOMString align() const;
    void setAlign(const DOMString &);

    DOMString archive() const;
    void setArchive(const DOMString &);

    DOMString border() const;
    void setBorder(const DOMString &);

    DOMString codeBase() const;
    void setCodeBase(const DOMString &);

    DOMString codeType() const;
    void setCodeType(const DOMString &);

    DOMString data() const;
    void setData(const DOMString &);

    bool declare() const;
    void setDeclare(bool);

    DOMString height() const;
    void setHeight(const DOMString &);

    DOMString hspace() const;
    void setHspace(const DOMString &);

    DOMString name() const;
    void setName(const DOMString &);

    DOMString standby() const;
    void setStandby(const DOMString &);

    int tabIndex() const;
    void setTabIndex(int);

    DOMString type() const;
    void setType(const DOMString &);

    DOMString useMap() const;
    void setUseMap(const DOMString &);

    DOMString vspace() const;
    void setVspace(const DOMString &);

    DOMString width() const;
    void setWidth(const DOMString &);

    bool isComplete() const { return m_complete; }
    void setComplete(bool complete);
    
    bool isDocNamedItem() const { return m_docNamedItem; }

    KJS::Bindings::Instance *getObjectInstance() const;

    QString serviceType;
    QString url;
    DOM::DOMString classId;
    bool needWidgetUpdate : 1;
    bool m_useFallbackContent : 1;
    HTMLImageLoader* m_imageLoader;

private:
    void updateDocNamedItem();
    DOMString oldIdAttr;
    DOMString oldNameAttr;

    mutable KJS::Bindings::Instance *objectInstance;

    bool m_complete;
    bool m_docNamedItem;
};

// -------------------------------------------------------------------------

class HTMLParamElementImpl : public HTMLElementImpl
{
    friend class HTMLAppletElementImpl;
public:
    HTMLParamElementImpl(DocumentImpl *doc);
    ~HTMLParamElementImpl();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    virtual void parseMappedAttribute(MappedAttributeImpl *token);

    virtual bool isURLAttribute(AttributeImpl *attr) const;

    DOMString name() const { return m_name; }
    void setName(const DOMString &);

    DOMString type() const;
    void setType(const DOMString &);

    DOMString value() const { return m_value; }
    void setValue(const DOMString &);

    DOMString valueType() const;
    void setValueType(const DOMString &);

 protected:
    AtomicString m_name;
    AtomicString m_value;
};

}

#endif
