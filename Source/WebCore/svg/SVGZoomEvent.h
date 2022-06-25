/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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
 */
 
#pragma once

#include "FloatRect.h"
#include "UIEvent.h"

namespace WebCore {

class SVGPoint;
class SVGRect;

class SVGZoomEvent final : public UIEvent {
    WTF_MAKE_ISO_ALLOCATED(SVGZoomEvent);
public:
    static Ref<SVGZoomEvent> createForBindings() { return adoptRef(*new SVGZoomEvent); }

    // 'SVGZoomEvent' functions
    Ref<SVGRect> zoomRectScreen() const;

    float previousScale() const;
    void setPreviousScale(float);

    Ref<SVGPoint> previousTranslate() const;

    float newScale() const;
    void setNewScale(float);

    Ref<SVGPoint> newTranslate() const;

    EventInterface eventInterface() const final;

private:
    SVGZoomEvent();

    float m_newScale;
    float m_previousScale;

    FloatRect m_zoomRectScreen;

    FloatPoint m_newTranslate;
    FloatPoint m_previousTranslate;
};

} // namespace WebCore
