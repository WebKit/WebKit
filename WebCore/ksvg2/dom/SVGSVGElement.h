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

#ifndef KSVG_SVGSVGElement_H
#define KSVG_SVGSVGElement_H

#include <SVGTests.h>
#include <SVGElement.h>
#include <SVGLangSpace.h>
#include <SVGStylable.h>
#include <SVGExternalResourcesRequired.h>
#include <SVGLocatable.h>
#include <SVGFitToViewBox.h>
#include <SVGZoomAndPan.h>

#include <kdom/events/DocumentEvent.h>

namespace KSVG
{
	class SVGRect;
	class SVGAngle;
	class SVGPoint;
	class SVGMatrix;
	class SVGNumber;
	class SVGLength;
	class SVGTransform;
	class SVGSVGElementImpl;
	class SVGAnimatedLength;
	class SVGSVGElement : public SVGElement,
						  public SVGTests,
						  public SVGLangSpace,
						  public SVGExternalResourcesRequired,
						  public SVGStylable,
						  public SVGLocatable,
						  public SVGFitToViewBox,
						  public SVGZoomAndPan,
						  public KDOM::DocumentEvent
	{
	public:
		SVGSVGElement();
		explicit SVGSVGElement(SVGSVGElementImpl *i);
		SVGSVGElement(const SVGSVGElement &other);
		SVGSVGElement(const KDOM::Node &other);
		virtual ~SVGSVGElement();

		// Operators
		SVGSVGElement &operator=(const SVGSVGElement &other);
		SVGSVGElement &operator=(const KDOM::Node &other);

		// 'SVGSVGElement' functions
		SVGAnimatedLength x() const;
		SVGAnimatedLength y() const;
		SVGAnimatedLength width() const;
		SVGAnimatedLength height() const;
		
		KDOM::DOMString contentScriptType() const;
		void setContentScriptType(const KDOM::DOMString &type);
		
		KDOM::DOMString contentStyleType() const;
		void setContentStyleType(const KDOM::DOMString &type);
		
		SVGRect viewport() const;

		float pixelUnitToMillimeterX() const;
		float pixelUnitToMillimeterY() const;
		float screenPixelToMillimeterX() const;
		float screenPixelToMillimeterY() const;
		
		bool useCurrentView() const;
		void setUseCurrentView(bool currentView);
		
		// SVGViewSpec currentView() const;
		
		float currentScale() const;
		void setCurrentScale(float scale);
		
		SVGPoint currentTranslate() const;

		unsigned long suspendRedraw(unsigned long max_wait_milliseconds);
		void unsuspendRedraw(unsigned long suspend_handle_id);
		void unsuspendRedrawAll();
		void forceRedraw();

		void pauseAnimations();
		void unpauseAnimations();
		bool animationsPaused();

		float getCurrentTime();
		void setCurrentTime(float seconds);

		KDOM::NodeList getIntersectionList(const SVGRect &rect, const SVGElement &referenceElement);
		KDOM::NodeList getEnclosureList(const SVGRect &rect, const SVGElement &referenceElement);
		bool checkIntersection(const SVGElement &element, const SVGRect &rect);
		bool checkEnclosure(const SVGElement &element, const SVGRect &rect);
		void deselectAll();

		SVGNumber createSVGNumber() const;
		SVGLength createSVGLength() const;
		SVGAngle createSVGAngle() const;
		SVGPoint createSVGPoint() const;
		SVGMatrix createSVGMatrix() const;
		SVGRect createSVGRect() const;
		SVGTransform createSVGTransform() const;
		SVGTransform createSVGTransformFromMatrix(const SVGMatrix &) const;

		// Internal
		KSVG_INTERNAL(SVGSVGElement)

	public: // EcmaScript section
		KDOM_GET
		KDOM_PUT

		KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
		void putValueProperty(KJS::ExecState *exec, int token, KJS::ValueImp *value, int attr);
	};
};

KSVG_DEFINE_PROTOTYPE(SVGSVGElementProto)
KSVG_IMPLEMENT_PROTOFUNC(SVGSVGElementProtoFunc, SVGSVGElement)

#endif

// vim:ts=4:noet
