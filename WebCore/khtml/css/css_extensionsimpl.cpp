/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * $Id$
 */
#include "DOMException.h"
#include "DOMString.h"

#include "CSS2AzimuthImpl.h"
using namespace DOM;

CSS2AzimuthImpl::CSS2AzimuthImpl(DocumentImpl *doc) : CSSValueImpl(doc)
{
}

CSS2AzimuthImpl::~CSS2AzimuthImpl()
{
}

unsigned short CSS2AzimuthImpl::azimuthType() const
{
}

DOMString CSS2AzimuthImpl::identifier() const
{
}

bool CSS2AzimuthImpl::behind() const
{
}

void CSS2AzimuthImpl::setAngleValue( const unsigned short &unitType, const float &floatValue )
{
}

float CSS2AzimuthImpl::getAngleValue( const unsigned short &unitType )
{
}

void CSS2AzimuthImpl::setIdentifier( const DOMString &identifier, const bool &behind )
{
}




#include "DOMException.h"
#include "DOMString.h"

#include "CSS2BackgroundPositionImpl.h"
CSS2BackgroundPositionImpl::CSS2BackgroundPositionImpl(DocumentImpl *doc) : CSSValueImpl(doc)
{
}

CSS2BackgroundPositionImpl::~CSS2BackgroundPositionImpl()
{
}

unsigned short CSS2BackgroundPositionImpl::horizontalType() const
{
}

unsigned short CSS2BackgroundPositionImpl::verticalType() const
{
}

DOMString CSS2BackgroundPositionImpl::horizontalIdentifier() const
{
}

DOMString CSS2BackgroundPositionImpl::verticalIdentifier() const
{
}

float CSS2BackgroundPositionImpl::getHorizontalPosition( const float &horizontalType )
{
}

float CSS2BackgroundPositionImpl::getVerticalPosition( const float &verticalType )
{
}

void CSS2BackgroundPositionImpl::setHorizontalPosition( const unsigned short &horizontalType, const float &value )
{
}

void CSS2BackgroundPositionImpl::setVerticalPosition( const unsigned short &verticalType, const float &value )
{
}

void CSS2BackgroundPositionImpl::setPositionIdentifier( const DOMString &horizontalIdentifier, const DOMString &verticalIdentifier )
{
}




#include "DOMException.h"

#include "CSS2BorderSpacingImpl.h"
CSS2BorderSpacingImpl::CSS2BorderSpacingImpl(DocumentImpl *doc) : CSSValueImpl(doc)
{
}

CSS2BorderSpacingImpl::~CSS2BorderSpacingImpl()
{
}

unsigned short CSS2BorderSpacingImpl::horizontalType() const
{
}

unsigned short CSS2BorderSpacingImpl::verticalType() const
{
}

float CSS2BorderSpacingImpl::getHorizontalSpacing( const float &horizontalType )
{
}

float CSS2BorderSpacingImpl::getVerticalSpacing( const float &verticalType )
{
}

void CSS2BorderSpacingImpl::setHorizontalSpacing( const unsigned short &horizontalType, const float &value )
{
}

void CSS2BorderSpacingImpl::setVerticalSpacing( const unsigned short &verticalType, const float &value )
{
}

void CSS2BorderSpacingImpl::setInherit()(  )
{
}




#include "DOMException.h"
#include "DOMString.h"

#include "CSS2CounterIncrementImpl.h"
CSS2CounterIncrementImpl::CSS2CounterIncrementImpl(DocumentImpl *doc)
{
}

CSS2CounterIncrementImpl::~CSS2CounterIncrementImpl()
{
}

short CSS2CounterIncrementImpl::increment() const
{
}

void CSS2CounterIncrementImpl::setIncrement( const short & )
{
}




#include "DOMException.h"
#include "DOMString.h"

#include "CSS2CounterResetImpl.h"
CSS2CounterResetImpl::CSS2CounterResetImpl(DocumentImpl *doc)
{
}

CSS2CounterResetImpl::~CSS2CounterResetImpl()
{
}

short CSS2CounterResetImpl::reset() const
{
}

void CSS2CounterResetImpl::setReset( const short & )
{
}




#include "CSSValueList.h"
#include "DOMException.h"
#include "DOMString.h"

#include "CSS2CursorImpl.h"
CSS2CursorImpl::CSS2CursorImpl(DocumentImpl *doc) : CSSValueImpl(doc)
{
}

CSS2CursorImpl::~CSS2CursorImpl()
{
}

unsigned short CSS2CursorImpl::cursorType() const
{
}

void CSS2CursorImpl::setCursorType( const unsigned short & )
{
}

CSSValueList CSS2CursorImpl::uris() const
{
}




#include "CSSValueList.h"
#include "DOMException.h"
#include "DOMString.h"

#include "CSS2FontFaceSrcImpl.h"
CSS2FontFaceSrcImpl::CSS2FontFaceSrcImpl(DocumentImpl *doc)
{
}

CSS2FontFaceSrcImpl::~CSS2FontFaceSrcImpl()
{
}

CSSValueList CSS2FontFaceSrcImpl::format() const
{
}




#include "CSSValueList.h"
#include "DOMException.h"
#include "DOMString.h"

#include "CSS2FontFaceWidthsImpl.h"
CSS2FontFaceWidthsImpl::CSS2FontFaceWidthsImpl(DocumentImpl *doc)
{
}

CSS2FontFaceWidthsImpl::~CSS2FontFaceWidthsImpl()
{
}

CSSValueList CSS2FontFaceWidthsImpl::numbers() const
{
}




#include "DOMException.h"
#include "DOMString.h"

#include "CSS2PageSizeImpl.h"
CSS2PageSizeImpl::CSS2PageSizeImpl(DocumentImpl *doc) : CSSValueImpl(doc)
{
}

CSS2PageSizeImpl::~CSS2PageSizeImpl()
{
}

unsigned short CSS2PageSizeImpl::widthType() const
{
}

unsigned short CSS2PageSizeImpl::heightType() const
{
}

DOMString CSS2PageSizeImpl::identifier() const
{
}

float CSS2PageSizeImpl::getWidth( const float &widthType )
{
}

float CSS2PageSizeImpl::getHeightSize( const float &heightType )
{
}

void CSS2PageSizeImpl::setWidthSize( const unsigned short &widthType, const float &value )
{
}

void CSS2PageSizeImpl::setHeightSize( const unsigned short &heightType, const float &value )
{
}

void CSS2PageSizeImpl::setIdentifier( const DOMString &identifier )
{
}




#include "DOMException.h"
#include "DOMString.h"

#include "CSS2PlayDuringImpl.h"
CSS2PlayDuringImpl::CSS2PlayDuringImpl(DocumentImpl *doc) : CSSValueImpl(doc)
{
}

CSS2PlayDuringImpl::~CSS2PlayDuringImpl()
{
}

unsigned short CSS2PlayDuringImpl::playDuringType() const
{
}

bool CSS2PlayDuringImpl::mix() const
{
}

void CSS2PlayDuringImpl::setMix( const bool & )
{
}

bool CSS2PlayDuringImpl::repeat() const
{
}

void CSS2PlayDuringImpl::setRepeat( const bool & )
{
}




#include "DOMString.h"

#include "CSS2PropertiesImpl.h"
CSS2PropertiesImpl::CSS2PropertiesImpl(DocumentImpl *doc)
{
}

CSS2PropertiesImpl::~CSS2PropertiesImpl()
{
}




#include "CSSValue.h"

#include "CSS2TextShadowImpl.h"
CSS2TextShadowImpl::CSS2TextShadowImpl(DocumentImpl *doc)
{
}

CSS2TextShadowImpl::~CSS2TextShadowImpl()
{
}

CSSValue CSS2TextShadowImpl::color() const
{
}

CSSValue CSS2TextShadowImpl::horizontal() const
{
}

CSSValue CSS2TextShadowImpl::vertical() const
{
}

CSSValue CSS2TextShadowImpl::blur() const
{
}



