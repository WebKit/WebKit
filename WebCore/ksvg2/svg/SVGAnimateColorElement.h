/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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

#ifndef KSVG_SVGAnimateColorElementImpl_H
#define KSVG_SVGAnimateColorElementImpl_H
#ifdef SVG_SUPPORT

#include "SVGAnimationElement.h"
#include <wtf/RefPtr.h>

namespace WebCore {

    class SVGColor;

    class SVGAnimateColorElement : public SVGAnimationElement {
    public:
        SVGAnimateColorElement(const QualifiedName&, Document*);
        virtual ~SVGAnimateColorElement();

        void applyAnimationToValue(Color& currentColor);
        virtual void handleTimerEvent(double timePercentage);

        // Helper
        Color addColorsAndClamp(const Color&, const Color&);
        Color clampColor(int r, int g, int b) const; // deprecated
        void calculateColor(double time, int &r, int &g, int &b) const;

        Color color() const;
        Color initialColor() const;

    protected:
        virtual const SVGElement* contextElement() const { return this; }
        void storeInitialValue();
        void resetValues();
        bool updateCurrentValue(double timePercentage);
        bool startIfNecessary();
        void handleEndCondition();

    private:
        Color m_lastColor;
        Color m_currentColor;
        Color m_initialColor;

        RefPtr<SVGColor> m_toColor;
        RefPtr<SVGColor> m_fromColor;

        int m_currentItem;
        int m_redDiff;
        int m_greenDiff;
        int m_blueDiff;
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif // KSVG_SVGAnimateColorElementImpl_H

// vim:ts=4:noet
