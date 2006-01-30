/* This file is part of the KDE project
   Copyright (C) 2000 Simon Hausmann <hausmann@kde.org>

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

#include "config.h"
#include "khtml_events.h"

#include "render_object.h"
#include "visible_position.h"

namespace WebCore {

MouseEvent::MouseEvent(QMouseEvent *qmouseEvent, int x, int y, const DOM::DOMString &url, const DOM::DOMString& target, NodeImpl *innerNode )
: m_qmouseEvent( qmouseEvent ), m_x( x ), m_y( y ),
  m_url( url ), m_target(target), m_innerNode( innerNode )
{
  if (innerNode && innerNode->renderer()) {
      // FIXME: For text nodes, for now, we get the absolute position from the parent.
      NodeImpl *n = innerNode;
      if (n->isTextNode())
        n = n->parentNode();
      n->renderer()->absolutePosition(m_nodeAbsX, m_nodeAbsY);
  }
}

int MouseEvent::offset() const
{
    Position pos;
    if (NodeImpl *inner = m_innerNode.get()) {
        // FIXME: Shouldn't be necessary to skip text nodes.
        if (inner->isTextNode())
            inner = inner->parentNode();
        if (inner->renderer())
            pos = inner->renderer()->positionForCoordinates(m_x, m_y).deepEquivalent();
    }
    return pos.offset();
}

DrawContentsEvent::DrawContentsEvent( QPainter *painter, int clipx, int clipy, int clipw, int cliph )
  : m_painter( painter ), m_clipx( clipx ), m_clipy( clipy ),
    m_clipw( clipw ), m_cliph( cliph )
{
}

}
