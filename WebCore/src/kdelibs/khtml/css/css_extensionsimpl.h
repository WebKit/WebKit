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
#ifndef _CSS_css_extensionsimpl_h_
#define _CSS_css_extensionsimpl_h_

#include "css_valueimpl.h"
#include "dom_string.h"

namespace DOM {

class CSS2AzimuthImpl : public CSSValueImpl
{
public:
    CSS2AzimuthImpl(DocumentImpl *doc);

    ~CSS2AzimuthImpl();

    unsigned short azimuthType() const;
    DOM::DOMString identifier() const;
    bool behind() const;
    void setAngleValue ( const unsigned short &unitType, const float &floatValue );
    float getAngleValue ( const unsigned short &unitType );
    void setIdentifier ( const DOM::DOMString &identifier, const bool &behind );
};


class DOM::DOMString;

class CSS2BackgroundPositionImpl : public CSSValueImpl
{
public:
    CSS2BackgroundPositionImpl(DocumentImpl *doc);

    ~CSS2BackgroundPositionImpl();

    unsigned short horizontalType() const;
    unsigned short verticalType() const;
    DOM::DOMString horizontalIdentifier() const;
    DOM::DOMString verticalIdentifier() const;
    float getHorizontalPosition ( const float &horizontalType );
    float getVerticalPosition ( const float &verticalType );
    void setHorizontalPosition ( const unsigned short &horizontalType, const float &value );
    void setVerticalPosition ( const unsigned short &verticalType, const float &value );
    void setPositionIdentifier ( const DOM::DOMString &horizontalIdentifier, const DOM::DOMString &verticalIdentifier );
};



class CSS2BorderSpacingImpl : public CSSValueImpl
{
public:
    CSS2BorderSpacingImpl(DocumentImpl *doc);

    ~CSS2BorderSpacingImpl();

    unsigned short horizontalType() const;
    unsigned short verticalType() const;
    float getHorizontalSpacing ( const float &horizontalType );
    float getVerticalSpacing ( const float &verticalType );
    void setHorizontalSpacing ( const unsigned short &horizontalType, const float &value );
    void setVerticalSpacing ( const unsigned short &verticalType, const float &value );
    void setInherit() (  );
};


class CSS2CounterIncrementImpl
{
public:
    CSS2CounterIncrementImpl(DocumentImpl *doc);

    ~CSS2CounterIncrementImpl();

    short increment() const;
    void setIncrement( const short & );
};


class CSS2CounterResetImpl
{
public:
    CSS2CounterResetImpl(DocumentImpl *doc);

    ~CSS2CounterResetImpl();

    short reset() const;
    void setReset( const short & );
};


class CSS2CursorImpl : public CSSValueImpl
{
public:
    CSS2CursorImpl(DocumentImpl *doc);

    ~CSS2CursorImpl();

    unsigned short cursorType() const;
    void setCursorType( const unsigned short & );

    CSSValueList uris() const;
};


class CSS2FontFaceSrcImpl
{
public:
    CSS2FontFaceSrcImpl(DocumentImpl *doc);

    ~CSS2FontFaceSrcImpl();

    CSSValueList format() const;
};


class CSS2FontFaceWidthsImpl
{
public:
    CSS2FontFaceWidthsImpl(DocumentImpl *doc);

    ~CSS2FontFaceWidthsImpl();

    CSSValueList numbers() const;
};


class CSS2PageSizeImpl : public CSSValueImpl
{
public:
    CSS2PageSizeImpl(DocumentImpl *doc);

    ~CSS2PageSizeImpl();

    unsigned short widthType() const;
    unsigned short heightType() const;
    DOM::DOMString identifier() const;
    float getWidth ( const float &widthType );
    float getHeightSize ( const float &heightType );
    void setWidthSize ( const unsigned short &widthType, const float &value );
    void setHeightSize ( const unsigned short &heightType, const float &value );
    void setIdentifier ( const DOM::DOMString &identifier );
};


class CSS2PlayDuringImpl : public CSSValueImpl
{
public:
    CSS2PlayDuringImpl(DocumentImpl *doc);

    ~CSS2PlayDuringImpl();

    unsigned short playDuringType() const;
    bool mix() const;

    void setMix( const bool & );
    bool repeat() const;

    void setRepeat( const bool & );
};


class CSS2PropertiesImpl
{
public:
    CSS2PropertiesImpl(DocumentImpl *doc);

    ~CSS2PropertiesImpl();
};


class CSS2TextShadowImpl
{
public:
    CSS2TextShadowImpl(DocumentImpl *doc);

    ~CSS2TextShadowImpl();

    CSSValue color() const;
    CSSValue horizontal() const;
    CSSValue vertical() const;
    CSSValue blur() const;
};


}; // namespace

#endif
