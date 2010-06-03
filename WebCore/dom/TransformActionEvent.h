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

#ifndef TransformActionEvent_h
#define TransformActionEvent_h

#if ENABLE(TRANSFORMACTION_EVENTS)

#include "MouseRelatedEvent.h"

namespace WebCore {

class TransformActionEvent : public MouseRelatedEvent {
public:
    static PassRefPtr<TransformActionEvent> create()
    {
        return adoptRef(new TransformActionEvent);
    }
    static PassRefPtr<TransformActionEvent> create(const AtomicString& type,
        bool canBubble, bool cancelable, PassRefPtr<AbstractView> view,
        int screenX, int screenY, int pageX, int pageY, bool ctrlKey,
        bool altKey, bool shiftKey, bool metaKey, int translateX,
        int translateY, int translateSpeedX, int translateSpeedY, float scale,
        float scaleSpeed, float rotate, float rotateSpeed)
    {
        return adoptRef(new TransformActionEvent(type, canBubble, cancelable,
            view, screenX, screenY, pageX, pageY, ctrlKey, altKey, shiftKey,
            metaKey, translateX, translateY, translateSpeedX, translateSpeedY,
            scale, scaleSpeed, rotate, rotateSpeed));
    }

    void initTransformActionEvent(const AtomicString& type, bool canBubble,
        bool cancelable, PassRefPtr<AbstractView> view, int screenX,
        int screenY, int clientX, int clientY, bool ctrlKey, bool altKey,
        bool shiftKey, bool metaKey, int translateX, int translateY,
        int translateSpeedX, int translateSpeedY, float scale, float scaleSpeed,
        float rotate, float rotateSpeed);

    int translateX() const { return m_translateX; }
    int translateY() const { return m_translateY; }
    int translateSpeedX() const { return m_translateSpeedX; }
    int translateSpeedY() const { return m_translateSpeedY; }
    float scale() const { return m_scale; }
    float scaleSpeed() const { return m_scaleSpeed; }
    float rotate() const { return m_rotate; }
    float rotateSpeed() const { return m_rotateSpeed; }

private:
    TransformActionEvent() {}
    TransformActionEvent(const AtomicString& type, bool canBubble,
        bool cancelable, PassRefPtr<AbstractView> view, int screenX,
        int screenY, int pageX, int pageY, bool ctrlKey, bool altKey,
        bool shiftKey, bool metaKey, int translateX, int translateY,
        int translateSpeedX, int translateSpeedY, float scale, float scaleSpeed,
        float rotate, float rotateSpeed);

    virtual bool isTransformActionEvent() const { return true; }

    int m_translateX;
    int m_translateY;
    int m_translateSpeedX;
    int m_translateSpeedY;
    float m_scale;
    float m_scaleSpeed;
    float m_rotate;
    float m_rotateSpeed;
};

} // namespace WebCore

#endif // ENABLE(TRANSFORMACTION_EVENTS)

#endif // TransformActionEvent_h
