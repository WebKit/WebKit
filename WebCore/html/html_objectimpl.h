/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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

#ifndef HTML_OBJECTIMPL_H
#define HTML_OBJECTIMPL_H

#include "HTMLElement.h"

#if __APPLE__
#include <JavaScriptCore/runtime.h>
#else
namespace KJS { namespace Bindings { class Instance; } }
#endif

namespace WebCore {

class HTMLFormElement;
class HTMLImageLoader;

class HTMLAppletElement : public HTMLElement
{
public:
    HTMLAppletElement(Document*);
    ~HTMLAppletElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }
    virtual bool checkDTD(const Node* newChild);

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute*);
    
    virtual bool rendererIsNeeded(RenderStyle*);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual void closeRenderer();
    virtual void detach();
    
    String align() const;
    void setAlign(const String&);

    String alt() const;
    void setAlt(const String&);

    String archive() const;
    void setArchive(const String&);

    String code() const;
    void setCode(const String&);

    String codeBase() const;
    void setCodeBase(const String&);

    String height() const;
    void setHeight(const String&);

    String hspace() const;
    void setHspace(const String&);

    String name() const;
    void setName(const String&);

    String object() const;
    void setObject(const String&);

    String vspace() const;
    void setVspace(const String&);

    String width() const;
    void setWidth(const String&);

    virtual bool allParamsAvailable();
    void setupApplet() const;

#if __APPLE__
    KJS::Bindings::Instance* getAppletInstance() const;
#endif

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

private:
    String oldNameAttr;
    String oldIdAttr;

#if __APPLE__
    mutable RefPtr<KJS::Bindings::Instance> m_appletInstance;
#endif

    bool m_allParamsAvailable;
};

// -------------------------------------------------------------------------

class HTMLEmbedElement : public HTMLElement
{
public:
    HTMLEmbedElement(Document*);
    ~HTMLEmbedElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 0; }
    virtual bool checkDTD(const Node* newChild);

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute*);

    virtual void attach();
    virtual void detach();
    virtual bool rendererIsNeeded(RenderStyle*);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    
    virtual bool isURLAttribute(Attribute*) const;

#if __APPLE__
    KJS::Bindings::Instance* getEmbedInstance() const;
#endif

    DeprecatedString url;
    DeprecatedString pluginPage;
    DeprecatedString serviceType;

private:
    String oldNameAttr;

#if __APPLE__
    mutable RefPtr<KJS::Bindings::Instance> m_embedInstance;
#endif
};

// -------------------------------------------------------------------------

class HTMLObjectElement : public HTMLElement
{
public:
    HTMLObjectElement(Document*);
    ~HTMLObjectElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 7; }
    virtual bool checkDTD(const Node* newChild);

    HTMLFormElement* form() const;

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute*);

    virtual void attach();
    virtual bool rendererIsNeeded(RenderStyle*);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual void closeRenderer();
    virtual void detach();
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    
    virtual void recalcStyle(StyleChange ch);
    virtual void childrenChanged();

    Document* contentDocument() const;
    
    virtual bool isURLAttribute(Attribute*) const;

    bool isImageType();

    void renderFallbackContent();

    String code() const;
    void setCode(const String&);

    String align() const;
    void setAlign(const String&);

    String archive() const;
    void setArchive(const String&);

    String border() const;
    void setBorder(const String&);

    String codeBase() const;
    void setCodeBase(const String&);

    String codeType() const;
    void setCodeType(const String&);

    String data() const;
    void setData(const String&);

    bool declare() const;
    void setDeclare(bool);

    String height() const;
    void setHeight(const String&);

    String hspace() const;
    void setHspace(const String&);

    String name() const;
    void setName(const String&);

    String standby() const;
    void setStandby(const String&);

    int tabIndex() const;
    void setTabIndex(int);

    String type() const;
    void setType(const String&);

    String useMap() const;
    void setUseMap(const String&);

    String vspace() const;
    void setVspace(const String&);

    String width() const;
    void setWidth(const String&);

    bool isComplete() const { return m_complete; }
    void setComplete(bool complete);
    
    bool isDocNamedItem() const { return m_docNamedItem; }

#if __APPLE__
    KJS::Bindings::Instance* getObjectInstance() const;
#endif

    DeprecatedString serviceType;
    DeprecatedString url;
    WebCore::String classId;
    bool needWidgetUpdate : 1;
    bool m_useFallbackContent : 1;
    HTMLImageLoader* m_imageLoader;

private:
    void updateDocNamedItem();
    String oldIdAttr;
    String oldNameAttr;

#if __APPLE__
    mutable RefPtr<KJS::Bindings::Instance> m_objectInstance;
#endif

    bool m_complete;
    bool m_docNamedItem;
};

// -------------------------------------------------------------------------

class HTMLParamElement : public HTMLElement
{
    friend class HTMLAppletElement;
public:
    HTMLParamElement(Document*);
    ~HTMLParamElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    virtual void parseMappedAttribute(MappedAttribute*);

    virtual bool isURLAttribute(Attribute*) const;

    String name() const { return m_name; }
    void setName(const String&);

    String type() const;
    void setType(const String&);

    String value() const { return m_value; }
    void setValue(const String&);

    String valueType() const;
    void setValueType(const String&);

 protected:
    AtomicString m_name;
    AtomicString m_value;
};

}

#endif
