/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
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

#ifndef HTMLFrameElementBase_h
#define HTMLFrameElementBase_h

#include "HTMLElement.h"
#include "ScrollBarMode.h"

namespace WebCore {

class Frame;
class HTMLFrameSetElement;

class HTMLFrameElementBase : public HTMLElement
{
    friend class RenderFrame;
    friend class RenderPartObject;

public:
    HTMLFrameElementBase(const QualifiedName&, Document*);

    virtual void parseMappedAttribute(MappedAttribute*);

    void insertedIntoDocument();
    virtual void willRemove();

    virtual void attach();

    void setLocation(const String&);

    virtual bool isFocusable() const;
    virtual void setFocus(bool);

    Frame* contentFrame() const;
    Document* contentDocument() const;
    HTMLFrameSetElement* containingFrameSetElement() const;
    
    virtual bool isURLAttribute(Attribute*) const;

    ScrollbarMode scrollingMode() const { return m_scrolling; }
    int getMarginWidth() const { return m_marginWidth; }
    int getMarginHeight() const { return m_marginHeight; }

    String frameBorder() const;
    void setFrameBorder(const String&);

    String longDesc() const;
    void setLongDesc(const String&);

    String marginHeight() const;
    void setMarginHeight(const String&);

    String marginWidth() const;
    void setMarginWidth(const String&);

    String name() const;
    void setName(const String&);

    void setNoResize(bool);
    bool noResize() { return m_noResize; }

    String scrolling() const;
    void setScrolling(const String&);

    virtual String src() const;
    void setSrc(const String&);

    int width() const;
    int height() const;

    bool viewSourceMode() const { return m_viewSource; }

protected:
    bool isURLAllowed(const AtomicString&) const;
    void openURL();

    static void openURLCallback(Node*);

    AtomicString m_URL;
    AtomicString m_name;

    ScrollbarMode m_scrolling;

    int m_marginWidth;
    int m_marginHeight;

    bool m_frameBorder : 1;
    bool m_frameBorderSet : 1;
    bool m_noResize : 1;
    bool m_viewSource : 1;
};

} // namespace WebCore

#endif // HTMLFrameElementBase_h
