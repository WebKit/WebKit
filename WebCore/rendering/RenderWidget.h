/*
 * This file is part of the HTML widget for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef RenderWidget_h
#define RenderWidget_h

#include "OverlapTestRequestClient.h"
#include "RenderReplaced.h"

namespace WebCore {

class Widget;

class RenderWidget : public RenderReplaced, private OverlapTestRequestClient {
public:
    RenderWidget(Node*);
    virtual ~RenderWidget();

    virtual bool isWidget() const { return true; }

    virtual void paint(PaintInfo&, int tx, int ty);

    virtual void destroy();
    virtual void layout();

    Widget* widget() const { return m_widget; }
    static RenderWidget* find(const Widget*);

    RenderArena* ref() { ++m_refCount; return renderArena(); }
    void deref(RenderArena*);

    virtual void setSelectionState(SelectionState);

    void updateWidgetPosition();

    virtual void setWidget(Widget*);

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);

protected:
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

private:
    void setWidgetGeometry(const IntRect&);

    virtual void deleteWidget();

    // OverlapTestRequestClient
    virtual void setOverlapTestResult(bool);

protected:
    Widget* m_widget;
    FrameView* m_view;

private:
    int m_refCount;
};

} // namespace WebCore

#endif // RenderWidget_h
