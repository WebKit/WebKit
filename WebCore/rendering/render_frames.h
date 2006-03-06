/*
 * This file is part of the KDE project.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef render_frames_h__
#define render_frames_h__

#include "RenderContainer.h"
#include "render_replaced.h"
#include "html_baseimpl.h"

namespace WebCore {

class FrameView;
class HTMLElementImpl;
class HTMLFrameElementImpl;
class MouseEventImpl;

struct ChildFrame;

class RenderFrameSet : public RenderContainer
{
    friend class HTMLFrameSetElementImpl;
public:
    RenderFrameSet(HTMLFrameSetElementImpl*);
    virtual ~RenderFrameSet();

    virtual const char* renderName() const { return "RenderFrameSet"; }
    virtual bool isFrameSet() const { return true; }

    virtual void layout();

    void positionFrames();

    bool resizing() const { return m_resizing; }

    bool userResize(MouseEventImpl*);
    bool canResize(int x, int y);
    void setResizing(bool);

    virtual bool nodeAtPoint(NodeInfo&, int x, int y, int tx, int ty, HitTestAction);

    HTMLFrameSetElementImpl* element() const
        { return static_cast<HTMLFrameSetElementImpl*>(RenderContainer::element()); }

#ifndef NDEBUG
    virtual void dump(QTextStream *stream, QString ind = "") const;
#endif

private:
    int m_oldpos;
    int m_gridLen[2];
    int* m_gridDelta[2];
    int* m_gridLayout[2];

    bool* m_hSplitVar; // is this split variable?
    bool* m_vSplitVar;

    int m_hSplit; // the split currently resized
    int m_vSplit;
    int m_hSplitPos;
    int m_vSplitPos;

    bool m_resizing;
    bool m_clientResizing;
};

class RenderPart : public RenderWidget {
public:
    RenderPart(HTMLElementImpl*);
    virtual ~RenderPart();
    
    virtual const char* renderName() const { return "RenderPart"; }

    void setFrame(Frame*);
    void setWidget(Widget*);

    // FIXME: This should not be necessary.
    // Remove this once WebKit knows to properly schedule layouts using WebCore when objects resize.
    void updateWidgetPosition();

    bool hasFallbackContent() const { return m_hasFallbackContent; }

    virtual void viewCleared();

protected:
    bool m_hasFallbackContent;

private:
    virtual void deleteWidget();

    Frame* m_frame;
};

class RenderFrame : public RenderPart
{
public:
    RenderFrame(HTMLFrameElementImpl*);

    virtual const char* renderName() const { return "RenderFrame"; }

    HTMLFrameElementImpl* element() const
        { return static_cast<HTMLFrameElementImpl*>(RenderPart::element()); }

    virtual void viewCleared();
};

// I can hardly call the class RenderObject ;-)
class RenderPartObject : public RenderPart
{
public:
    RenderPartObject(HTMLElementImpl*);

    virtual const char* renderName() const { return "RenderPartObject"; }

    virtual void layout();
    virtual void updateWidget();

    virtual void viewCleared();
};

}

#endif
