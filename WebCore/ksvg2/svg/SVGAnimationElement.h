/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>
    Copyright (C) 2007 Eric Seidel <eric@webkit.org>

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

#ifndef SVGAnimationElement_h
#define SVGAnimationElement_h
#ifdef SVG_SUPPORT

#include "SVGExternalResourcesRequired.h"
#include "SVGStringList.h"
#include "SVGTests.h"

namespace WebCore {

    enum EFillMode {
        FILL_REMOVE = 0,
        FILL_FREEZE
    };

    enum EAdditiveMode {
        ADDITIVE_REPLACE = 0,
        ADDITIVE_SUM
    };

    enum EAccumulateMode {
        ACCUMULATE_NONE = 0,
        ACCUMULATE_SUM
    };

    enum ECalcMode {
        CALCMODE_DISCRETE = 0,
        CALCMODE_LINEAR,
        CALCMODE_PACED,
        CALCMODE_SPLINE
    };

    enum ERestart {
        RESTART_ALWAYS = 0,
        RESTART_WHENNOTACTIVE,
        RESTART_NEVER
    };

    enum EAttributeType {
        ATTRIBUTETYPE_CSS = 0,
        ATTRIBUTETYPE_XML,
        ATTRIBUTETYPE_AUTO
    };

    // internal
    enum EAnimationMode {
        NO_ANIMATION = 0,
        TO_ANIMATION,
        BY_ANIMATION,
        VALUES_ANIMATION,
        FROM_TO_ANIMATION,
        FROM_BY_ANIMATION
    };

    class SVGAnimationElement : public SVGElement,
                                public SVGTests,
                                public SVGExternalResourcesRequired
    {
    public:
        SVGAnimationElement(const QualifiedName&, Document*);
        virtual ~SVGAnimationElement();

        // 'SVGAnimationElement' functions
        SVGElement* targetElement() const;
        
        virtual bool hasValidTarget() const;
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        double getEndTime() const;
        double getStartTime() const;
        double getCurrentTime() const;
        double getSimpleDuration(ExceptionCode&) const;
    
        virtual void parseMappedAttribute(MappedAttribute* attr);
        virtual bool rendererIsNeeded(RenderStyle*) { return false; }

        virtual void closeRenderer();

        // Helpers
        bool updateForElapsedSeconds(double);
        void handleTimerEvent(double timePercentage);

        double parseClockValue(const String&) const;

        String targetAttribute() const;
        void setTargetAttribute(const String&);

        static void setTargetAttribute(SVGElement* target, const String& name, const String& value, EAttributeType = ATTRIBUTETYPE_AUTO);

        String attributeName() const;

        bool connectedToTimer() const;

        bool isFrozen() const;
        bool isAdditive() const;
        bool isAccumulated() const;

        EAnimationMode detectAnimationMode() const;

        int calculateCurrentValueItem(double timePercentage);
        double calculateRelativeTimePercentage(double timePercentage, int currentItem);

        double repeations() const;
        bool isIndefinite(double value) const;

    protected:
        mutable SVGElement* m_targetElement;
        
        void connectTimer();
        void disconnectTimer();
        
        virtual bool updateCurrentValue(double timePercentage) = 0;
        virtual bool handleStartCondition() = 0;
        virtual void updateLastValueWithCurrent() { } // See bug 12075 for explaination of why this is a bad API
        virtual void resetValues() { }
        
        static void parseKeyNumbers(Vector<float>& keyNumbers, const String& value);

        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGExternalResourcesRequired, bool, ExternalResourcesRequired, externalResourcesRequired)

        bool m_connectedToTimer : 1;
        
        double m_currentTime;
        double m_simpleDuration;

        // Shared animation properties
        unsigned m_fill : 1; // EFillMode m_fill
        unsigned m_restart : 2; // ERestart
        unsigned m_calcMode : 2; // ECalcMode
        unsigned m_additive : 1; // EAdditiveMode
        unsigned m_accumulate : 1; // EAccumulateMode
        unsigned m_attributeType : 2; // EAttributeType
        
        String m_to;
        String m_by;
        String m_from;
        String m_href;
        String m_repeatDur;
        String m_attributeName;

        double m_max;
        double m_min;
        double m_end;
        double m_begin;

        double m_repetitions;
        double m_repeatCount;

        RefPtr<SVGStringList> m_values;
        Vector<float> m_keyTimes;
        
        struct KeySpline {
            FloatPoint control1;
            FloatPoint control2;
        };
        Vector<KeySpline> m_keySplines;
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif // SVGAnimationElement_h

// vim:ts=4:noet
