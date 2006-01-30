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
#ifndef __khtml_events_h__
#define __khtml_events_h__

#include "dom/dom_string.h"

class QMouseEvent;

namespace WebCore {

class NodeImpl;
class QPainter;

class MouseEvent {
public:
  MouseEvent(QMouseEvent*, int x, int y, const DOMString& url, const DOMString& target, NodeImpl* innerNode);

  QMouseEvent* qmouseEvent() const { return m_qmouseEvent; }
  int x() const { return m_x; }
  int y() const { return m_y; }
  int absX() const { return m_nodeAbsX; }
  int absY() const { return m_nodeAbsY; }

  DOMString url() const { return m_url; }
  DOMString target() const { return m_target; }
  NodeImpl* innerNode() const { return m_innerNode.get(); }

  // return the offset of innerNode
  int offset() const;

private:
  QMouseEvent* m_qmouseEvent;
  int m_x;
  int m_y;
  int m_nodeAbsX, m_nodeAbsY;
  DOMString m_url;
  DOMString m_target;
  RefPtr<NodeImpl> m_innerNode;
};

class MousePressEvent : public MouseEvent
{
public:
  MousePressEvent(QMouseEvent* e, int x, int y, const DOMString& url, const DOMString& target, NodeImpl* innerNode)
    : MouseEvent(e, x, y, url, target, innerNode) { }
};

class MouseDoubleClickEvent : public MouseEvent
{
public:
  MouseDoubleClickEvent(QMouseEvent* e, int x, int y, const DOMString& url, const DOMString& target, NodeImpl* innerNode)
    : MouseEvent(e, x, y, url, target, innerNode) { }
};


class MouseMoveEvent : public MouseEvent
{
public:
  MouseMoveEvent(QMouseEvent* e, int x, int y, const DOMString& url, const DOMString& target, NodeImpl* innerNode)
   : MouseEvent(e, x, y, url, target, innerNode) { }
};

class MouseReleaseEvent : public MouseEvent
{
public:
  MouseReleaseEvent( QMouseEvent *mouseEvent, int x, int y,
                     const DOMString &url, const DOMString& target,
                     NodeImpl *innerNode, int = 0 )
  : MouseEvent( mouseEvent, x, y, url, target, innerNode )
  { }
};

class DrawContentsEvent
{
public:
  DrawContentsEvent(QPainter*, int clipx, int clipy, int clipw, int cliph);

  QPainter *painter() const { return m_painter; }
  int clipx() const { return m_clipx; }
  int clipy() const { return m_clipy; }
  int clipw() const { return m_clipw; }
  int cliph() const { return m_cliph; }

private:
  QPainter *m_painter;
  int m_clipx;
  int m_clipy;
  int m_clipw;
  int m_cliph;
};

}

#endif
