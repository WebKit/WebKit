/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

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
#include <qevent.h>

#include "DOMStringImpl.h"
#include "MouseEventImpl.h"

using namespace KDOM;

MouseEventImpl::MouseEventImpl(EventImplType identifier) : UIEventImpl(identifier), m_relatedTarget(0)
{
    m_screenX = 0;
    m_screenY = 0;
    m_clientX = 0;
    m_clientY = 0;
    
    m_ctrlKey = false;
    m_altKey = false;
    m_shiftKey = false;
    m_metaKey = false;

    m_button = 0;
    
    m_qevent = 0;
}

MouseEventImpl::~MouseEventImpl()
{
}

long MouseEventImpl::screenX() const
{
    return m_screenX;
}

long MouseEventImpl::screenY() const
{
    return m_screenY;
}

long MouseEventImpl::clientX() const
{
    return m_clientX;
}

long MouseEventImpl::clientY() const
{
    return m_clientY;
}

bool MouseEventImpl::ctrlKey() const
{
    return m_ctrlKey;
}

bool MouseEventImpl::shiftKey() const
{
    return m_shiftKey;
}

bool MouseEventImpl::altKey() const
{
    return m_altKey;
}

bool MouseEventImpl::metaKey() const
{
    return m_metaKey;
}

unsigned short MouseEventImpl::button() const
{
    return m_button;
}

EventTargetImpl *MouseEventImpl::relatedTarget() const
{
    return m_relatedTarget;
}

void MouseEventImpl::setRelatedTarget(EventTargetImpl *target)
{
    m_relatedTarget = target;
}

void MouseEventImpl::initMouseEvent(DOMStringImpl *typeArg, bool canBubbleArg, bool cancelableArg, AbstractViewImpl *viewArg, long detailArg, long screenXArg, long screenYArg, long clientXArg, long clientYArg, bool ctrlKeyArg, bool altKeyArg, bool shiftKeyArg, bool metaKeyArg, unsigned short buttonArg, EventTargetImpl *relatedTargetArg)
{
    initUIEvent(typeArg, canBubbleArg, cancelableArg, viewArg, detailArg);

    m_screenX = screenXArg;
    m_screenY = screenYArg;
    m_clientX = clientXArg;
    m_clientY = clientYArg;
    m_ctrlKey = ctrlKeyArg;
    m_altKey = altKeyArg;
    m_shiftKey = shiftKeyArg;
    m_metaKey = metaKeyArg;
    m_button = buttonArg;
    m_relatedTarget = relatedTargetArg;
}

void MouseEventImpl::initMouseEvent(DOMStringImpl *typeArg, QMouseEvent *qevent, float scale)
{
    if(!qevent || !typeArg || typeArg->isEmpty())
        return;

    int clientX = qevent->x(), screenX = qevent->globalX();
    int clientY = qevent->y(), screenY = qevent->globalY();

    if(scale != 1.0)
    {
        clientX = int(clientX / scale);
        clientY = int(clientY / scale);
    }

    int button = 0;
    if(qevent->stateAfter() & Qt::LeftButton)
        button = 1;
    else if(qevent->stateAfter() & Qt::MidButton)
        button = 2;
    else if(qevent->stateAfter() & Qt::RightButton)
        button = 3;

    initMouseEvent(typeArg, true, true, 0, 0, screenX, screenY, clientX, clientY,
                   (qevent->state() & Qt::ControlButton), (qevent->state() & Qt::AltButton),
                   (qevent->state() & Qt::ShiftButton), (qevent->state() & Qt::MetaButton),
                   button, 0 /* the target will be set by the prepareMouseEvent() function */);

    m_qevent = 0;
}

// vim:ts=4:noet
