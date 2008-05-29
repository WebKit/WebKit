/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef HTMLEmbedElement_h
#define HTMLEmbedElement_h

#include "HTMLPlugInElement.h"

#if USE(JAVASCRIPTCORE_BINDINGS)
namespace KJS {
    namespace Bindings {
        class Instance;
    }
}
#endif

namespace WebCore {

class HTMLEmbedElement : public HTMLPlugInElement {
public:
    HTMLEmbedElement(Document*);
    ~HTMLEmbedElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusForbidden; }
    virtual int tagPriority() const { return 0; }

    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void parseMappedAttribute(MappedAttribute*);

    virtual void attach();
    virtual bool canLazyAttach() { return false; }
    virtual void detach();
    virtual bool rendererIsNeeded(RenderStyle*);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    virtual void attributeChanged(Attribute*, bool preserveDecls = false);
    
    virtual bool isURLAttribute(Attribute*) const;

    virtual void updateWidget();
    void setNeedWidgetUpdate(bool needWidgetUpdate) { m_needWidgetUpdate = needWidgetUpdate; }

#if USE(JAVASCRIPTCORE_BINDINGS)
    virtual KJS::Bindings::Instance* getInstance() const;
#endif

    String src() const;
    void setSrc(const String&);

    String type() const;
    void setType(const String&);

    const String& url() const { return m_url; }
    const String& serviceType() const { return m_serviceType; }

    virtual void getSubresourceAttributeStrings(Vector<String>&) const;

private:
    String m_url;
    String m_pluginPage;
    String m_serviceType;
    bool m_needWidgetUpdate;
};

}

#endif
