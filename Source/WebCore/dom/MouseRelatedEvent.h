/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2013 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/LayoutPoint.h>
#include <WebCore/UIEventWithKeyState.h>

namespace WebCore {

class LocalFrameView;

struct MouseEventInit;

// Internal only: Helper class for what's common between mouse and wheel events.
class MouseRelatedEvent : public UIEventWithKeyState {
    WTF_MAKE_TZONE_ALLOCATED(MouseRelatedEvent);
public:
    enum class IsSimulated : bool { No, Yes };

    // Note that these values are adjusted to counter the effects of zoom, so that values
    // exposed via DOM APIs are invariant under zooming.
    double screenX() const override;
    double screenY() const override;
    double clientX() const override;
    double clientY() const override;
    const DoublePoint& screenLocation() const { return m_screenLocation; }

    double movementX() const { return m_movementX; }
    double movementY() const { return m_movementY; }

    const DoublePoint& windowLocation() const { return m_windowLocation; }

    const DoublePoint& clientLocation() const { return m_clientLocation; }
    int layerX() override;
    int layerY() override;

    bool isSimulated() const { return m_isSimulated; }
    void setIsSimulated(bool value) { m_isSimulated = value; }
    double pageX() const override;
    double pageY() const override;
    WEBCORE_EXPORT DoublePoint locationInRootViewCoordinates() const;

    // Page point in "absolute" coordinates (i.e. post-zoomed, page-relative coords,
    // usable with RenderObject::absoluteToLocal).
    const DoublePoint& absoluteLocation() const { return m_absoluteLocation; }

    static LocalFrameView* frameViewFromWindowProxy(WindowProxy*);

    static DoublePoint pagePointToClientPoint(DoublePoint pagePoint, LocalFrameView*);
    static DoublePoint pagePointToAbsolutePoint(DoublePoint pagePoint, LocalFrameView*);

    WEBCORE_EXPORT virtual double offsetX();
    WEBCORE_EXPORT virtual double offsetY();

    void computeRelativePosition();

protected:
    MouseRelatedEvent(enum EventInterfaceType);
    MouseRelatedEvent();
    MouseRelatedEvent(enum EventInterfaceType, const AtomString& type, CanBubble, IsCancelable, IsComposed, MonotonicTime, RefPtr<WindowProxy>&&, int detail,
        const DoublePoint& screenLocation, const DoublePoint& windowLocation, double movementX, double movementY, OptionSet<Modifier> modifiers,
        IsSimulated = IsSimulated::No, IsTrusted = IsTrusted::Yes);
    MouseRelatedEvent(enum EventInterfaceType, const AtomString& type, IsCancelable, MonotonicTime, RefPtr<WindowProxy>&&, const DoublePoint& globalLocation, OptionSet<Modifier>);
    MouseRelatedEvent(enum EventInterfaceType, const AtomString& type, const MouseEventInit&, IsTrusted = IsTrusted::No);
    MouseRelatedEvent(enum EventInterfaceType, const AtomString& type, const EventModifierInit&, IsTrusted = IsTrusted::No);

    void initCoordinates();
    void initCoordinates(const DoublePoint& clientLocation);
    void receivedTarget() override;

    void computePageLocation();

    float documentToAbsoluteScaleFactor() const;

    bool hasCachedRelativePosition() const { return m_hasCachedRelativePosition; }

    DoublePoint offsetLocation() const { return m_offsetLocation; }
    DoublePoint pageLocation() const { return m_pageLocation; }

    void setScreenLocation(const DoublePoint& point) { m_screenLocation = point; }

private:
    void init(bool isSimulated, const DoublePoint&);

    double m_movementX { 0 };
    double m_movementY { 0 };
    DoublePoint m_pageLocation;
    LayoutPoint m_layerLocation;
    DoublePoint m_offsetLocation;
    DoublePoint m_absoluteLocation;
    DoublePoint m_windowLocation;

    DoublePoint m_screenLocation;
    DoublePoint m_clientLocation;

    bool m_isSimulated { false };
    bool m_hasCachedRelativePosition { false };
};

} // namespace WebCore
