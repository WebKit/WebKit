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

#include "Node.h"
#include "PlatformMouseEvent.h"

namespace WebCore {

class MouseEventWithHitTestResults {
public:
    MouseEventWithHitTestResults(const PlatformMouseEvent& e, const String& u, const String& t, PassRefPtr<Node> n)
        : m_event(e), m_url(u), m_target(t), m_innerNode(n) { }

    const PlatformMouseEvent& event() const { return m_event; }
    String url() const { return m_url; }
    String target() const { return m_target; }
    Node* innerNode() const { return m_innerNode.get(); }

private:
    PlatformMouseEvent m_event;
    String m_url;
    String m_target;
    RefPtr<Node> m_innerNode;
};

}

#endif
