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

#include "khtml_events.h"
#include "rendering/render_object.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_position.h"

using namespace DOM;

namespace khtml {

class MouseEvent::MouseEventPrivate
{
};

MouseEvent::MouseEvent( const char *name, QMouseEvent *qmouseEvent, int x, int y,
                               const DOM::DOMString &url, const DOM::DOMString& target,
                               NodeImpl *innerNode )
: KParts::Event( name ), m_qmouseEvent( qmouseEvent ), m_x( x ), m_y( y ),
  m_url( url ), m_target(target), m_innerNode( innerNode )
{
  d = 0;
  if (innerNode && innerNode->renderer()) {
      // FIXME: For text nodes, for now, we get the absolute position from
      // the parent.
      NodeImpl *n = innerNode;
      if (n->isTextNode())
        n = n->parentNode();
      n->renderer()->absolutePosition(m_nodeAbsX, m_nodeAbsY);
  }
}

MouseEvent::~MouseEvent()
{
  delete d;
}

long MouseEvent::offset() const
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

const char *MousePressEvent::s_strMousePressEvent = "khtml/Events/MousePressEvent";
const char *MouseDoubleClickEvent::s_strMouseDoubleClickEvent = "khtml/Events/MouseDoubleClickEvent";
const char *MouseMoveEvent::s_strMouseMoveEvent = "khtml/Events/MouseMoveEvent";
const char *MouseReleaseEvent::s_strMouseReleaseEvent = "khtml/Events/MouseReleaseEvent";
const char *DrawContentsEvent::s_strDrawContentsEvent = "khtml/Events/DrawContentsEvent";

class DrawContentsEvent::DrawContentsEventPrivate
{
};

DrawContentsEvent::DrawContentsEvent( QPainter *painter, int clipx, int clipy, int clipw, int cliph )
  : KParts::Event( s_strDrawContentsEvent ), m_painter( painter ), m_clipx( clipx ), m_clipy( clipy ),
    m_clipw( clipw ), m_cliph( cliph )
{
  d = new DrawContentsEventPrivate;
}

DrawContentsEvent::~DrawContentsEvent()
{
  delete d;
}

}
