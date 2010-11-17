/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

    class HTMLInputElement;
    class MouseEvent;
    class SliderThumbElement;
    
    class RenderSlider : public RenderBlock {
    public:
        RenderSlider(HTMLInputElement*);
        virtual ~RenderSlider();

        void forwardEvent(Event*);
        bool inDragMode() const;
        IntRect thumbRect();

    private:
        virtual const char* renderName() const { return "RenderSlider"; }
        virtual bool isSlider() const { return true; }

        virtual int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const;
        virtual void computePreferredLogicalWidths();
        virtual void layout();
        virtual void updateFromElement();

        bool mouseEventIsInThumb(MouseEvent*);
        FloatPoint mouseEventOffsetToThumb(MouseEvent*);

        void setValueForPosition(int position);
        void setPositionFromValue();
        int positionForOffset(const IntPoint&);

        int currentPosition();

        virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);

        virtual bool requiresForcedStyleRecalcPropagation() const { return true; }

        PassRefPtr<RenderStyle> createThumbStyle(const RenderStyle* parentStyle);

        int trackSize();

        RefPtr<SliderThumbElement> m_thumb;

        friend class SliderThumbElement;
    };

    inline RenderSlider* toRenderSlider(RenderObject* object)
    {
        ASSERT(!object || object->isSlider());
        return static_cast<RenderSlider*>(object);
    }

    // This will catch anyone doing an unnecessary cast.
    void toRenderSlider(const RenderSlider*);

} // namespace WebCore

#endif // RenderSlider_h
