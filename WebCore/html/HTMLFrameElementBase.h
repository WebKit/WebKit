/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2004, 2006, 2008, 2009 Apple Inc. All rights reserved.
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
    KURL location() const;
    void setLocation(const String&);

    virtual ScrollbarMode scrollingMode() const { return m_scrolling; }
    
    int getMarginWidth() const { return m_marginWidth; }
    int getMarginHeight() const { return m_marginHeight; }

    int width() const;
    int height() const;

    void setRemainsAliveOnRemovalFromTree(bool);

protected:
    HTMLFrameElementBase(const QualifiedName&, Document*);

    bool isURLAllowed() const;

    virtual void parseMappedAttribute(MappedAttribute*);

    virtual void insertedIntoDocument();
    virtual void removedFromDocument();

    virtual void attach();

private:
    virtual bool canLazyAttach() { return false; }

    virtual bool supportsFocus() const;
    virtual void setFocus(bool);
    
    virtual bool isURLAttribute(Attribute*) const;

    virtual void willRemove();
    void checkAttachedTimerFired(Timer<HTMLFrameElementBase>*);

    bool viewSourceMode() const { return m_viewSource; }

    void setNameAndOpenURL();
    void openURL();

    static void setNameAndOpenURLCallback(Node*);

    AtomicString m_URL;
    AtomicString m_frameName;

    ScrollbarMode m_scrolling;

    int m_marginWidth;
    int m_marginHeight;

    Timer<HTMLFrameElementBase> m_checkAttachedTimer;

    bool m_viewSource;

    bool m_shouldOpenURLAfterAttach;

    bool m_remainsAliveOnRemovalFromTree;
};

} // namespace WebCore

#endif // HTMLFrameElementBase_h
