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

#ifndef HTMLObjectElement_H
#define HTMLObjectElement_H

#include "HTMLPlugInElement.h"

#if USE(JAVASCRIPTCORE_BINDINGS)
#include <JavaScriptCore/runtime.h>
#else
namespace KJS { namespace Bindings { class Instance; } }
#endif

namespace WebCore {

class HTMLFormElement;
class HTMLImageLoader;
#ifdef SVG_SUPPORT
class SVGDocument;
#endif

class HTMLObjectElement : public HTMLPlugInElement
{
public:
    HTMLObjectElement(Document*);
    ~HTMLObjectElement();

    virtual int tagPriority() const { return 7; }

    virtual void parseMappedAttribute(MappedAttribute*);

    virtual void attach();
    virtual bool rendererIsNeeded(RenderStyle*);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual void closeRenderer();
    virtual void detach();
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    
    virtual void recalcStyle(StyleChange);
    virtual void childrenChanged();

    virtual bool isURLAttribute(Attribute*) const;

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

    HTMLFormElement* form() const;
    
    int hspace() const;
    void setHspace(int);

    String standby() const;
    void setStandby(const String&);

    int tabIndex() const;
    void setTabIndex(int);

    String type() const;
    void setType(const String&);

    String useMap() const;
    void setUseMap(const String&);

    int vspace() const;
    void setVspace(int);

    bool isComplete() const { return m_complete; }
    void setComplete(bool complete);
    
    bool isDocNamedItem() const { return m_docNamedItem; }

#ifdef SVG_SUPPORT
    SVGDocument* getSVGDocument(ExceptionCode&) const;
#endif

    String m_serviceType;
    String m_url;
    String m_classId;
    bool m_needWidgetUpdate : 1;
    bool m_useFallbackContent : 1;
    HTMLImageLoader* m_imageLoader;

private:
    void updateDocNamedItem();
    String oldIdAttr;
    bool m_complete;
    bool m_docNamedItem;
};

}

#endif
