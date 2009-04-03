/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
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

#ifndef HTMLFrameElementBase_h
#define HTMLFrameElementBase_h

#include "HTMLFrameOwnerElement.h"
#include "ScrollTypes.h"

namespace WebCore {

class HTMLFrameElementBase : public HTMLFrameOwnerElement {
public:
    virtual void parseMappedAttribute(MappedAttribute*);

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    virtual void attach();
    virtual bool canLazyAttach() { return false; }

    KURL location() const;
    void setLocation(const String&);

    virtual bool isFocusable() const;
    virtual void setFocus(bool);
    
    virtual bool isURLAttribute(Attribute*) const;

    virtual ScrollbarMode scrollingMode() const { return m_scrolling; }
    
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

    bool noResize() const { return m_noResize; }
    void setNoResize(bool);

    String scrolling() const;
    void setScrolling(const String&);

    KURL src() const;
    void setSrc(const String&);

    int width() const;
    int height() const;

    bool viewSourceMode() const { return m_viewSource; }

protected:
    HTMLFrameElementBase(const QualifiedName&, Document*);

    bool isURLAllowed(const AtomicString&) const;
    void setNameAndOpenURL();
    void openURL();

    static void setNameAndOpenURLCallback(Node*);

    AtomicString m_URL;
    AtomicString m_frameName;

    ScrollbarMode m_scrolling;

    int m_marginWidth;
    int m_marginHeight;

    bool m_noResize;
    bool m_viewSource;

    bool m_shouldOpenURLAfterAttach;
};

} // namespace WebCore

#endif // HTMLFrameElementBase_h
