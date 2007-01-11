/* This file is part of the KDE project
   Copyright (C) 2000 Simon Hausmann <hausmann@kde.org>
   Copyright (C) 2006 Apple Computer, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef MouseEventWithHitTestResults_h
#define MouseEventWithHitTestResults_h

#include "Element.h"
#include "PlatformMouseEvent.h"

namespace WebCore {

class PlatformScrollbar;

// FIXME: Why doesn't this class just cache a HitTestResult instead of copying all of HitTestResult's fields over?
class MouseEventWithHitTestResults {
public:
    MouseEventWithHitTestResults(const PlatformMouseEvent&, PassRefPtr<Node>, const IntPoint& localPoint, PlatformScrollbar*, bool isOverLink);

    const PlatformMouseEvent& event() const { return m_event; }
    Node* targetNode() const;
    const IntPoint& localPoint() const { return m_localPoint; }
    PlatformScrollbar* scrollbar() const { return m_scrollbar; }
    bool isOverLink() const { return m_isOverLink; }

private:
    PlatformMouseEvent m_event;
    RefPtr<Node> m_targetNode;
    RefPtr<Element> m_targetElement;
    IntPoint m_localPoint;
    PlatformScrollbar* m_scrollbar;
    bool m_isOverLink;
};

}

#endif
