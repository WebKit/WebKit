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
#include "dom_exception.h"
#include "dom_string.h"

#include "css_extensions.h"
#include "css_extensionsimpl.h"
using namespace DOM;


CSS2Azimuth::CSS2Azimuth() : CSSValue()
{
}

CSS2Azimuth::CSS2Azimuth(const CSS2Azimuth &other) : CSSValue(other)
{
}

CSS2Azimuth::CSS2Azimuth(CSS2AzimuthImpl *impl) : CSSValue(impl)
{
}

CSS2Azimuth &CSS2Azimuth::operator = (const CSS2Azimuth &other)
{
    CSSValue::operator = (other);
    return *this;
}

CSS2Azimuth::~CSS2Azimuth()
{
}

unsigned short CSS2Azimuth::azimuthType() const
{
    if(!impl) return 0;
    return ((CSS2AzimuthImpl *)impl)->azimuthType();
}

DOMString CSS2Azimuth::identifier() const
{
    if(!impl) return 0;
    return ((CSS2AzimuthImpl *)impl)->identifier();
}

bool CSS2Azimuth::behind() const
{
    if(!impl) return 0;
    return ((CSS2AzimuthImpl *)impl)->behind();
}

void CSS2Azimuth::setAngleValue( const unsigned short unitType, const float floatValue )
{
    if(impl)
        ((CSS2AzimuthImpl *)impl)->setAngleValue( unitType, floatValue );
}

float CSS2Azimuth::getAngleValue( const unsigned short unitType )
{
    if(!impl) return 0;
    return ((CSS2AzimuthImpl *)impl)->getAngleValue( unitType );
}

void CSS2Azimuth::setIdentifier( const DOMString &identifier, const bool behind )
{
    if(impl)
        ((CSS2AzimuthImpl *)impl)->setIdentifier( identifier, behind );
}



CSS2BackgroundPosition::CSS2BackgroundPosition() : CSSValue()
{
}

CSS2BackgroundPosition::CSS2BackgroundPosition(const CSS2BackgroundPosition &other) : CSSValue(other)
{
}

CSS2BackgroundPosition::CSS2BackgroundPosition(CSS2BackgroundPositionImpl *impl) : CSSValue(impl)
{
}

CSS2BackgroundPosition &CSS2BackgroundPosition::operator = (const CSS2BackgroundPosition &other)
{
    CSSValue::operator = (other);
    return *this;
}

CSS2BackgroundPosition::~CSS2BackgroundPosition()
{
}

unsigned short CSS2BackgroundPosition::horizontalType() const
{
    if(!impl) return 0;
    return ((CSS2BackgroundPositionImpl *)impl)->horizontalType();
}

unsigned short CSS2BackgroundPosition::verticalType() const
{
    if(!impl) return 0;
    return ((CSS2BackgroundPositionImpl *)impl)->verticalType();
}

DOMString CSS2BackgroundPosition::horizontalIdentifier() const
{
    if(!impl) return 0;
    return ((CSS2BackgroundPositionImpl *)impl)->horizontalIdentifier();
}

DOMString CSS2BackgroundPosition::verticalIdentifier() const
{
    if(!impl) return 0;
    return ((CSS2BackgroundPositionImpl *)impl)->verticalIdentifier();
}

float CSS2BackgroundPosition::getHorizontalPosition( const float horizontalType )
{
    if(!impl) return 0;
    return ((CSS2BackgroundPositionImpl *)impl)->getHorizontalPosition( horizontalType );
}

float CSS2BackgroundPosition::getVerticalPosition( const float verticalType )
{
    if(!impl) return 0;
    return ((CSS2BackgroundPositionImpl *)impl)->getVerticalPosition( verticalType );
}

void CSS2BackgroundPosition::setHorizontalPosition( const unsigned short horizontalType, const float value )
{
    if(impl)
        ((CSS2BackgroundPositionImpl *)impl)->setHorizontalPosition( horizontalType, value );
}

void CSS2BackgroundPosition::setVerticalPosition( const unsigned short verticalType, const float value )
{
    if(impl)
        ((CSS2BackgroundPositionImpl *)impl)->setVerticalPosition( verticalType, value );
}

void CSS2BackgroundPosition::setPositionIdentifier( const DOMString &horizontalIdentifier, const DOMString &verticalIdentifier )
{
    if(impl)
        ((CSS2BackgroundPositionImpl *)impl)->setPositionIdentifier( horizontalIdentifier, verticalIdentifier );
}



CSS2BorderSpacing::CSS2BorderSpacing() : CSSValue()
{
}

CSS2BorderSpacing::CSS2BorderSpacing(const CSS2BorderSpacing &other) : CSSValue(other)
{
}

CSS2BorderSpacing::CSS2BorderSpacing(CSS2BorderSpacingImpl *impl) : CSSValue(impl)
{
}

CSS2BorderSpacing &CSS2BorderSpacing::operator = (const CSS2BorderSpacing &other)
{
    CSSValue::operator = (other);
    return *this;
}

CSS2BorderSpacing::~CSS2BorderSpacing()
{
}

unsigned short CSS2BorderSpacing::horizontalType() const
{
    if(!impl) return 0;
    return ((CSS2BorderSpacingImpl *)impl)->horizontalType();
}

unsigned short CSS2BorderSpacing::verticalType() const
{
    if(!impl) return 0;
    return ((CSS2BorderSpacingImpl *)impl)->verticalType();
}

float CSS2BorderSpacing::getHorizontalSpacing( const float horizontalType )
{
    if(!impl) return 0;
    return ((CSS2BorderSpacingImpl *)impl)->getHorizontalSpacing( horizontalType );
}

float CSS2BorderSpacing::getVerticalSpacing( const float verticalType )
{
    if(!impl) return 0;
    return ((CSS2BorderSpacingImpl *)impl)->getVerticalSpacing( verticalType );
}

void CSS2BorderSpacing::setHorizontalSpacing( const unsigned short horizontalType, const float value )
{
    if(impl)
        ((CSS2BorderSpacingImpl *)impl)->setHorizontalSpacing( horizontalType, value );
}

void CSS2BorderSpacing::setVerticalSpacing( const unsigned short verticalType, const float value )
{
    if(impl)
        ((CSS2BorderSpacingImpl *)impl)->setVerticalSpacing( verticalType, value );
}

void CSS2BorderSpacing::setInherit()(  )
{
    if(impl)
        ((CSS2BorderSpacingImpl *)impl)->setInherit()(  );
}



CSS2CounterIncrement::CSS2CounterIncrement()
{
}

CSS2CounterIncrement::CSS2CounterIncrement(const CSS2CounterIncrement &other)
{
}

CSS2CounterIncrement::CSS2CounterIncrement(CSS2CounterIncrementImpl *impl)
{
}

CSS2CounterIncrement &CSS2CounterIncrement::operator = (const CSS2CounterIncrement &other)
{
    ::operator = (other);
    return *this;
}

CSS2CounterIncrement::~CSS2CounterIncrement()
{
}

DOMString CSS2CounterIncrement::identifier() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("identifier");
}

void CSS2CounterIncrement::setIdentifier( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("identifier", value);
}

short CSS2CounterIncrement::increment() const
{
    if(!impl) return 0;
    return ((CSS2CounterIncrementImpl *)impl)->increment();
}

void CSS2CounterIncrement::setIncrement( const short _increment )
{

    if(impl)
        ((CSS2CounterIncrementImpl *)impl)->setIncrement( _increment );
}




CSS2CounterReset::CSS2CounterReset()
{
}

CSS2CounterReset::CSS2CounterReset(const CSS2CounterReset &other)
{
}

CSS2CounterReset::CSS2CounterReset(CSS2CounterResetImpl *impl)
{
}

CSS2CounterReset &CSS2CounterReset::operator = (const CSS2CounterReset &other)
{
    ::operator = (other);
    return *this;
}

CSS2CounterReset::~CSS2CounterReset()
{
}

DOMString CSS2CounterReset::identifier() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("identifier");
}

void CSS2CounterReset::setIdentifier( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("identifier", value);
}

short CSS2CounterReset::reset() const
{
    if(!impl) return 0;
    return ((CSS2CounterResetImpl *)impl)->reset();
}

void CSS2CounterReset::setReset( const short _reset )
{

    if(impl)
        ((CSS2CounterResetImpl *)impl)->setReset( _reset );
}




CSS2Cursor::CSS2Cursor() : CSSValue()
{
}

CSS2Cursor::CSS2Cursor(const CSS2Cursor &other) : CSSValue(other)
{
}

CSS2Cursor::CSS2Cursor(CSS2CursorImpl *impl) : CSSValue(impl)
{
}

CSS2Cursor &CSS2Cursor::operator = (const CSS2Cursor &other)
{
    CSSValue::operator = (other);
    return *this;
}

CSS2Cursor::~CSS2Cursor()
{
}

unsigned short CSS2Cursor::cursorType() const
{
    if(!impl) return 0;
    return ((CSS2CursorImpl *)impl)->cursorType();
}

void CSS2Cursor::setCursorType( const unsigned short _cursorType )
{

    if(impl)
        ((CSS2CursorImpl *)impl)->setCursorType( _cursorType );
}

CSSValueList CSS2Cursor::uris() const
{
    if(!impl) return 0;
    return ((CSS2CursorImpl *)impl)->uris();
}

DOMString CSS2Cursor::predefinedCursor() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("predefinedCursor");
}

void CSS2Cursor::setPredefinedCursor( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("predefinedCursor", value);
}



CSS2FontFaceSrc::CSS2FontFaceSrc()
{
}

CSS2FontFaceSrc::CSS2FontFaceSrc(const CSS2FontFaceSrc &other)
{
}

CSS2FontFaceSrc::CSS2FontFaceSrc(CSS2FontFaceSrcImpl *impl)
{
}

CSS2FontFaceSrc &CSS2FontFaceSrc::operator = (const CSS2FontFaceSrc &other)
{
    ::operator = (other);
    return *this;
}

CSS2FontFaceSrc::~CSS2FontFaceSrc()
{
}

DOMString CSS2FontFaceSrc::uri() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("uri");
}

void CSS2FontFaceSrc::setUri( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("uri", value);
}

CSSValueList CSS2FontFaceSrc::format() const
{
    if(!impl) return 0;
    return ((CSS2FontFaceSrcImpl *)impl)->format();
}

DOMString CSS2FontFaceSrc::fontFaceName() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("fontFaceName");
}

void CSS2FontFaceSrc::setFontFaceName( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("fontFaceName", value);
}



CSS2FontFaceWidths::CSS2FontFaceWidths()
{
}

CSS2FontFaceWidths::CSS2FontFaceWidths(const CSS2FontFaceWidths &other)
{
}

CSS2FontFaceWidths::CSS2FontFaceWidths(CSS2FontFaceWidthsImpl *impl)
{
}

CSS2FontFaceWidths &CSS2FontFaceWidths::operator = (const CSS2FontFaceWidths &other)
{
    ::operator = (other);
    return *this;
}

CSS2FontFaceWidths::~CSS2FontFaceWidths()
{
}

DOMString CSS2FontFaceWidths::urange() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("urange");
}

void CSS2FontFaceWidths::setUrange( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("urange", value);
}

CSSValueList CSS2FontFaceWidths::numbers() const
{
    if(!impl) return 0;
    return ((CSS2FontFaceWidthsImpl *)impl)->numbers();
}




CSS2PageSize::CSS2PageSize() : CSSValue()
{
}

CSS2PageSize::CSS2PageSize(const CSS2PageSize &other) : CSSValue(other)
{
}

CSS2PageSize::CSS2PageSize(CSS2PageSizeImpl *impl) : CSSValue(impl)
{
}

CSS2PageSize &CSS2PageSize::operator = (const CSS2PageSize &other)
{
    CSSValue::operator = (other);
    return *this;
}

CSS2PageSize::~CSS2PageSize()
{
}

unsigned short CSS2PageSize::widthType() const
{
    if(!impl) return 0;
    return ((CSS2PageSizeImpl *)impl)->widthType();
}

unsigned short CSS2PageSize::heightType() const
{
    if(!impl) return 0;
    return ((CSS2PageSizeImpl *)impl)->heightType();
}

DOMString CSS2PageSize::identifier() const
{
    if(!impl) return 0;
    return ((CSS2PageSizeImpl *)impl)->identifier();
}

float CSS2PageSize::getWidth( const float widthType )
{
    if(!impl) return 0;
    return ((CSS2PageSizeImpl *)impl)->getWidth( widthType );
}

float CSS2PageSize::getHeightSize( const float heightType )
{
    if(!impl) return 0;
    return ((CSS2PageSizeImpl *)impl)->getHeightSize( heightType );
}

void CSS2PageSize::setWidthSize( const unsigned short widthType, const float value )
{
    if(impl)
        ((CSS2PageSizeImpl *)impl)->setWidthSize( widthType, value );
}

void CSS2PageSize::setHeightSize( const unsigned short heightType, const float value )
{
    if(impl)
        ((CSS2PageSizeImpl *)impl)->setHeightSize( heightType, value );
}

void CSS2PageSize::setIdentifier( const DOMString &identifier )
{
    if(impl)
        ((CSS2PageSizeImpl *)impl)->setIdentifier( identifier );
}



CSS2PlayDuring::CSS2PlayDuring() : CSSValue()
{
}

CSS2PlayDuring::CSS2PlayDuring(const CSS2PlayDuring &other) : CSSValue(other)
{
}

CSS2PlayDuring::CSS2PlayDuring(CSS2PlayDuringImpl *impl) : CSSValue(impl)
{
}

CSS2PlayDuring &CSS2PlayDuring::operator = (const CSS2PlayDuring &other)
{
    CSSValue::operator = (other);
    return *this;
}

CSS2PlayDuring::~CSS2PlayDuring()
{
}

unsigned short CSS2PlayDuring::playDuringType() const
{
    if(!impl) return 0;
    return ((CSS2PlayDuringImpl *)impl)->playDuringType();
}

DOMString CSS2PlayDuring::playDuringIdentifier() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("playDuringIdentifier");
}

void CSS2PlayDuring::setPlayDuringIdentifier( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("playDuringIdentifier", value);
}

DOMString CSS2PlayDuring::uri() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("uri");
}

void CSS2PlayDuring::setUri( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("uri", value);
}

bool CSS2PlayDuring::mix() const
{
    if(!impl) return 0;
    return ((CSS2PlayDuringImpl *)impl)->mix();
}

void CSS2PlayDuring::setMix( const bool _mix )
{

    if(impl)
        ((CSS2PlayDuringImpl *)impl)->setMix( _mix );
}

bool CSS2PlayDuring::repeat() const
{
    if(!impl) return 0;
    return ((CSS2PlayDuringImpl *)impl)->repeat();
}

void CSS2PlayDuring::setRepeat( const bool _repeat )
{

    if(impl)
        ((CSS2PlayDuringImpl *)impl)->setRepeat( _repeat );
}



CSS2Properties::CSS2Properties()
{
}

CSS2Properties::CSS2Properties(const CSS2Properties &other)
{
}

CSS2Properties::CSS2Properties(CSS2PropertiesImpl *impl)
{
}

CSS2Properties &CSS2Properties::operator = (const CSS2Properties &other)
{
    ::operator = (other);
    return *this;
}

CSS2Properties::~CSS2Properties()
{
}

DOMString CSS2Properties::azimuth() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("azimuth");
}

void CSS2Properties::setAzimuth( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("azimuth", value);
}

DOMString CSS2Properties::background() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("background");
}

void CSS2Properties::setBackground( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("background", value);
}

DOMString CSS2Properties::backgroundAttachment() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("backgroundAttachment");
}

void CSS2Properties::setBackgroundAttachment( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("backgroundAttachment", value);
}

DOMString CSS2Properties::backgroundColor() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("backgroundColor");
}

void CSS2Properties::setBackgroundColor( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("backgroundColor", value);
}

DOMString CSS2Properties::backgroundImage() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("backgroundImage");
}

void CSS2Properties::setBackgroundImage( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("backgroundImage", value);
}

DOMString CSS2Properties::backgroundPosition() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("backgroundPosition");
}

void CSS2Properties::setBackgroundPosition( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("backgroundPosition", value);
}

DOMString CSS2Properties::backgroundRepeat() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("backgroundRepeat");
}

void CSS2Properties::setBackgroundRepeat( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("backgroundRepeat", value);
}

DOMString CSS2Properties::border() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("border");
}

void CSS2Properties::setBorder( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("border", value);
}

DOMString CSS2Properties::borderCollapse() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("borderCollapse");
}

void CSS2Properties::setBorderCollapse( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("borderCollapse", value);
}

DOMString CSS2Properties::borderColor() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("borderColor");
}

void CSS2Properties::setBorderColor( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("borderColor", value);
}

DOMString CSS2Properties::borderSpacing() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("borderSpacing");
}

void CSS2Properties::setBorderSpacing( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("borderSpacing", value);
}

DOMString CSS2Properties::borderStyle() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("borderStyle");
}

void CSS2Properties::setBorderStyle( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("borderStyle", value);
}

DOMString CSS2Properties::borderTop() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("borderTop");
}

void CSS2Properties::setBorderTop( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("borderTop", value);
}

DOMString CSS2Properties::borderRight() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("borderRight");
}

void CSS2Properties::setBorderRight( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("borderRight", value);
}

DOMString CSS2Properties::borderBottom() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("borderBottom");
}

void CSS2Properties::setBorderBottom( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("borderBottom", value);
}

DOMString CSS2Properties::borderLeft() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("borderLeft");
}

void CSS2Properties::setBorderLeft( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("borderLeft", value);
}

DOMString CSS2Properties::borderTopColor() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("borderTopColor");
}

void CSS2Properties::setBorderTopColor( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("borderTopColor", value);
}

DOMString CSS2Properties::borderRightColor() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("borderRightColor");
}

void CSS2Properties::setBorderRightColor( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("borderRightColor", value);
}

DOMString CSS2Properties::borderBottomColor() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("borderBottomColor");
}

void CSS2Properties::setBorderBottomColor( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("borderBottomColor", value);
}

DOMString CSS2Properties::borderLeftColor() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("borderLeftColor");
}

void CSS2Properties::setBorderLeftColor( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("borderLeftColor", value);
}

DOMString CSS2Properties::borderTopStyle() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("borderTopStyle");
}

void CSS2Properties::setBorderTopStyle( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("borderTopStyle", value);
}

DOMString CSS2Properties::borderRightStyle() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("borderRightStyle");
}

void CSS2Properties::setBorderRightStyle( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("borderRightStyle", value);
}

DOMString CSS2Properties::borderBottomStyle() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("borderBottomStyle");
}

void CSS2Properties::setBorderBottomStyle( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("borderBottomStyle", value);
}

DOMString CSS2Properties::borderLeftStyle() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("borderLeftStyle");
}

void CSS2Properties::setBorderLeftStyle( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("borderLeftStyle", value);
}

DOMString CSS2Properties::borderTopWidth() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("borderTopWidth");
}

void CSS2Properties::setBorderTopWidth( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("borderTopWidth", value);
}

DOMString CSS2Properties::borderRightWidth() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("borderRightWidth");
}

void CSS2Properties::setBorderRightWidth( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("borderRightWidth", value);
}

DOMString CSS2Properties::borderBottomWidth() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("borderBottomWidth");
}

void CSS2Properties::setBorderBottomWidth( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("borderBottomWidth", value);
}

DOMString CSS2Properties::borderLeftWidth() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("borderLeftWidth");
}

void CSS2Properties::setBorderLeftWidth( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("borderLeftWidth", value);
}

DOMString CSS2Properties::borderWidth() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("borderWidth");
}

void CSS2Properties::setBorderWidth( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("borderWidth", value);
}

DOMString CSS2Properties::bottom() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("bottom");
}

void CSS2Properties::setBottom( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("bottom", value);
}

DOMString CSS2Properties::captionSide() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("captionSide");
}

void CSS2Properties::setCaptionSide( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("captionSide", value);
}

DOMString CSS2Properties::clear() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("clear");
}

void CSS2Properties::setClear( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("clear", value);
}

DOMString CSS2Properties::clip() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("clip");
}

void CSS2Properties::setClip( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("clip", value);
}

DOMString CSS2Properties::color() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("color");
}

void CSS2Properties::setColor( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("color", value);
}

DOMString CSS2Properties::content() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("content");
}

void CSS2Properties::setContent( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("content", value);
}

DOMString CSS2Properties::counterIncrement() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("counterIncrement");
}

void CSS2Properties::setCounterIncrement( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("counterIncrement", value);
}

DOMString CSS2Properties::counterReset() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("counterReset");
}

void CSS2Properties::setCounterReset( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("counterReset", value);
}

DOMString CSS2Properties::cue() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("cue");
}

void CSS2Properties::setCue( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("cue", value);
}

DOMString CSS2Properties::cueAfter() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("cueAfter");
}

void CSS2Properties::setCueAfter( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("cueAfter", value);
}

DOMString CSS2Properties::cueBefore() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("cueBefore");
}

void CSS2Properties::setCueBefore( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("cueBefore", value);
}

DOMString CSS2Properties::cursor() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("cursor");
}

void CSS2Properties::setCursor( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("cursor", value);
}

DOMString CSS2Properties::direction() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("direction");
}

void CSS2Properties::setDirection( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("direction", value);
}

DOMString CSS2Properties::display() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("display");
}

void CSS2Properties::setDisplay( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("display", value);
}

DOMString CSS2Properties::elevation() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("elevation");
}

void CSS2Properties::setElevation( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("elevation", value);
}

DOMString CSS2Properties::emptyCells() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("emptyCells");
}

void CSS2Properties::setEmptyCells( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("emptyCells", value);
}

DOMString CSS2Properties::cssFloat() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("cssFloat");
}

void CSS2Properties::setCssFloat( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("cssFloat", value);
}

DOMString CSS2Properties::font() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("font");
}

void CSS2Properties::setFont( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("font", value);
}

DOMString CSS2Properties::fontFamily() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("fontFamily");
}

void CSS2Properties::setFontFamily( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("fontFamily", value);
}

DOMString CSS2Properties::fontSize() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("fontSize");
}

void CSS2Properties::setFontSize( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("fontSize", value);
}

DOMString CSS2Properties::fontSizeAdjust() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("fontSizeAdjust");
}

void CSS2Properties::setFontSizeAdjust( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("fontSizeAdjust", value);
}

DOMString CSS2Properties::fontStretch() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("fontStretch");
}

void CSS2Properties::setFontStretch( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("fontStretch", value);
}

DOMString CSS2Properties::fontStyle() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("fontStyle");
}

void CSS2Properties::setFontStyle( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("fontStyle", value);
}

DOMString CSS2Properties::fontVariant() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("fontVariant");
}

void CSS2Properties::setFontVariant( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("fontVariant", value);
}

DOMString CSS2Properties::fontWeight() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("fontWeight");
}

void CSS2Properties::setFontWeight( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("fontWeight", value);
}

DOMString CSS2Properties::height() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("height");
}

void CSS2Properties::setHeight( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("height", value);
}

DOMString CSS2Properties::left() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("left");
}

void CSS2Properties::setLeft( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("left", value);
}

DOMString CSS2Properties::letterSpacing() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("letterSpacing");
}

void CSS2Properties::setLetterSpacing( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("letterSpacing", value);
}

DOMString CSS2Properties::lineHeight() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("lineHeight");
}

void CSS2Properties::setLineHeight( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("lineHeight", value);
}

DOMString CSS2Properties::listStyle() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("listStyle");
}

void CSS2Properties::setListStyle( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("listStyle", value);
}

DOMString CSS2Properties::listStyleImage() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("listStyleImage");
}

void CSS2Properties::setListStyleImage( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("listStyleImage", value);
}

DOMString CSS2Properties::listStylePosition() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("listStylePosition");
}

void CSS2Properties::setListStylePosition( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("listStylePosition", value);
}

DOMString CSS2Properties::listStyleType() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("listStyleType");
}

void CSS2Properties::setListStyleType( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("listStyleType", value);
}

DOMString CSS2Properties::margin() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("margin");
}

void CSS2Properties::setMargin( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("margin", value);
}

DOMString CSS2Properties::marginTop() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("marginTop");
}

void CSS2Properties::setMarginTop( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("marginTop", value);
}

DOMString CSS2Properties::marginRight() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("marginRight");
}

void CSS2Properties::setMarginRight( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("marginRight", value);
}

DOMString CSS2Properties::marginBottom() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("marginBottom");
}

void CSS2Properties::setMarginBottom( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("marginBottom", value);
}

DOMString CSS2Properties::marginLeft() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("marginLeft");
}

void CSS2Properties::setMarginLeft( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("marginLeft", value);
}

DOMString CSS2Properties::markerOffset() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("markerOffset");
}

void CSS2Properties::setMarkerOffset( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("markerOffset", value);
}

DOMString CSS2Properties::marks() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("marks");
}

void CSS2Properties::setMarks( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("marks", value);
}

DOMString CSS2Properties::maxHeight() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("maxHeight");
}

void CSS2Properties::setMaxHeight( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("maxHeight", value);
}

DOMString CSS2Properties::maxWidth() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("maxWidth");
}

void CSS2Properties::setMaxWidth( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("maxWidth", value);
}

DOMString CSS2Properties::minHeight() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("minHeight");
}

void CSS2Properties::setMinHeight( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("minHeight", value);
}

DOMString CSS2Properties::minWidth() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("minWidth");
}

void CSS2Properties::setMinWidth( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("minWidth", value);
}

DOMString CSS2Properties::orphans() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("orphans");
}

void CSS2Properties::setOrphans( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("orphans", value);
}

DOMString CSS2Properties::outline() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("outline");
}

void CSS2Properties::setOutline( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("outline", value);
}

DOMString CSS2Properties::outlineColor() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("outlineColor");
}

void CSS2Properties::setOutlineColor( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("outlineColor", value);
}

DOMString CSS2Properties::outlineStyle() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("outlineStyle");
}

void CSS2Properties::setOutlineStyle( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("outlineStyle", value);
}

DOMString CSS2Properties::outlineWidth() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("outlineWidth");
}

void CSS2Properties::setOutlineWidth( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("outlineWidth", value);
}

DOMString CSS2Properties::overflow() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("overflow");
}

void CSS2Properties::setOverflow( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("overflow", value);
}

DOMString CSS2Properties::padding() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("padding");
}

void CSS2Properties::setPadding( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("padding", value);
}

DOMString CSS2Properties::paddingTop() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("paddingTop");
}

void CSS2Properties::setPaddingTop( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("paddingTop", value);
}

DOMString CSS2Properties::paddingRight() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("paddingRight");
}

void CSS2Properties::setPaddingRight( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("paddingRight", value);
}

DOMString CSS2Properties::paddingBottom() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("paddingBottom");
}

void CSS2Properties::setPaddingBottom( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("paddingBottom", value);
}

DOMString CSS2Properties::paddingLeft() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("paddingLeft");
}

void CSS2Properties::setPaddingLeft( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("paddingLeft", value);
}

DOMString CSS2Properties::page() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("page");
}

void CSS2Properties::setPage( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("page", value);
}

DOMString CSS2Properties::pageBreakAfter() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("pageBreakAfter");
}

void CSS2Properties::setPageBreakAfter( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("pageBreakAfter", value);
}

DOMString CSS2Properties::pageBreakBefore() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("pageBreakBefore");
}

void CSS2Properties::setPageBreakBefore( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("pageBreakBefore", value);
}

DOMString CSS2Properties::pageBreakInside() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("pageBreakInside");
}

void CSS2Properties::setPageBreakInside( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("pageBreakInside", value);
}

DOMString CSS2Properties::pause() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("pause");
}

void CSS2Properties::setPause( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("pause", value);
}

DOMString CSS2Properties::pauseAfter() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("pauseAfter");
}

void CSS2Properties::setPauseAfter( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("pauseAfter", value);
}

DOMString CSS2Properties::pauseBefore() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("pauseBefore");
}

void CSS2Properties::setPauseBefore( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("pauseBefore", value);
}

DOMString CSS2Properties::pitch() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("pitch");
}

void CSS2Properties::setPitch( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("pitch", value);
}

DOMString CSS2Properties::pitchRange() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("pitchRange");
}

void CSS2Properties::setPitchRange( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("pitchRange", value);
}

DOMString CSS2Properties::playDuring() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("playDuring");
}

void CSS2Properties::setPlayDuring( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("playDuring", value);
}

DOMString CSS2Properties::position() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("position");
}

void CSS2Properties::setPosition( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("position", value);
}

DOMString CSS2Properties::quotes() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("quotes");
}

void CSS2Properties::setQuotes( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("quotes", value);
}

DOMString CSS2Properties::richness() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("richness");
}

void CSS2Properties::setRichness( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("richness", value);
}

DOMString CSS2Properties::right() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("right");
}

void CSS2Properties::setRight( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("right", value);
}

DOMString CSS2Properties::size() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("size");
}

void CSS2Properties::setSize( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("size", value);
}

DOMString CSS2Properties::speak() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("speak");
}

void CSS2Properties::setSpeak( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("speak", value);
}

DOMString CSS2Properties::speakHeader() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("speakHeader");
}

void CSS2Properties::setSpeakHeader( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("speakHeader", value);
}

DOMString CSS2Properties::speakNumeral() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("speakNumeral");
}

void CSS2Properties::setSpeakNumeral( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("speakNumeral", value);
}

DOMString CSS2Properties::speakPunctuation() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("speakPunctuation");
}

void CSS2Properties::setSpeakPunctuation( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("speakPunctuation", value);
}

DOMString CSS2Properties::speechRate() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("speechRate");
}

void CSS2Properties::setSpeechRate( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("speechRate", value);
}

DOMString CSS2Properties::stress() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("stress");
}

void CSS2Properties::setStress( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("stress", value);
}

DOMString CSS2Properties::tableLayout() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("tableLayout");
}

void CSS2Properties::setTableLayout( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("tableLayout", value);
}

DOMString CSS2Properties::textAlign() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("textAlign");
}

void CSS2Properties::setTextAlign( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("textAlign", value);
}

DOMString CSS2Properties::textDecoration() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("textDecoration");
}

void CSS2Properties::setTextDecoration( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("textDecoration", value);
}

DOMString CSS2Properties::textIndent() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("textIndent");
}

void CSS2Properties::setTextIndent( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("textIndent", value);
}

DOMString CSS2Properties::textShadow() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("textShadow");
}

void CSS2Properties::setTextShadow( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("textShadow", value);
}

DOMString CSS2Properties::textTransform() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("textTransform");
}

void CSS2Properties::setTextTransform( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("textTransform", value);
}

DOMString CSS2Properties::top() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("top");
}

void CSS2Properties::setTop( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("top", value);
}

DOMString CSS2Properties::unicodeBidi() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("unicodeBidi");
}

void CSS2Properties::setUnicodeBidi( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("unicodeBidi", value);
}

DOMString CSS2Properties::verticalAlign() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("verticalAlign");
}

void CSS2Properties::setVerticalAlign( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("verticalAlign", value);
}

DOMString CSS2Properties::visibility() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("visibility");
}

void CSS2Properties::setVisibility( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("visibility", value);
}

DOMString CSS2Properties::voiceFamily() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("voiceFamily");
}

void CSS2Properties::setVoiceFamily( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("voiceFamily", value);
}

DOMString CSS2Properties::volume() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("volume");
}

void CSS2Properties::setVolume( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("volume", value);
}

DOMString CSS2Properties::whiteSpace() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("whiteSpace");
}

void CSS2Properties::setWhiteSpace( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("whiteSpace", value);
}

DOMString CSS2Properties::widows() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("widows");
}

void CSS2Properties::setWidows( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("widows", value);
}

DOMString CSS2Properties::width() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("width");
}

void CSS2Properties::setWidth( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("width", value);
}

DOMString CSS2Properties::wordSpacing() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("wordSpacing");
}

void CSS2Properties::setWordSpacing( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("wordSpacing", value);
}

DOMString CSS2Properties::zIndex() const
{
    if(!impl) return 0;
    return ((ElementImpl *)impl)->getAttribute("zIndex");
}

void CSS2Properties::setZIndex( const DOMString &value )
{
    if(impl) ((ElementImpl *)impl)->setAttribute("zIndex", value);
}



CSS2TextShadow::CSS2TextShadow()
{
}

CSS2TextShadow::CSS2TextShadow(const CSS2TextShadow &other)
{
}

CSS2TextShadow::CSS2TextShadow(CSS2TextShadowImpl *impl)
{
}

CSS2TextShadow &CSS2TextShadow::operator = (const CSS2TextShadow &other)
{
    ::operator = (other);
    return *this;
}

CSS2TextShadow::~CSS2TextShadow()
{
}

CSSValue CSS2TextShadow::color() const
{
    if(!impl) return 0;
    return ((CSS2TextShadowImpl *)impl)->color();
}

CSSValue CSS2TextShadow::horizontal() const
{
    if(!impl) return 0;
    return ((CSS2TextShadowImpl *)impl)->horizontal();
}

CSSValue CSS2TextShadow::vertical() const
{
    if(!impl) return 0;
    return ((CSS2TextShadowImpl *)impl)->vertical();
}

CSSValue CSS2TextShadow::blur() const
{
    if(!impl) return 0;
    return ((CSS2TextShadowImpl *)impl)->blur();
}



