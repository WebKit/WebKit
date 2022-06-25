/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2013 Apple Inc.
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

#include "LayoutPoint.h"
#include "UIEventWithKeyState.h"

namespace WebCore {

class FrameView;

struct MouseRelatedEventInit : public EventModifierInit {
    int screenX { 0 };
    int screenY { 0 };
};

// Internal only: Helper class for what's common between mouse and wheel events.
class MouseRelatedEvent : public UIEventWithKeyState {
    WTF_MAKE_ISO_ALLOCATED(MouseRelatedEvent);
public:
    enum class IsSimulated : uint8_t { Yes, No };

    // Note that these values are adjusted to counter the effects of zoom, so that values
    // exposed via DOM APIs are invariant under zooming.
    int screenX() const { return m_screenLocation.x(); }
    int screenY() const { return m_screenLocation.y(); }
    const IntPoint& screenLocation() const { return m_screenLocation; }
    int clientX() const { return m_clientLocation.x(); }
    int clientY() const { return m_clientLocation.y(); }
    int movementX() const { return m_movementDelta.x(); }
    int movementY() const { return m_movementDelta.y(); }

    const LayoutPoint& clientLocation() const { return m_clientLocation; }
    int layerX() override;
    int layerY() override;
    WEBCORE_EXPORT int offsetX();
    WEBCORE_EXPORT int offsetY();
    bool isSimulated() const { return m_isSimulated; }
    void setIsSimulated(bool value) { m_isSimulated = value; }
    int pageX() const final;
    int pageY() const final;
    WEBCORE_EXPORT FloatPoint locationInRootViewCoordinates() const;
    virtual const LayoutPoint& pageLocation() const;
    WEBCORE_EXPORT int x() const;
    WEBCORE_EXPORT int y() const;

    // Page point in "absolute" coordinates (i.e. post-zoomed, page-relative coords,
    // usable with RenderObject::absoluteToLocal).
    const LayoutPoint& absoluteLocation() const { return m_absoluteLocation; }
    
    static FrameView* frameViewFromWindowProxy(WindowProxy*);

    static LayoutPoint pagePointToClientPoint(LayoutPoint pagePoint, FrameView*);
    static LayoutPoint pagePointToAbsolutePoint(LayoutPoint pagePoint, FrameView*);

protected:
    MouseRelatedEvent() = default;
    MouseRelatedEvent(const AtomString& type, CanBubble, IsCancelable, IsComposed, MonotonicTime, RefPtr<WindowProxy>&&, int detail,
        const IntPoint& screenLocation, const IntPoint& windowLocation, const IntPoint& movementDelta, OptionSet<Modifier> modifiers,
        IsSimulated = IsSimulated::No, IsTrusted = IsTrusted::Yes);
    MouseRelatedEvent(const AtomString& type, IsCancelable, MonotonicTime, RefPtr<WindowProxy>&&, const IntPoint& globalLocation, OptionSet<Modifier>);
    MouseRelatedEvent(const AtomString& type, const MouseRelatedEventInit&, IsTrusted = IsTrusted::No);

    void initCoordinates();
    void initCoordinates(const LayoutPoint& clientLocation);
    void receivedTarget() final;

    void computePageLocation();
    void computeRelativePosition();

    float documentToAbsoluteScaleFactor() const;

    // Expose these so MouseEvent::initMouseEvent can set them.
    IntPoint m_screenLocation;
    LayoutPoint m_clientLocation;
    LayoutPoint m_movementDelta;

private:
    void init(bool isSimulated, const IntPoint&);

    LayoutPoint m_pageLocation;
    LayoutPoint m_layerLocation;
    LayoutPoint m_offsetLocation;
    LayoutPoint m_absoluteLocation;
    bool m_isSimulated { false };
    bool m_hasCachedRelativePosition { false };
};

} // namespace WebCore
