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

#ifndef HTMLEmbedElement_H
#define HTMLEmbedElement_H

#include "HTMLPlugInElement.h"

#if __APPLE__
#include <JavaScriptCore/runtime.h>
#else
namespace KJS { namespace Bindings { class Instance; } }
#endif

namespace WebCore {

class HTMLEmbedElement : public HTMLPlugInElement
{
public:
    HTMLEmbedElement(Document*);
    ~HTMLEmbedElement();

    virtual int tagPriority() const { return 0; }

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
    virtual KJS::Bindings::Instance* getInstance() const;
#endif

    String src() const;
    void setSrc(const String&);

    String type() const;
    void setType(const String&);

    DeprecatedString url;
    DeprecatedString pluginPage;
    DeprecatedString serviceType;
};

}

#endif
