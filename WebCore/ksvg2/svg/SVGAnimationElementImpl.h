/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

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

#include "SVGTestsImpl.h"
#include "SVGElementImpl.h"
#include "SVGStringListImpl.h"
#include "SVGExternalResourcesRequiredImpl.h"

namespace KSVG
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

	class SVGAnimationElementImpl : public SVGElementImpl,
									public SVGTestsImpl,
									public SVGExternalResourcesRequiredImpl
	{
	public:
		SVGAnimationElementImpl(KDOM::DocumentImpl *doc, KDOM::NodeImpl::Id id, const KDOM::DOMString &prefix);
		virtual ~SVGAnimationElementImpl();

		// 'SVGAnimationElement' functions
		SVGElementImpl *targetElement() const;

		double endTime() const;
		double startTime() const;
		double currentTime() const;
		double simpleDuration() const;
	
		virtual void parseAttribute(KDOM::AttributeImpl *attr);
		virtual bool implementsCanvasItem() const { return false; }

		virtual void close();

		// Helpers
		virtual void handleTimerEvent(double timePercentage) = 0;

		double parseClockValue(const QString &data) const;

		KDOM::DOMString targetAttribute() const;
		void setTargetAttribute(const KDOM::DOMString &value);

		static void setTargetAttribute(SVGElementImpl *target,
									   const KDOM::DOMString &name,
									   const KDOM::DOMString &value,
									   EAttributeType type = ATTRIBUTETYPE_AUTO);

		QString attributeName() const;

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
		mutable SVGElementImpl *m_targetElement;

		bool m_connected : 1;
		
		double m_currentTime;
		double m_simpleDuration;

		// Shared animation properties
		EFillMode m_fill : 1;
		ERestart m_restart : 2;
		ECalcMode m_calcMode : 2;
		EAdditiveMode m_additive : 1;
		EAccumulateMode m_accumulate : 1;
		EAttributeType m_attributeType : 2;
		
		QString m_to;
		QString m_by;
		QString m_from;
		QString m_href;
		QString m_repeatDur;
		QString m_attributeName;

		double m_max;
		double m_min;
		double m_end;
		double m_begin;

		double m_repeations;
		double m_repeatCount;

		SVGStringListImpl *m_values;
		SVGStringListImpl *m_keyTimes;
		SVGStringListImpl *m_keySplines;
	};
};

#endif

// vim:ts=4:noet
