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

using namespace khtml;
using namespace DOM;

class khtml::MouseEvent::MouseEventPrivate
{
};

khtml::MouseEvent::MouseEvent( const char *name, QMouseEvent *qmouseEvent, int x, int y,
                               const DOM::DOMString &url, const DOM::DOMString& target,
	                const DOM::Node &innerNode )
: KParts::Event( name ), m_qmouseEvent( qmouseEvent ), m_x( x ), m_y( y ),
  m_url( url ), m_target(target), m_innerNode( innerNode )
{
  d = 0;
  if (innerNode.handle() && innerNode.handle()->renderer()) {
      // FIXME: For text nodes, for now, we get the absolute position from
      // the parent.
      DOM::Node n = innerNode;
      if (n.nodeType() == Node::TEXT_NODE)
        n = n.parentNode();
      n.handle()->renderer()->absolutePosition(m_nodeAbsX, m_nodeAbsY);
  }
}

khtml::MouseEvent::~MouseEvent()
{
  delete d;
}

long khtml::MouseEvent::offset() const
{
    int offset = 0;
    DOM::NodeImpl* tempNode = 0;
    int absX, absY;
    absX = absY = 0;
    if (innerNode().handle()->renderer()) {
        // FIXME: Shouldn't be necessary to skip text nodes.
        DOM::Node inner = innerNode();
        if (inner.nodeType() == Node::TEXT_NODE)
            inner = inner.parentNode();
        inner.handle()->renderer()->absolutePosition(absX, absY);
        inner.handle()->renderer()->checkSelectionPoint(m_x, m_y, absX, absY, tempNode, offset );
    }
    return offset;
}

const char *khtml::MousePressEvent::s_strMousePressEvent = "khtml/Events/MousePressEvent";

const char *khtml::MouseDoubleClickEvent::s_strMouseDoubleClickEvent = "khtml/Events/MouseDoubleClickEvent";

const char *khtml::MouseMoveEvent::s_strMouseMoveEvent = "khtml/Events/MouseMoveEvent";

const char *khtml::MouseReleaseEvent::s_strMouseReleaseEvent = "khtml/Events/MouseReleaseEvent";

const char *khtml::DrawContentsEvent::s_strDrawContentsEvent = "khtml/Events/DrawContentsEvent";

class khtml::DrawContentsEvent::DrawContentsEventPrivate
{
public:
  DrawContentsEventPrivate()
  {
  }
  ~DrawContentsEventPrivate()
  {
  }
};

khtml::DrawContentsEvent::DrawContentsEvent( QPainter *painter, int clipx, int clipy, int clipw, int cliph )
  : KParts::Event( s_strDrawContentsEvent ), m_painter( painter ), m_clipx( clipx ), m_clipy( clipy ),
    m_clipw( clipw ), m_cliph( cliph )
{
  d = new DrawContentsEventPrivate;
}

khtml::DrawContentsEvent::~DrawContentsEvent()
{
  delete d;
}

