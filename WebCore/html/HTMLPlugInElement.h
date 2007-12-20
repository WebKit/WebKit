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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef HTMLPlugInElement_h
#define HTMLPlugInElement_h

#include "HTMLFrameOwnerElement.h"

#if USE(JAVASCRIPTCORE_BINDINGS)
#include <bindings/runtime.h>
#endif

#if USE(NPOBJECT)
#include <bindings/npruntime_internal.h>
#endif

namespace WebCore {

class HTMLPlugInElement : public HTMLFrameOwnerElement {
public:
    HTMLPlugInElement(const QualifiedName& tagName, Document*);
    virtual ~HTMLPlugInElement();

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute*);

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual bool checkDTD(const Node* newChild);

    virtual void updateWidget() { }

    String align() const;
    void setAlign(const String&);
    
    String height() const;
    void setHeight(const String&);
    
    String name() const;
    void setName(const String&);
    
    String width() const;
    void setWidth(const String&);

#if USE(JAVASCRIPTCORE_BINDINGS)
    virtual KJS::Bindings::Instance* getInstance() const = 0;
#endif
#if USE(NPOBJECT)
    virtual NPObject* getNPObject();
#endif

    virtual void defaultEventHandler(Event*);
private:
#if USE(NPOBJECT)
    NPObject* createNPObject();
#endif

protected:
    static void updateWidgetCallback(Node*);

    String oldNameAttr;
#if USE(JAVASCRIPTCORE_BINDINGS)
    mutable RefPtr<KJS::Bindings::Instance> m_instance;
#endif
#if USE(NPOBJECT)
    NPObject* m_NPObject;
#endif
};

} // namespace WebCore

#endif // HTMLPlugInElement_h
