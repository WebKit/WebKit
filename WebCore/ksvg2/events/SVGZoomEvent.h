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

#ifndef KDOM_SVGZoomEventImpl_H
#define KDOM_SVGZoomEventImpl_H
#if SVG_SUPPORT

#include "dom2_eventsimpl.h"

namespace WebCore
{
    class SVGRect;
    class SVGPoint;
    class SVGZoomEvent : public UIEvent
    {
    public:
        SVGZoomEvent();
        virtual ~SVGZoomEvent();

        // 'SVGZoomEvent' functions
        SVGRect *zoomRectScreen() const;

        float previousScale() const;
        void setPreviousScale(float scale);

        SVGPoint *previousTranslate() const;

        float newScale() const;
        void setNewScale(float scale);

        SVGPoint *newTranslate() const;

    private:
        float m_newScale;
        float m_previousScale;
        
        RefPtr<SVGRect> m_zoomRectScreen;
        
        RefPtr<SVGPoint> m_newTranslate;
        RefPtr<SVGPoint> m_previousTranslate;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
