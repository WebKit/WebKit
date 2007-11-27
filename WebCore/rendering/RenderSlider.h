/**
 *
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef RenderSlider_h
#define RenderSlider_h

#include "RenderBlock.h"

namespace WebCore {

    class HTMLDivElement;
    class HTMLInputElement;
    class HTMLSliderThumbElement;
    class MouseEvent;
    
    class RenderSlider : public RenderBlock {
    public:
        RenderSlider(HTMLInputElement*);
        ~RenderSlider();

        virtual const char* renderName() const { return "RenderSlider"; }
        virtual bool isSlider() const { return true; }

        virtual short baselinePosition( bool, bool ) const;
        virtual void calcPrefWidths();
        virtual void setStyle(RenderStyle*);
        virtual void layout();
        virtual void updateFromElement();
        
        bool mouseEventIsInThumb(MouseEvent*);

        void setValueForPosition(int position);
        double setPositionFromValue(bool inLayout = false);
        int positionForOffset(const IntPoint&);

        void valueChanged();
        
        int currentPosition();
        void setCurrentPosition(int pos);        
        
        void forwardEvent(Event*);
        bool inDragMode() const;

    private:
        RenderStyle* createThumbStyle(RenderStyle* parentStyle, RenderStyle* oldStyle = 0);
        int trackSize();

        RefPtr<HTMLSliderThumbElement> m_thumb;
};

} // namespace WebCore

#endif // RenderSlider_h
