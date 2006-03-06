/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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

#ifndef DOM_TextImpl_h
#define DOM_TextImpl_h

#include "CharacterDataImpl.h"

namespace WebCore {

class TextImpl : public CharacterDataImpl
{
public:
    TextImpl(DocumentImpl *impl, const DOMString &_text);
    TextImpl(DocumentImpl *impl);
    virtual ~TextImpl();

    // DOM methods & attributes for CharacterData

    TextImpl *splitText ( const unsigned offset, ExceptionCode&);

    // DOM methods overridden from  parent classes
    const AtomicString& localName() const;
    virtual DOMString nodeName() const;
    virtual NodeType nodeType() const;
    virtual PassRefPtr<NodeImpl> cloneNode(bool deep);

    // Other methods (not part of DOM)

    virtual bool isTextNode() const { return true; }
    virtual void attach();
    virtual bool rendererIsNeeded(RenderStyle *);
    virtual RenderObject *createRenderer(RenderArena *, RenderStyle *);
    virtual void recalcStyle( StyleChange = NoChange );
    virtual bool childTypeAllowed(NodeType);

    virtual DOMString toString() const;

#ifndef NDEBUG
    virtual void formatForDebugger(char *buffer, unsigned length) const;
#endif

protected:
    virtual TextImpl* createNew(DOMStringImpl*);
};

} // namespace WebCore

#endif // DOM_TextImpl_h
