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

#ifndef KSVG_SVGAnimationElementImpl_H
#define KSVG_SVGAnimationElementImpl_H
#if SVG_SUPPORT

#include "SVGTests.h"
#include "SVGStringList.h"
#include "SVGExternalResourcesRequired.h"

namespace WebCore
{
    enum EFillMode
    {
        FILL_REMOVE = 0,
        FILL_FREEZE
    };

    enum EAdditiveMode
    {
        ADDITIVE_REPLACE = 0,
        ADDITIVE_SUM
    };

    enum EAccumulateMode
    {
        ACCUMULATE_NONE = 0,
        ACCUMULATE_SUM
    };

    enum ECalcMode
    {
        CALCMODE_DISCRETE = 0,
        CALCMODE_LINEAR,
        CALCMODE_PACED,
        CALCMODE_SPLINE
    };

    enum ERestart
    {
        RESTART_ALWAYS = 0,
        RESTART_WHENNOTACTIVE,
        RESTART_NEVER
    };

    enum EAttributeType
    {
        ATTRIBUTETYPE_CSS = 0,
        ATTRIBUTETYPE_XML,
        ATTRIBUTETYPE_AUTO
    };

    enum EAnimationMode // internal
    {
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
        SVGAnimationElement(const QualifiedName& tagName, Document *doc);
        virtual ~SVGAnimationElement();

        // 'SVGAnimationElement' functions
        SVGElement *targetElement() const;
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        double getEndTime() const;
        double getStartTime() const;
        double getCurrentTime() const;
        double getSimpleDuration() const;
    
        virtual void parseMappedAttribute(MappedAttribute *attr);
        virtual bool rendererIsNeeded(RenderStyle *) { return false; }

        virtual void closeRenderer();

        // Helpers
        virtual void handleTimerEvent(double timePercentage) = 0;

        double parseClockValue(const DeprecatedString &data) const;

        String targetAttribute() const;
        void setTargetAttribute(StringImpl *value);

        static void setTargetAttribute(SVGElement *target,
                                       StringImpl *name,
                                       StringImpl *value,
                                       EAttributeType type = ATTRIBUTETYPE_AUTO);

        DeprecatedString attributeName() const;

        bool connected() const;

        bool isFrozen() const;
        bool isAdditive() const;
        bool isAccumulated() const;

        EAnimationMode detectAnimationMode() const;

        int calculateCurrentValueItem(double timePercentage);
        double calculateRelativeTimePercentage(double timePercentage, int currentItem);

        double repeations() const;
        bool isIndefinite(double value) const;

    protected:
        mutable SVGElement *m_targetElement;

        bool m_connected : 1;
        
        double m_currentTime;
        double m_simpleDuration;

        // Shared animation properties
        unsigned m_fill : 1; // EFillMode m_fill
        unsigned m_restart : 2; // ERestart
        unsigned m_calcMode : 2; // ECalcMode
        unsigned m_additive : 1; // EAdditiveMode
        unsigned m_accumulate : 1; // EAccumulateMode
        unsigned m_attributeType : 2; // EAttributeType
        
        DeprecatedString m_to;
        DeprecatedString m_by;
        DeprecatedString m_from;
        DeprecatedString m_href;
        DeprecatedString m_repeatDur;
        DeprecatedString m_attributeName;

        double m_max;
        double m_min;
        double m_end;
        double m_begin;

        double m_repeations;
        double m_repeatCount;

        RefPtr<SVGStringList> m_values;
        RefPtr<SVGStringList> m_keyTimes;
        RefPtr<SVGStringList> m_keySplines;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
