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

#include <kparts/event.h>

#include "dom/dom_node.h"
#include "dom/dom_string.h"

namespace khtml
{

class MouseEvent : public KParts::Event
{
public:
  MouseEvent( const char *name, QMouseEvent *qmouseEvent, int x, int y, const DOM::DOMString &url,
		   const DOM::Node &innerNode, long /*offset UNUSED REMOVE 3.0*/ = 0);
  virtual ~MouseEvent();

  QMouseEvent *qmouseEvent() const { return m_qmouseEvent; }
  int x() const { return m_x; }
  int y() const { return m_y; }
  DOM::DOMString url() const { return m_url; }
  DOM::Node innerNode() const { return m_innerNode; }

  // Temporary, for text selection only. To be removed when findSelectionNode determines
  // this by itself.
  int nodeAbsX() const;
  int nodeAbsY() const;
  void setNodePos( int x, int y);

  // obsolete. don't use it. its a no-op. 
  bool isURLHandlingEnabled() const; // ### remove KDE 3.0
  // obsolete. don't use it. its a no-op.
  void setURLHandlingEnabled( bool enable ); // ### remove KDE 3.0

  // return the offset of innerNode
  long offset() const;

private:
  QMouseEvent *m_qmouseEvent;
  int m_x;
  int m_y;
  DOM::DOMString m_url;
  DOM::Node m_innerNode;
  long m_offset; // ##remove
  class MouseEventPrivate;
  MouseEventPrivate *d;
};

class MousePressEvent : public MouseEvent
{
public:
  MousePressEvent( QMouseEvent *mouseEvent, int x, int y, const DOM::DOMString &url,
		   const DOM::Node &innerNode, long = 0 )
  : MouseEvent( s_strMousePressEvent, mouseEvent, x, y, url, innerNode )
  {}

  static bool test( const QEvent *event ) { return KParts::Event::test( event, s_strMousePressEvent ); }


private:
  static const char *s_strMousePressEvent;
};

class MouseDoubleClickEvent : public MouseEvent
{
public:
  MouseDoubleClickEvent( QMouseEvent *mouseEvent, int x, int y, const DOM::DOMString &url,
		         const DOM::Node &innerNode, long = 0 )
  : MouseEvent( s_strMouseDoubleClickEvent, mouseEvent, x, y, url, innerNode )
  {}

  static bool test( const QEvent *event )
  { return KParts::Event::test( event, s_strMouseDoubleClickEvent ); }

private:
  static const char *s_strMouseDoubleClickEvent;
};

class MouseMoveEvent : public MouseEvent
{
public:
  MouseMoveEvent( QMouseEvent *mouseEvent, int x, int y, const DOM::DOMString &url,
		   const DOM::Node &innerNode, long = 0 )
  : MouseEvent( s_strMouseMoveEvent, mouseEvent, x, y, url, innerNode )
  {}

  static bool test( const QEvent *event ) { return KParts::Event::test( event, s_strMouseMoveEvent ); }

private:
  static const char *s_strMouseMoveEvent;
};

class MouseReleaseEvent : public MouseEvent
{
public:
  MouseReleaseEvent( QMouseEvent *mouseEvent, int x, int y, const DOM::DOMString &url,
		     const DOM::Node &innerNode, long = 0 )
  : MouseEvent( s_strMouseReleaseEvent, mouseEvent, x, y, url, innerNode )
  {}

  static bool test( const QEvent *event ) { return KParts::Event::test( event, s_strMouseReleaseEvent ); }

private:
  static const char *s_strMouseReleaseEvent;
};

class DrawContentsEvent : public KParts::Event
{
public:
  DrawContentsEvent( QPainter *painter, int clipx, int clipy, int clipw, int cliph );
  virtual ~DrawContentsEvent();

  QPainter *painter() const { return m_painter; }
  int clipx() const { return m_clipx; }
  int clipy() const { return m_clipy; }
  int clipw() const { return m_clipw; }
  int cliph() const { return m_cliph; }

  static bool test( const QEvent *event ) { return KParts::Event::test( event, s_strDrawContentsEvent ); }

private:
  QPainter *m_painter;
  int m_clipx;
  int m_clipy;
  int m_clipw;
  int m_cliph;
  class DrawContentsEventPrivate;
  DrawContentsEventPrivate *d;
  static const char *s_strDrawContentsEvent;
};

};

#endif
