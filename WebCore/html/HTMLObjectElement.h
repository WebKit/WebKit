/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef HTMLObjectElement_h
#define HTMLObjectElement_h

#include "HTMLPlugInElement.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class HTMLImageLoader;

class HTMLObjectElement : public HTMLPlugInElement {
public:
    HTMLObjectElement(Document*, bool createdByParser);
    ~HTMLObjectElement();

    virtual int tagPriority() const { return 5; }

    virtual void parseMappedAttribute(MappedAttribute*);

    virtual void attach();
    virtual bool rendererIsNeeded(RenderStyle*);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual void finishParsingChildren();
    virtual void detach();
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    
    virtual void recalcStyle(StyleChange);
    virtual void childrenChanged(bool changedByParser = false);

    virtual bool isURLAttribute(Attribute*) const;
    virtual const QualifiedName& imageSourceAttributeName() const;

    virtual void updateWidget();
    void setNeedWidgetUpdate(bool needWidgetUpdate) { m_needWidgetUpdate = needWidgetUpdate; }

    bool isImageType();

    void renderFallbackContent();

#if USE(JAVASCRIPTCORE_BINDINGS)
    virtual KJS::Bindings::Instance* getInstance() const;
#endif

    String archive() const;
    void setArchive(const String&);

    String border() const;
    void setBorder(const String&);

    String code() const;
    void setCode(const String&);
    
    String codeBase() const;
    void setCodeBase(const String&);

    String codeType() const;
    void setCodeType(const String&);
    
    String data() const;
    void setData(const String&);

    bool declare() const;
    void setDeclare(bool);

    int hspace() const;
    void setHspace(int);

    String standby() const;
    void setStandby(const String&);

    void setTabIndex(int);

    String type() const;
    void setType(const String&);

    String useMap() const;
    void setUseMap(const String&);

    int vspace() const;
    void setVspace(int);

    bool isDocNamedItem() const { return m_docNamedItem; }

    bool containsJavaApplet() const;

    String m_serviceType;
    String m_url;
    String m_classId;
    bool m_needWidgetUpdate : 1;
    bool m_useFallbackContent : 1;
    OwnPtr<HTMLImageLoader> m_imageLoader;

private:
    void updateDocNamedItem();
    String oldIdAttr;
    bool m_docNamedItem;
};

}

#endif
