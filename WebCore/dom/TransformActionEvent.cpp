/*
    Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(TRANSFORMACTION_EVENTS)

#include "TransformActionEvent.h"

namespace WebCore {

TransformActionEvent::TransformActionEvent(const AtomicString& type,
    bool canBubble, bool cancelable, PassRefPtr<AbstractView> view, int screenX,
    int screenY, int pageX, int pageY, bool ctrlKey, bool altKey,
    bool shiftKey, bool metaKey, int translateX, int translateY,
    int translateSpeedX, int translateSpeedY, float scale, float scaleSpeed,
    float rotate, float rotateSpeed)
    : MouseRelatedEvent(type, canBubble, cancelable, view, 0, screenX, screenY,
                        pageX, pageY, ctrlKey, altKey, shiftKey, metaKey)
    , m_translateX(translateX)
    , m_translateY(translateY)
    , m_translateSpeedX(translateSpeedX)
    , m_translateSpeedY(translateSpeedY)
    , m_scale(scale)
    , m_scaleSpeed(scaleSpeed)
    , m_rotate(rotate)
    , m_rotateSpeed(rotateSpeed)
{
}

void TransformActionEvent::initTransformActionEvent(const AtomicString& type,
    bool canBubble, bool cancelable, PassRefPtr<AbstractView> view, int screenX,
    int screenY, int clientX, int clientY, bool ctrlKey, bool altKey,
    bool shiftKey, bool metaKey, int translateX, int translateY,
    int translateSpeedX, int translateSpeedY, float scale, float scaleSpeed,
    float rotate, float rotateSpeed)
{
    if (dispatched())
        return;

    initUIEvent(type, canBubble, cancelable, view, 0);

    m_ctrlKey = ctrlKey;
    m_altKey = altKey;
    m_shiftKey = shiftKey;
    m_metaKey = metaKey;

    m_screenX = screenX;
    m_screenY = screenY;
    initCoordinates(clientX, clientY);

    m_translateX = translateX;
    m_translateY = translateY;
    m_translateSpeedX = translateSpeedX;
    m_translateSpeedY = translateSpeedY;
    m_scale = scale;
    m_scaleSpeed = scaleSpeed;
    m_rotate = rotate;
    m_rotateSpeed = rotateSpeed;
}

} // namespace WebCore

#endif // ENABLE(TRANSFORMACTION_EVENTS)
