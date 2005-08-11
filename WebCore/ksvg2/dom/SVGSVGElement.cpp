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

#include <kdom/ecma/Ecma.h>
#include <kdom/events/Event.h>
#include <kdom/NodeList.h>

#include "SVGRect.h"
#include "SVGAngle.h"
#include "SVGPoint.h"
#include "SVGEvent.h"
#include "SVGMatrix.h"
#include "SVGNumber.h"
#include "SVGLength.h"
#include "SVGDocument.h"
#include "SVGTransform.h"
#include "SVGEventImpl.h"
#include "SVGException.h"
#include "SVGSVGElement.h"
#include "SVGExceptionImpl.h"
#include "SVGSVGElementImpl.h"
#include "SVGAnimatedLength.h"

#include "SVGConstants.h"
#include "SVGSVGElement.lut.h"
using namespace KSVG;

/*
@begin SVGSVGElement::s_hashTable 17
 x								SVGSVGElementConstants::X								DontDelete|ReadOnly
 y								SVGSVGElementConstants::Y								DontDelete|ReadOnly
 width							SVGSVGElementConstants::Width							DontDelete|ReadOnly
 height							SVGSVGElementConstants::Height							DontDelete|ReadOnly
 contentScriptType				SVGSVGElementConstants::ContentScriptType				DontDelete
 contentStyleType				SVGSVGElementConstants::ContentStyleType				DontDelete
 viewport						SVGSVGElementConstants::Viewport						DontDelete|ReadOnly
 pixelUnitToMillimeterX			SVGSVGElementConstants::PixelUnitToMillimeterX			DontDelete|ReadOnly
 pixelUnitToMillimeterY			SVGSVGElementConstants::PixelUnitToMillimeterY			DontDelete|ReadOnly
 screenPixelToMillimeterX		SVGSVGElementConstants::ScreenPixelToMillimeterX		DontDelete|ReadOnly
 screenPixelToMillimeterY		SVGSVGElementConstants::ScreenPixelToMillimeterY		DontDelete|ReadOnly
 useCurrentView					SVGSVGElementConstants::UseCurrentView					DontDelete|ReadOnly
 currentView					SVGSVGElementConstants::CurrentView						DontDelete|ReadOnly
 currentScale					SVGSVGElementConstants::CurrentScale					DontDelete
 currentTranslate				SVGSVGElementConstants::CurrentTranslate				DontDelete|ReadOnly
@end
@begin SVGSVGElementProto::s_hashTable 27
 suspendRedraw					SVGSVGElementConstants::SuspendRedraw					DontDelete|Function 1
 unsuspendRedraw				SVGSVGElementConstants::UnsuspendRedraw					DontDelete|Function 1
 unsuspendRedrawAll				SVGSVGElementConstants::UnsuspendRedrawAll				DontDelete|Function 0
 forceRedraw					SVGSVGElementConstants::ForceRedraw						DontDelete|Function 0
 pauseAnimations				SVGSVGElementConstants::PauseAnimations					DontDelete|Function 0
 unpauseAnimations				SVGSVGElementConstants::UnpauseAnimations				DontDelete|Function 0
 animationsPaused				SVGSVGElementConstants::AnimationsPaused				DontDelete|Function 0
 getCurrentTime					SVGSVGElementConstants::GetCurrentTime					DontDelete|Function 0
 setCurrentTime					SVGSVGElementConstants::SetCurrentTime					DontDelete|Function 1
 getIntersectionList			SVGSVGElementConstants::GetIntersectionList				DontDelete|Function 2
 getEnclosureList				SVGSVGElementConstants::GetEnclosureList				DontDelete|Function 2
 checkIntersection				SVGSVGElementConstants::CheckIntersection				DontDelete|Function 2
 checkEnclosure					SVGSVGElementConstants::CheckEnclosure					DontDelete|Function 2
 deselectAll					SVGSVGElementConstants::DeselectAll						DontDelete|Function 0
 createSVGNumber				SVGSVGElementConstants::CreateSVGNumber					DontDelete|Function 0
 createSVGLength				SVGSVGElementConstants::CreateSVGLength					DontDelete|Function 0
 createSVGAngle					SVGSVGElementConstants::CreateSVGAngle					DontDelete|Function 0
 createSVGPoint					SVGSVGElementConstants::CreateSVGPoint					DontDelete|Function 0
 createSVGMatrix				SVGSVGElementConstants::CreateSVGMatrix					DontDelete|Function 0
 createSVGRect					SVGSVGElementConstants::CreateSVGRect					DontDelete|Function 0
 createSVGTransform				SVGSVGElementConstants::CreateSVGTransform				DontDelete|Function 0
 createSVGTransformFromMatrix	SVGSVGElementConstants::CreateSVGTransformFromMatrix	DontDelete|Function 1
 getElementById					SVGSVGElementConstants::GetElementById					DontDelete|Function 1
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGSVGElement", SVGSVGElementProto, SVGSVGElementProtoFunc)

ValueImp *SVGSVGElement::getValueProperty(ExecState *exec, int token) const
{
	KDOM_ENTER_SAFE
	
	switch(token)
	{
		case SVGSVGElementConstants::X:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, x());
		case SVGSVGElementConstants::Y:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, y());
		case SVGSVGElementConstants::Width:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, width());
		case SVGSVGElementConstants::Height:
			return KDOM::safe_cache<SVGAnimatedLength>(exec, height());
		case SVGSVGElementConstants::ContentScriptType:
			return getDOMString(contentScriptType());
		case SVGSVGElementConstants::ContentStyleType:
			return getDOMString(contentStyleType());
		case SVGSVGElementConstants::Viewport:
			return KDOM::safe_cache<SVGRect>(exec, viewport());
		case SVGSVGElementConstants::PixelUnitToMillimeterX:
			return Number(pixelUnitToMillimeterX());
		case SVGSVGElementConstants::PixelUnitToMillimeterY:
			return Number(pixelUnitToMillimeterY());
		case SVGSVGElementConstants::ScreenPixelToMillimeterX:
			return Number(screenPixelToMillimeterX());
		case SVGSVGElementConstants::ScreenPixelToMillimeterY:
			return Number(screenPixelToMillimeterY());
		case SVGSVGElementConstants::UseCurrentView:
			return KJS::Boolean(useCurrentView());
		// TODO: case SVGSVGElementConstants::CurrentView:
		case SVGSVGElementConstants::CurrentScale:
			return Number(currentScale());
		case SVGSVGElementConstants::CurrentTranslate:
			return KDOM::safe_cache<SVGPoint>(exec, currentTranslate());
	default:
		kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
	return Undefined();
}

void SVGSVGElement::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
	KDOM_ENTER_SAFE

	switch(token)
	{
		case SVGSVGElementConstants::ContentScriptType:
		{
			setContentScriptType(KDOM::toDOMString(exec, value));
			return;
		}
		case SVGSVGElementConstants::ContentStyleType:
		{
			setContentStyleType(KDOM::toDOMString(exec, value));
			return;
		}
		case SVGSVGElementConstants::CurrentScale:
		{
			setCurrentScale(value->toNumber(exec));
			return;
		}
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	KDOM_LEAVE_SAFE(SVGException)
}

ValueImp *SVGSVGElementProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
	SVGSVGElement obj(cast(exec, thisObj));
	KDOM_ENTER_SAFE

	switch(id)
	{
		case SVGSVGElementConstants::CreateSVGNumber:
			return KDOM::safe_cache<SVGNumber>(exec, obj.createSVGNumber());
		case SVGSVGElementConstants::CreateSVGLength:
			return KDOM::safe_cache<SVGLength>(exec, obj.createSVGLength());
		case SVGSVGElementConstants::CreateSVGAngle:
			return KDOM::safe_cache<SVGAngle>(exec, obj.createSVGAngle());
		case SVGSVGElementConstants::CreateSVGPoint:
			return KDOM::safe_cache<SVGPoint>(exec, obj.createSVGPoint());
		case SVGSVGElementConstants::CreateSVGMatrix:
			return KDOM::safe_cache<SVGMatrix>(exec, obj.createSVGMatrix());
		case SVGSVGElementConstants::CreateSVGRect:
			return KDOM::safe_cache<SVGRect>(exec, obj.createSVGRect());
		case SVGSVGElementConstants::CreateSVGTransform:
			return KDOM::safe_cache<SVGTransform>(exec, obj.createSVGTransform());
		case SVGSVGElementConstants::CreateSVGTransformFromMatrix:
		{
			SVGMatrix matrix = KDOM::ecma_cast<SVGMatrix>(exec, args[0], &toSVGMatrix);
			return KDOM::safe_cache<SVGTransform>(exec, obj.createSVGTransformFromMatrix(matrix));
		}
		case SVGSVGElementConstants::SuspendRedraw:
		{
			unsigned long max_wait_milliseconds = args[0]->toUInt32(exec);;
			obj.suspendRedraw(max_wait_milliseconds);
			return Undefined();
		}
		case SVGSVGElementConstants::UnsuspendRedraw:
		{
			unsigned long suspend_handle_id = args[0]->toUInt32(exec);;
			obj.unsuspendRedraw(suspend_handle_id);
			return Undefined();
		}
		case SVGSVGElementConstants::UnsuspendRedrawAll:
		{
			obj.unsuspendRedrawAll();
			return Undefined();
		}
		case SVGSVGElementConstants::ForceRedraw:
		{
			obj.forceRedraw();
			return Undefined();
		}
		case SVGSVGElementConstants::PauseAnimations:
		{
			obj.pauseAnimations();
			return Undefined();
		}
		case SVGSVGElementConstants::UnpauseAnimations:
		{
			obj.unpauseAnimations();
			return Undefined();
		}
		case SVGSVGElementConstants::AnimationsPaused:
			return KJS::Boolean(obj.animationsPaused());
		case SVGSVGElementConstants::GetCurrentTime:
		{
			return Number(obj.getCurrentTime());
		}
		case SVGSVGElementConstants::SetCurrentTime:
		{
			obj.setCurrentTime(args[0]->toNumber(exec));
			return Undefined();
		}
		case SVGSVGElementConstants::GetIntersectionList:
		{
			SVGRect rect = KDOM::ecma_cast<SVGRect>(exec, args[0], &toSVGRect);
			SVGElement element = KDOM::ecma_cast<SVGElement>(exec, args[1], &toSVGElement);
			return KDOM::safe_cache<KDOM::NodeList>(exec, obj.getIntersectionList(rect, element));
		}
		case SVGSVGElementConstants::GetEnclosureList:
		{
			SVGRect rect = KDOM::ecma_cast<SVGRect>(exec, args[0], &toSVGRect);
			SVGElement element = KDOM::ecma_cast<SVGElement>(exec, args[1], &toSVGElement);
			return KDOM::safe_cache<KDOM::NodeList>(exec, obj.getEnclosureList(rect, element));
		}
		case SVGSVGElementConstants::CheckIntersection:
		{
			SVGElement element = KDOM::ecma_cast<SVGElement>(exec, args[0], &toSVGElement);
			SVGRect rect = KDOM::ecma_cast<SVGRect>(exec, args[1], &toSVGRect);
			return KJS::Boolean(obj.checkIntersection(element, rect));
		}
		case SVGSVGElementConstants::CheckEnclosure:
		{
			SVGElement element = KDOM::ecma_cast<SVGElement>(exec, args[0], &toSVGElement);
			SVGRect rect = KDOM::ecma_cast<SVGRect>(exec, args[1], &toSVGRect);
			return KJS::Boolean(obj.checkEnclosure(element, rect));
		}
		case SVGSVGElementConstants::DeselectAll:
		{
			obj.deselectAll();
			return Undefined();
		}
		case SVGSVGElementConstants::GetElementById:
		default:
			kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
	}

	KDOM_LEAVE_CALL_SAFE(SVGException)
	return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGSVGElementImpl *>(SVGElement::d))

SVGSVGElement SVGSVGElement::null;

SVGSVGElement::SVGSVGElement()
: SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(),
  SVGStylable(), SVGLocatable(), SVGFitToViewBox(), SVGZoomAndPan(), KDOM::DocumentEvent()
{
}

SVGSVGElement::SVGSVGElement(SVGSVGElementImpl *i)
: SVGElement(i), SVGTests(i), SVGLangSpace(i), SVGExternalResourcesRequired(i),
  SVGStylable(i), SVGLocatable(i), SVGFitToViewBox(i), SVGZoomAndPan(i), KDOM::DocumentEvent(i)
{
}

SVGSVGElement::SVGSVGElement(const SVGSVGElement &other)
: SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(),
  SVGStylable(), SVGLocatable(), SVGFitToViewBox(), SVGZoomAndPan(), KDOM::DocumentEvent()
{
	(*this) = other;
}

SVGSVGElement::SVGSVGElement(const KDOM::Node &other)
: SVGElement(), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(),
  SVGStylable(), SVGLocatable(), SVGFitToViewBox(), SVGZoomAndPan(), KDOM::DocumentEvent()
{
	(*this) = other;
}

SVGSVGElement::~SVGSVGElement()
{
}

SVGSVGElement &SVGSVGElement::operator=(const SVGSVGElement &other)
{
	SVGElement::operator=(other);
	SVGTests::operator=(other);
	SVGLangSpace::operator=(other);
	SVGExternalResourcesRequired::operator=(other);
	SVGStylable::operator=(other);
	SVGLocatable::operator=(other);
	SVGFitToViewBox::operator=(other);
	SVGZoomAndPan::operator=(other);
	KDOM::DocumentEvent::operator=(other);
	return *this;
}

SVGSVGElement &SVGSVGElement::operator=(const KDOM::Node &other)
{
	SVGSVGElementImpl *ohandle = static_cast<SVGSVGElementImpl *>(other.handle());
	if(impl != ohandle)
	{
		if(!ohandle || ohandle->nodeType() != KDOM::ELEMENT_NODE)
		{
			if(impl)
				impl->deref();

			Node::d = 0;
		}
		else
		{
			SVGElement::operator=(other);
			SVGTests::operator=(ohandle);
			SVGLangSpace::operator=(ohandle);
			SVGExternalResourcesRequired::operator=(ohandle);
			SVGStylable::operator=(ohandle);
			SVGLocatable::operator=(ohandle);
			SVGFitToViewBox::operator=(ohandle);
			SVGZoomAndPan::operator=(ohandle);
			KDOM::DocumentEvent::operator=(ohandle);
		}
	}

	return *this;
}

SVGAnimatedLength SVGSVGElement::x() const
{
	if(!impl)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->x());
}

SVGAnimatedLength SVGSVGElement::y() const
{
	if(!impl)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->y());
}

SVGAnimatedLength SVGSVGElement::width() const
{
	if(!impl)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->width());
}

SVGAnimatedLength SVGSVGElement::height() const
{
	if(!impl)
		return SVGAnimatedLength::null;

	return SVGAnimatedLength(impl->height());
}

KDOM::DOMString SVGSVGElement::contentScriptType() const
{
	if(!impl)
		return KDOM::DOMString();

	return impl->contentScriptType();
}

void SVGSVGElement::setContentScriptType(const KDOM::DOMString &type)
{
	if(impl)
		impl->setContentScriptType(type);
}
			
KDOM::DOMString SVGSVGElement::contentStyleType() const
{
	if(!impl)
		return KDOM::DOMString();

	return impl->contentStyleType();
}

void SVGSVGElement::setContentStyleType(const KDOM::DOMString &type)
{
	if(impl)
		impl->setContentStyleType(type);
}

SVGRect SVGSVGElement::viewport() const
{
	if(!impl)
		return SVGRect::null;

	return SVGRect(impl->viewport());
}

float SVGSVGElement::pixelUnitToMillimeterX() const
{
	if(!impl)
		return 0.0;

	return impl->pixelUnitToMillimeterX();
}

float SVGSVGElement::pixelUnitToMillimeterY() const
{
	if(!impl)
		return 0.0;

	return impl->pixelUnitToMillimeterY();
}

float SVGSVGElement::screenPixelToMillimeterX() const
{
	if(!impl)
		return 0.0;

	return impl->screenPixelToMillimeterX();
}

float SVGSVGElement::screenPixelToMillimeterY() const
{
	if(!impl)
		return 0.0;

	return impl->screenPixelToMillimeterY();
}

bool SVGSVGElement::useCurrentView() const
{
	if(!impl)
		return false;

	return impl->useCurrentView();
}

void SVGSVGElement::setUseCurrentView(bool currentView)
{
	if(impl)
		impl->setUseCurrentView(currentView);
}

/*
SVGViewSpec SVGSVGElement::currentView() const
{
	if(!impl)
		return SVGViewSpec();

	return SVGViewSpec(impl->currentView()); 
}*/

float SVGSVGElement::currentScale() const
{
	if(!impl)
		return 0.0;

	return impl->currentScale();
}

void SVGSVGElement::setCurrentScale(float scale)
{
	if(impl)
		impl->setCurrentScale(scale);
}

SVGPoint SVGSVGElement::currentTranslate() const
{
	if(!impl)
		return SVGPoint::null;
	
	return SVGPoint(impl->currentTranslate());
}

unsigned long SVGSVGElement::suspendRedraw(unsigned long max_wait_milliseconds)
{
	if(!impl)
		return 0;

	return impl->suspendRedraw(max_wait_milliseconds);
}

void SVGSVGElement::unsuspendRedraw(unsigned long suspend_handle_id)
{
	if(impl)
		impl->unsuspendRedraw(suspend_handle_id);
}

void SVGSVGElement::unsuspendRedrawAll()
{
	if(impl)
		impl->unsuspendRedrawAll();
}

void SVGSVGElement::forceRedraw()
{
	if(impl)
		impl->forceRedraw();
}

void SVGSVGElement::pauseAnimations()
{
	if(impl)
		impl->pauseAnimations();
}

void SVGSVGElement::unpauseAnimations()
{
	if(impl)
		impl->unpauseAnimations();
}

bool SVGSVGElement::animationsPaused()
{
	if(!impl)
		return false;

	return impl->animationsPaused();
}

float SVGSVGElement::getCurrentTime()
{
	if(!impl)
		return 0.;

	return impl->getCurrentTime();
}

void SVGSVGElement::setCurrentTime(float seconds)
{
	if(impl)
		impl->setCurrentTime(seconds);
}


KDOM::NodeList SVGSVGElement::getIntersectionList(const SVGRect &rect, const SVGElement &referenceElement)
{
	if(!impl)
		return KDOM::NodeList::null;

	return KDOM::NodeList(impl->getIntersectionList(rect.handle(), static_cast<SVGElementImpl *>(referenceElement.handle())));
}

KDOM::NodeList SVGSVGElement::getEnclosureList(const SVGRect &rect, const SVGElement &referenceElement)
{
	if(!impl)
		return KDOM::NodeList::null;

	return KDOM::NodeList(impl->getEnclosureList(rect.handle(), static_cast<SVGElementImpl *>(referenceElement.handle())));
}

bool SVGSVGElement::checkIntersection(const SVGElement &element, const SVGRect &rect)
{
	if(!impl)
		return false;

	return impl->checkIntersection(static_cast<SVGElementImpl *>(element.handle()), rect.handle());
}

bool SVGSVGElement::checkEnclosure(const SVGElement &element, const SVGRect &rect)
{
	if(!impl)
		return false;

	return impl->checkEnclosure(static_cast<SVGElementImpl *>(element.handle()), rect.handle());
}

void SVGSVGElement::deselectAll()
{
	if(impl)
		impl->deselectAll();
}

SVGNumber SVGSVGElement::createSVGNumber() const
{
	if(!impl)
		return SVGNumber::null;

	return SVGNumber(impl->createSVGNumber());
}

SVGLength SVGSVGElement::createSVGLength() const
{
	if(!impl)
		return SVGLength::null;

	return SVGLength(impl->createSVGLength());
}

SVGAngle SVGSVGElement::createSVGAngle() const
{
	if(!impl)
		return SVGAngle::null;

	return SVGAngle(impl->createSVGAngle());
}

SVGPoint SVGSVGElement::createSVGPoint() const
{
	if(!impl)
		return SVGPoint::null;

	return SVGPoint(impl->createSVGPoint());
}

SVGMatrix SVGSVGElement::createSVGMatrix() const
{
	if(!impl)
		return SVGMatrix::null;

	return SVGMatrix(impl->createSVGMatrix());
}

SVGRect SVGSVGElement::createSVGRect() const
{
	if(!impl)
		return SVGRect::null;

	return SVGRect(impl->createSVGRect());
}

SVGTransform SVGSVGElement::createSVGTransform() const
{
	if(!impl)
		return SVGTransform::null;

	return SVGTransform(impl->createSVGTransform());
}

SVGTransform SVGSVGElement::createSVGTransformFromMatrix(const SVGMatrix &matrix) const
{
	if(!impl)
		return SVGTransform::null;

	return SVGTransform(impl->createSVGTransformFromMatrix(matrix.handle()));
}

// vim:ts=4:noet
