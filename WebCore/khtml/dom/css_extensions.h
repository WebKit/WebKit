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
 * This file includes excerpts from the Document Object Model (DOM)
 * Level 2 Specification (Candidate Recommendation)
 * http://www.w3.org/TR/2000/CR-DOM-Level-2-20000510/
 * Copyright © 2000 W3C® (MIT, INRIA, Keio), All Rights Reserved.
 *
 * $Id$
 */
#ifndef _CSS_css_extensions_h_
#define _CSS_css_extensions_h_

#include <css_value.h>
#include <dom/dom_string.h>

namespace DOM {

/**
 * The <code> CSS2Azimuth </code> interface represents the <a
 * href="http://www.w3.org/TR/REC-CSS2/aural.html#propdef-azimuth">
 * azimuth </a> CSS Level 2 property.
 *
 */
class CSS2Azimuth : public CSSValue
{
public:
    CSS2Azimuth();
    CSS2Azimuth(const CSS2Azimuth &other);
    CSS2Azimuth(CSS2AzimuthImpl *impl);
public:

    CSS2Azimuth & operator = (const CSS2Azimuth &other);

    ~CSS2Azimuth();

    /**
     * A code defining the type of the value as defined in <code>
     * CSSValue </code> . It would be one of <code> CSS_DEG </code> ,
     * <code> CSS_RAD </code> , <code> CSS_GRAD </code> or <code>
     * CSS_IDENT </code> .
     *
     */
    unsigned short azimuthType() const;

    /**
     * If <code> azimuthType </code> is <code> CSS_IDENT </code> ,
     * <code> identifier </code> contains one of left-side, far-left,
     * left, center-left, center, center-right, right, far-right,
     * right-side, leftwards, rightwards. The empty string if none is
     * set.
     *
     */
    DOM::DOMString identifier() const;

    /**
     * <code> behind </code> indicates whether the behind identifier
     * has been set.
     *
     */
    bool behind() const;

    /**
     * A method to set the angle value with a specified unit. This
     * method will unset any previously set identifiers values.
     *
     * @param unitType The unitType could only be one of <code>
     * CSS_DEG </code> , <code> CSS_RAD </code> or <code> CSS_GRAD
     * </code> ).
     *
     * @param floatValue The new float value of the angle.
     *
     * @return
     * @exception DOMException
     * INVALID_ACCESS_ERR: Raised if the unit type is invalid.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this property is
     * readonly.
     *
     */
    void setAngleValue ( const unsigned short unitType, const float floatValue );

    /**
     * Used to retrieved the float value of the azimuth property.
     *
     * @param unitType The unit type can be only an angle unit type (
     * <code> CSS_DEG </code> , <code> CSS_RAD </code> or <code>
     * CSS_GRAD </code> ).
     *
     * @return The float value.
     *
     * @exception DOMException
     * INVALID_ACCESS_ERR: Raised if the unit type is invalid.
     *
     */
    float getAngleValue ( const unsigned short unitType );

    /**
     * Setting the identifier for the azimuth property will unset any
     * previously set angle value. The value of <code> azimuthType
     * </code> is set to <code> CSS_IDENT </code>
     *
     * @param identifier The new identifier. If the identifier is
     * "leftwards" or "rightward", the behind attribute is ignored.
     *
     * @param behind The new value for behind.
     *
     * @return
     * @exception DOMException
     * SYNTAX_ERR: Raised if the specified <code> identifier </code>
     * has a syntax error and is unparsable.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this property is
     * readonly.
     *
     */
    void setIdentifier ( const DOM::DOMString &identifier, const bool behind );
};


class CSS2BackgroundPositionImpl;

/**
 * The <code> CSS2BackgroundPosition </code> interface represents the
 * <a
 * href="http://www.w3.org/TR/REC-CSS2/colors.html#propdef-background-position">
 * background-position </a> CSS Level 2 property.
 *
 */
class CSS2BackgroundPosition : public CSSValue
{
public:
    CSS2BackgroundPosition();
    CSS2BackgroundPosition(const CSS2BackgroundPosition &other);
    CSS2BackgroundPosition(CSS2BackgroundPositionImpl *impl);
public:

    CSS2BackgroundPosition & operator = (const CSS2BackgroundPosition &other);

    ~CSS2BackgroundPosition();

    /**
     * A code defining the type of the horizontal value. It would be
     * one <code> CSS_PERCENTAGE </code> , <code> CSS_EMS </code> ,
     * <code> CSS_EXS </code> , <code> CSS_PX </code> , <code> CSS_CM
     * </code> , <code> CSS_MM </code> , <code> CSS_IN </code> ,
     * <code> CSS_PT </code> , <code> CSS_PC </code> , <code>
     * CSS_IDENT </code> , <code> CSS_INHERIT </code> . If one of
     * horizontal or vertical is <code> CSS_IDENT </code> or <code>
     * CSS_INHERIT </code> , it's guaranteed that the other is the
     * same.
     *
     */
    unsigned short horizontalType() const;

    /**
     * A code defining the type of the horizontal value. The code can
     * be one of the following units : <code> CSS_PERCENTAGE </code> ,
     * <code> CSS_EMS </code> , <code> CSS_EXS </code> , <code> CSS_PX
     * </code> , <code> CSS_CM </code> , <code> CSS_MM </code> ,
     * <code> CSS_IN </code> , <code> CSS_PT </code> , <code> CSS_PC
     * </code> , <code> CSS_IDENT </code> , <code> CSS_INHERIT </code>
     * . If one of horizontal or vertical is <code> CSS_IDENT </code>
     * or <code> CSS_INHERIT </code> , it's guaranteed that the other
     * is the same.
     *
     */
    unsigned short verticalType() const;

    /**
     * If <code> horizontalType </code> is <code> CSS_IDENT </code> or
     * <code> CSS_INHERIT </code> , this attribute contains the string
     * representation of the ident, otherwise it contains an empty
     * string.
     *
     */
    DOM::DOMString horizontalIdentifier() const;

    /**
     * If <code> verticalType </code> is <code> CSS_IDENT </code> or
     * <code> CSS_INHERIT </code> , this attribute contains the string
     * representation of the ident, otherwise it contains an empty
     * string. The value is <code> "center" </code> if only the
     * horizontalIdentifier has been set. The value is <code>
     * "inherit" </code> if the horizontalIdentifier is <code>
     * "inherit" </code> .
     *
     */
    DOM::DOMString verticalIdentifier() const;

    /**
     * This method is used to get the float value in a specified unit
     * if the <code> horizontalPosition </code> represents a length or
     * a percentage. If the float doesn't contain a float value or
     * can't be converted into the specified unit, a <code>
     * DOMException </code> is raised.
     *
     * @param horizontalType The specified unit.
     *
     * @return The float value.
     *
     * @exception DOMException
     * INVALID_ACCESS_ERR: Raises if the property doesn't contain a
     * float or the value can't be converted.
     *
     */
    float getHorizontalPosition ( const float horizontalType );

    /**
     * This method is used to get the float value in a specified unit
     * if the <code> verticalPosition </code> represents a length or a
     * percentage. If the float doesn't contain a float value or can't
     * be converted into the specified unit, a <code> DOMException
     * </code> is raised. The value is <code> 50% </code> if only the
     * horizontal value has been specified.
     *
     * @param verticalType The specified unit.
     *
     * @return The float value.
     *
     * @exception DOMException
     * INVALID_ACCESS_ERR: Raises if the property doesn't contain a
     * float or the value can't be converted.
     *
     */
    float getVerticalPosition ( const float verticalType );

    /**
     * This method is used to set the horizontal position with a
     * specified unit. If the vertical value is not a percentage or a
     * length, it sets the vertical position to <code> 50% </code> .
     *
     * @param horizontalType The specified unit (a length or a
     * percentage).
     *
     * @param value The new value.
     *
     * @return
     * @exception DOMException
     * INVALID_ACCESS_ERR: Raises if the specified unit is not a
     * length or a percentage.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raises if this property is
     * readonly.
     *
     */
    void setHorizontalPosition ( const unsigned short horizontalType, const float value );

    /**
     * This method is used to set the vertical position with a
     * specified unit. If the horizontal value is not a percentage or
     * a length, it sets the vertical position to <code> 50% </code> .
     *
     * @param verticalType The specified unit (a length or a
     * percentage).
     *
     * @param value The new value.
     *
     * @return
     * @exception DOMException
     * INVALID_ACCESS_ERR: Raises if the specified unit is not a
     * length or a percentage.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raises if this property is
     * readonly.
     *
     */
    void setVerticalPosition ( const unsigned short verticalType, const float value );

    /**
     * Sets the identifiers. If the second identifier is the empty
     * string, the vertical identifier is set to his default value (
     * <code> "center" </code> ). If the first identfier is <code>
     * "inherit </code> , the second identifier is ignored and is set
     * to <code> "inherit" </code> .
     *
     * @param horizontalIdentifier The new horizontal identifier.
     *
     * @param verticalIdentifier The new vertical identifier.
     *
     * @return
     * @exception DOMException
     * SYNTAX_ERR: Raises if the identifiers have a syntax error and
     * is unparsable.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raises if this property is
     * readonly.
     *
     */
    void setPositionIdentifier ( const DOM::DOMString &horizontalIdentifier, const DOM::DOMString &verticalIdentifier );
};


class CSS2BorderSpacingImpl;

/**
 * The <code> CSS2BorderSpacing </code> interface represents the <a
 * href="http://www.w3.org/TR/REC-CSS2/tables.html#propdef-border-spacing">
 * border-spacing </a> CSS Level 2 property.
 *
 */
class CSS2BorderSpacing : public CSSValue
{
public:
    CSS2BorderSpacing();
    CSS2BorderSpacing(const CSS2BorderSpacing &other);
    CSS2BorderSpacing(CSS2BorderSpacingImpl *impl);
public:

    CSS2BorderSpacing & operator = (const CSS2BorderSpacing &other);

    ~CSS2BorderSpacing();

    /**
     * The A code defining the type of the value as defined in <code>
     * CSSValue </code> . It would be one of <code> CSS_EMS </code> ,
     * <code> CSS_EXS </code> , <code> CSS_PX </code> , <code> CSS_CM
     * </code> , <code> CSS_MM </code> , <code> CSS_IN </code> ,
     * <code> CSS_PT </code> , <code> CSS_PC </code> or <code>
     * CSS_INHERIT </code> .
     *
     */
    unsigned short horizontalType() const;

    /**
     * The A code defining the type of the value as defined in <code>
     * CSSValue </code> . It would be one of <code> CSS_EMS </code> ,
     * <code> CSS_EXS </code> , <code> CSS_PX </code> , <code> CSS_CM
     * </code> , <code> CSS_MM </code> , <code> CSS_IN </code> ,
     * <code> CSS_PT </code> , <code> CSS_PC </code> or <code>
     * CSS_INHERIT </code> .
     *
     */
    unsigned short verticalType() const;

    /**
     * This method is used to get the float value in a specified unit
     * if the <code> horizontalSpacing </code> represents a length. If
     * the float doesn't contain a float value or can't be converted
     * into the specified unit, a <code> DOMException </code> is
     * raised.
     *
     * @param horizontalType The specified unit.
     *
     * @return The float value.
     *
     * @exception DOMException
     * INVALID_ACCESS_ERR: Raises if the property doesn't contain a
     * float or the value can't be converted.
     *
     */
    float getHorizontalSpacing ( const float horizontalType );

    /**
     * This method is used to get the float value in a specified unit
     * if the <code> verticalSpacing </code> represents a length. If
     * the float doesn't contain a float value or can't be converted
     * into the specified unit, a <code> DOMException </code> is
     * raised. The value is <code> 0 </code> if only the horizontal
     * value has been specified.
     *
     * @param verticalType The specified unit.
     *
     * @return The float value.
     *
     * @exception DOMException
     * INVALID_ACCESS_ERR: Raises if the property doesn't contain a
     * float or the value can't be converted.
     *
     */
    float getVerticalSpacing ( const float verticalType );

    /**
     * This method is used to set the horizontal spacing with a
     * specified unit. If the vertical value is a length, it sets the
     * vertical spacing to <code> 0 </code> .
     *
     * @param horizontalType The specified unit.
     *
     * @param value The new value.
     *
     * @return
     * @exception DOMException
     * INVALID_ACCESS_ERR: Raises if the specified unit is not a
     * length.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raises if this property is
     * readonly.
     *
     */
    void setHorizontalSpacing ( const unsigned short horizontalType, const float value );

    /**
     * This method is used to set the vertical spacing with a
     * specified unit. If the horizontal value is not a length, it
     * sets the vertical spacing to <code> 0 </code> .
     *
     * @param verticalType The specified unit.
     *
     * @param value The new value.
     *
     * @return
     * @exception DOMException
     * INVALID_ACCESS_ERR: Raises if the specified unit is not a
     * length or a percentage.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raises if this property is
     * readonly.
     *
     */
    void setVerticalSpacing ( const unsigned short verticalType, const float value );

    /**
     * Set this property as inherit. <code> horizontalType </code> and
     * <code> verticalType </code> will be inherited.
     *
     * @return
     */
    void setInherit() (  );
};


class CSS2CounterIncrementImpl;

/**
 * The <code> CSS2CounterIncrement </code> interface represents a
 * imple value for the <a
 * href="http://www.w3.org/TR/REC-CSS2/generate.html#propdef-counter-increment">
 * counter-increment </a> CSS Level 2 property.
 *
 */
class CSS2CounterIncrement
{
public:
    CSS2CounterIncrement();
    CSS2CounterIncrement(const CSS2CounterIncrement &other);
    CSS2CounterIncrement(CSS2CounterIncrementImpl *impl);
public:

    CSS2CounterIncrement & operator = (const CSS2CounterIncrement &other);

    ~CSS2CounterIncrement();

    /**
     * The element name.
     *
     */
    DOM::DOMString identifier() const;

    /**
     * see @ref identifier
     * @exception DOMException
     * SYNTAX_ERR: Raised if the specified identifier has a syntax
     * error and is unparsable.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this identifier is
     * readonly.
     *
     */
    void setIdentifier( const DOM::DOMString & );

    /**
     * The increment (default value is 1).
     *
     */
    short increment() const;

    /**
     * see @ref increment
     * @exception DOMException
     * NO_MODIFICATION_ALLOWED_ERR: Raised if this identifier is
     * readonly.
     *
     */
    void setIncrement( const short  );
};


class CSS2CounterResetImpl;

/**
 * The <code> CSS2CounterReset </code> interface represents a simple
 * value for the <a
 * href="http://www.w3.org/TR/REC-CSS2/generate.html#propdef-counter-reset">
 * counter-reset </a> CSS Level 2 property.
 *
 */
class CSS2CounterReset
{
public:
    CSS2CounterReset();
    CSS2CounterReset(const CSS2CounterReset &other);
    CSS2CounterReset(CSS2CounterResetImpl *impl);
public:

    CSS2CounterReset & operator = (const CSS2CounterReset &other);

    ~CSS2CounterReset();

    /**
     * The element name.
     *
     */
    DOM::DOMString identifier() const;

    /**
     * see @ref identifier
     * @exception DOMException
     * SYNTAX_ERR: Raised if the specified identifier has a syntax
     * error and is unparsable.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this identifier is
     * readonly.
     *
     */
    void setIdentifier( const DOM::DOMString & );

    /**
     * The reset (default value is 0).
     *
     */
    short reset() const;

    /**
     * see @ref reset
     * @exception DOMException
     * NO_MODIFICATION_ALLOWED_ERR: Raised if this identifier is
     * readonly.
     *
     */
    void setReset( const short  );
};


class CSS2CursorImpl;
class CSSValueList;

/**
 * The <code> CSS2Cursor </code> interface represents the <a
 * href="http://www.w3.org/TR/REC-CSS2/ui.html#propdef-cursor"> cursor
 * </a> CSS Level 2 property.
 *
 */
class CSS2Cursor : public CSSValue
{
public:
    CSS2Cursor();
    CSS2Cursor(const CSS2Cursor &other);
    CSS2Cursor(CSS2CursorImpl *impl);
public:

    CSS2Cursor & operator = (const CSS2Cursor &other);

    ~CSS2Cursor();

    /**
     * A code defining the type of the property. It would one of
     * <code> CSS_UNKNOWN </code> or <code> CSS_INHERIT </code> . If
     * the type is <code> CSS_UNKNOWN </code> , then <code> uris
     * </code> contains a list of URIs and <code> predefinedCursor
     * </code> contains an ident. Setting this attribute from <code>
     * CSS_INHERIT </code> to <code> CSS_UNKNOWN </code> will set the
     * <code> predefinedCursor </code> to <code> "auto" </code> .
     *
     */
    unsigned short cursorType() const;

    /**
     * see @ref cursorType
     */
    void setCursorType( const unsigned short  );

    /**
     * <code> uris </code> represents the list of URIs ( <code>
     * CSS_URI </code> ) on the cursor property. The list can be
     * empty.
     *
     */
    CSSValueList uris() const;

    /**
     * This identifier represents a generic cursor name or an empty
     * string.
     *
     */
    DOM::DOMString predefinedCursor() const;

    /**
     * see @ref predefinedCursor
     * @exception DOMException
     * SYNTAX_ERR: Raised if the specified CSS string value has a
     * syntax error and is unparsable.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this declaration is
     * readonly.
     *
     */
    void setPredefinedCursor( const DOM::DOMString & );
};


class CSS2FontFaceSrcImpl;
class CSSValueList;

/**
 * The <code> CSS2Cursor </code> interface represents the <a
 * href="http://www.w3.org/TR/REC-CSS2/fonts.html#descdef-src"> src
 * </a> CSS Level 2 descriptor.
 *
 */
class CSS2FontFaceSrc
{
public:
    CSS2FontFaceSrc();
    CSS2FontFaceSrc(const CSS2FontFaceSrc &other);
    CSS2FontFaceSrc(CSS2FontFaceSrcImpl *impl);
public:

    CSS2FontFaceSrc & operator = (const CSS2FontFaceSrc &other);

    ~CSS2FontFaceSrc();

    /**
     * Specifies the source of the font, empty string otherwise.
     *
     */
    DOM::DOMString uri() const;

    /**
     * see @ref uri
     * @exception DOMException
     * SYNTAX_ERR: Raised if the specified CSS string value has a
     * syntax error and is unparsable.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this declaration is
     * readonly.
     *
     */
    void setUri( const DOM::DOMString & );

    /**
     * This attribute contains a list of strings for the format CSS
     * function.
     *
     */
    CSSValueList format() const;

    /**
     * Specifies the full font name of a locally installed font.
     *
     */
    DOM::DOMString fontFaceName() const;

    /**
     * see @ref fontFaceName
     * @exception DOMException
     * SYNTAX_ERR: Raised if the specified CSS string value has a
     * syntax error and is unparsable.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this declaration is
     * readonly.
     *
     */
    void setFontFaceName( const DOM::DOMString & );
};


class CSS2FontFaceWidthsImpl;
class CSSValueList;

/**
 * The <code> CSS2Cursor </code> interface represents a simple value
 * for the <a
 * href="http://www.w3.org/TR/REC-CSS2/fonts.html#descdef-widths">
 * widths </a> CSS Level 2 descriptor.
 *
 */
class CSS2FontFaceWidths
{
public:
    CSS2FontFaceWidths();
    CSS2FontFaceWidths(const CSS2FontFaceWidths &other);
    CSS2FontFaceWidths(CSS2FontFaceWidthsImpl *impl);
public:

    CSS2FontFaceWidths & operator = (const CSS2FontFaceWidths &other);

    ~CSS2FontFaceWidths();

    /**
     * The range for the characters.
     *
     */
    DOM::DOMString urange() const;

    /**
     * see @ref urange
     * @exception DOMException
     * SYNTAX_ERR: Raised if the specified CSS string value has a
     * syntax error and is unparsable.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this declaration is
     * readonly.
     *
     */
    void setUrange( const DOM::DOMString & );

    /**
     * A list of numbers representing the glyph widths.
     *
     */
    CSSValueList numbers() const;
};


class CSS2PageSizeImpl;

/**
 * The <code> CSS2Cursor </code> interface represents the <a
 * href="http://www.w3.org/TR/REC-CSS2/page.html#propdef-size"> size
 * </a> CSS Level 2 descriptor.
 *
 */
class CSS2PageSize : public CSSValue
{
public:
    CSS2PageSize();
    CSS2PageSize(const CSS2PageSize &other);
    CSS2PageSize(CSS2PageSizeImpl *impl);
public:

    CSS2PageSize & operator = (const CSS2PageSize &other);

    ~CSS2PageSize();

    /**
     * A code defining the type of the width of the page. It would be
     * one of <code> CSS_EMS </code> , <code> CSS_EXS </code> , <code>
     * CSS_PX </code> , <code> CSS_CM </code> , <code> CSS_MM </code>
     * , <code> CSS_IN </code> , <code> CSS_PT </code> , <code> CSS_PC
     * </code> , <code> CSS_IDENT </code> , <code> CSS_INHERIT </code>
     * . If one of width or height is <code> CSS_IDENT </code> or
     * <code> CSS_INHERIT </code> , it's guaranteed that the other is
     * the same.
     *
     */
    unsigned short widthType() const;

    /**
     * A code defining the type of the height of the page. It would be
     * one of <code> CSS_EMS </code> , <code> CSS_EXS </code> , <code>
     * CSS_PX </code> , <code> CSS_CM </code> , <code> CSS_MM </code>
     * , <code> CSS_IN </code> , <code> CSS_PT </code> , <code> CSS_PC
     * </code> , <code> CSS_IDENT </code> , <code> CSS_INHERIT </code>
     * . If one of width or height is <code> CSS_IDENT </code> or
     * <code> CSS_INHERIT </code> , it's guaranteed that the other is
     * the same.
     *
     */
    unsigned short heightType() const;

    /**
     * If <code> width </code> is <code> CSS_IDENT </code> or <code>
     * CSS_INHERIT </code> , this attribute contains the string
     * representation of the ident, otherwise it contains an empty
     * string.
     *
     */
    DOM::DOMString identifier() const;

    /**
     * This method is used to get the float value in a specified unit
     * if the <code> widthType </code> represents a length. If the
     * float doesn't contain a float value or can't be converted into
     * the specified unit, a <code> DOMException </code> is raised.
     *
     * @param widthType The specified unit.
     *
     * @return The float value.
     *
     * @exception DOMException
     * INVALID_ACCESS_ERR: Raises if the property doesn't contain a
     * float or the value can't be converted.
     *
     */
    float getWidth ( const float widthType );

    /**
     * This method is used to get the float value in a specified unit
     * if the <code> heightType </code> represents a length. If the
     * float doesn't contain a float value or can't be converted into
     * the specified unit, a <code> DOMException </code> is raised. If
     * only the width value has been specified, the height value is
     * the same.
     *
     * @param heightType The specified unit.
     *
     * @return The float value.
     *
     * @exception DOMException
     * INVALID_ACCESS_ERR: Raises if the property doesn't contain a
     * float or the value can't be converted.
     *
     */
    float getHeightSize ( const float heightType );

    /**
     * This method is used to set the width position with a specified
     * unit. If the <code> heightType </code> is not a length, it sets
     * the height position to the same value.
     *
     * @param widthType The specified unit.
     *
     * @param value The new value.
     *
     * @return
     * @exception DOMException
     * INVALID_ACCESS_ERR: Raises if the specified unit is not a
     * length or a percentage.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raises if this property is
     * readonly.
     *
     */
    void setWidthSize ( const unsigned short widthType, const float value );

    /**
     * This method is used to set the height position with a specified
     * unit. If the <code> widthType </code> is not a length, it sets
     * the width position to the same value.
     *
     * @param heightType The specified unit.
     *
     * @param value The new value.
     *
     * @return
     * @exception DOMException
     * INVALID_ACCESS_ERR: Raises if the specified unit is not a
     * length or a percentage.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raises if this property is
     * readonly.
     *
     */
    void setHeightSize ( const unsigned short heightType, const float value );

    /**
     * Sets the identifier.
     *
     * @param identifier The new identifier.
     *
     * @return
     * @exception DOMException
     * SYNTAX_ERR: Raises if the identifier has a syntax error and is
     * unparsable.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raises if this property is
     * readonly.
     *
     */
    void setIdentifier ( const DOM::DOMString &identifier );
};


class CSS2PlayDuringImpl;

/**
 * The <code> CSS2PlayDuring </code> interface represents the <a
 * href="http://www.w3.org/TR/REC-CSS2/aural.html#propdef-play-during">
 * play-during </a> CSS Level 2 property.
 *
 */
class CSS2PlayDuring : public CSSValue
{
public:
    CSS2PlayDuring();
    CSS2PlayDuring(const CSS2PlayDuring &other);
    CSS2PlayDuring(CSS2PlayDuringImpl *impl);
public:

    CSS2PlayDuring & operator = (const CSS2PlayDuring &other);

    ~CSS2PlayDuring();

    /**
     * A code defining the type of the value as define in <code>
     * CSSvalue </code> . It would be one of <code> CSS_UNKNOWN
     * </code> , <code> CSS_INHERIT </code> , <code> CSS_IDENT </code>
     * .
     *
     */
    unsigned short playDuringType() const;

    /**
     * One of <code> "inherit" </code> , <code> "auto" </code> ,
     * <code> "none" </code> or the empty string if the <code>
     * playDuringType </code> is <code> CSS_UNKNOWN </code> . On
     * setting, it will set the <code> uri </code> to the empty string
     * and <code> mix </code> and <code> repeat </code> to <code>
     * false </code> .
     *
     */
    DOM::DOMString playDuringIdentifier() const;

    /**
     * see @ref playDuringIdentifier
     * @exception DOMException
     * SYNTAX_ERR: Raised if the specified CSS string value has a
     * syntax error and is unparsable.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this declaration is
     * readonly.
     *
     */
    void setPlayDuringIdentifier( const DOM::DOMString & );

    /**
     * The sound specified by the <code> uri </code> . It will set the
     * <code> playDuringType </code> attribute to <code> CSS_UNKNOWN
     * </code> .
     *
     */
    DOM::DOMString uri() const;

    /**
     * see @ref uri
     * @exception DOMException
     * SYNTAX_ERR: Raised if the specified CSS string value has a
     * syntax error and is unparsable.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this declaration is
     * readonly.
     *
     */
    void setUri( const DOM::DOMString & );

    /**
     * <code> true </code> if the sound should be mixed. It will be
     * ignored if the attribute doesn't contain a <code> uri </code> .
     *
     */
    bool mix() const;

    /**
     * see @ref mix
     * @exception DOMException
     * NO_MODIFICATION_ALLOWED_ERR: Raised if this declaration is
     * readonly.
     *
     */
    void setMix( const bool  );

    /**
     * <code> true </code> if the sound should be repeated. It will be
     * ignored if the attribute doesn't contain a <code> uri </code> .
     *
     */
    bool repeat() const;

    /**
     * see @ref repeat
     * @exception DOMException
     * NO_MODIFICATION_ALLOWED_ERR: Raised if this declaration is
     * readonly.
     *
     */
    void setRepeat( const bool );
};


class CSS2PropertiesImpl;

/**
 * The <code> CSS2Properties </code> interface represents a
 * convenience mechanism for retrieving and setting properties within
 * a <code> CSSStyleDeclaration </code> . The attributes of this
 * interface correspond to all the <a
 * href="http://www.w3.org/TR/REC-CSS2/propidx.html"> properties
 * specified in CSS2 </a> . Getting an attribute of this interface is
 * equivalent to calling the <code> getPropertyValue </code> method of
 * the <code> CSSStyleDeclaration </code> interface. Setting an
 * attribute of this interface is equivalent to calling the <code>
 * setProperty </code> method of the <code> CSSStyleDeclaration
 * </code> interface.
 *
 *  A compliant implementation is not required to implement the <code>
 * CSS2Properties </code> interface. If an implementation does
 * implement this interface, the expectation is that language-specific
 * methods can be used to cast from an instance of the <code>
 * CSSStyleDeclaration </code> interface to the <code> CSS2Properties
 * </code> interface.
 *
 *  If an implementation does implement this interface, it is expected
 * to understand the specific syntax of the shorthand properties, and
 * apply their semantics; when the <code> margin </code> property is
 * set, for example, the <code> marginTop </code> , <code> marginRight
 * </code> , <code> marginBottom </code> and <code> marginLeft </code>
 * properties are actually being set by the underlying implementation.
 *
 *  When dealing with CSS "shorthand" properties, the shorthand
 * properties should be decomposed into their component longhand
 * properties as appropriate, and when querying for their value, the
 * form returned should be the shortest form exactly equivalent to the
 * declarations made in the ruleset. However, if there is no shorthand
 * declaration that could be added to the ruleset without changing in
 * any way the rules already declared in the ruleset (i.e., by adding
 * longhand rules that were previously not declared in the ruleset),
 * then the empty string should be returned for the shorthand
 * property.
 *
 *  For example, querying for the <code> font </code> property should
 * not return "normal normal normal 14pt/normal Arial, sans-serif",
 * when "14pt Arial, sans-serif" suffices (the normals are initial
 * values, and are implied by use of the longhand property).
 *
 *  If the values for all the longhand properties that compose a
 * particular string are the initial values, then a string consisting
 * of all the initial values should be returned (e.g. a <code>
 * border-width </code> value of "medium" should be returned as such,
 * not as "").
 *
 *  For some shorthand properties that take missing values from other
 * sides, such as the <code> margin </code> , <code> padding </code> ,
 * and <code> border-[width|style|color] </code> properties, the
 * minimum number of sides possible should be used, i.e., "0px 10px"
 * will be returned instead of "0px 10px 0px 10px".
 *
 *  If the value of a shorthand property can not be decomposed into
 * its component longhand properties, as is the case for the <code>
 * font </code> property with a value of "menu", querying for the
 * values of the component longhand properties should return the empty
 * string.
 *
 */
class CSS2Properties
{
public:
    CSS2Properties();
    CSS2Properties(const CSS2Properties &other);
    CSS2Properties(CSS2PropertiesImpl *impl);
public:

    CSS2Properties & operator = (const CSS2Properties &other);

    ~CSS2Properties();

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/aural.html#propdef-azimuth">
     * azimuth property definition </a> in CSS2.
     *
     */
    DOM::DOMString azimuth() const;

    /**
     * see @ref azimuth
     */
    void setAzimuth( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/colors.html#propdef-background">
     * background property definition </a> in CSS2.
     *
     */
    DOM::DOMString background() const;

    /**
     * see @ref background
     */
    void setBackground( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/colors.html#propdef-background-attachment">
     * background-attachment property definition </a> in CSS2.
     *
     */
    DOM::DOMString backgroundAttachment() const;

    /**
     * see @ref backgroundAttachment
     */
    void setBackgroundAttachment( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/colors.html#propdef-background-color">
     * background-color property definition </a> in CSS2.
     *
     */
    DOM::DOMString backgroundColor() const;

    /**
     * see @ref backgroundColor
     */
    void setBackgroundColor( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/colors.html#propdef-background-image">
     * background-image property definition </a> in CSS2.
     *
     */
    DOM::DOMString backgroundImage() const;

    /**
     * see @ref backgroundImage
     */
    void setBackgroundImage( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/colors.html#propdef-background-position">
     * background-position property definition </a> in CSS2.
     *
     */
    DOM::DOMString backgroundPosition() const;

    /**
     * see @ref backgroundPosition
     */
    void setBackgroundPosition( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/colors.html#propdef-background-repeat">
     * background-repeat property definition </a> in CSS2.
     *
     */
    DOM::DOMString backgroundRepeat() const;

    /**
     * see @ref backgroundRepeat
     */
    void setBackgroundRepeat( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-border">
     * border property definition </a> in CSS2.
     *
     */
    DOM::DOMString border() const;

    /**
     * see @ref border
     */
    void setBorder( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/tables.html#propdef-border-collapse">
     * border-collapse property definition </a> in CSS2.
     *
     */
    DOM::DOMString borderCollapse() const;

    /**
     * see @ref borderCollapse
     */
    void setBorderCollapse( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-border-color">
     * border-color property definition </a> in CSS2.
     *
     */
    DOM::DOMString borderColor() const;

    /**
     * see @ref borderColor
     */
    void setBorderColor( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/tables.html#propdef-border-spacing">
     * border-spacing property definition </a> in CSS2.
     *
     */
    DOM::DOMString borderSpacing() const;

    /**
     * see @ref borderSpacing
     */
    void setBorderSpacing( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-border-style">
     * border-style property definition </a> in CSS2.
     *
     */
    DOM::DOMString borderStyle() const;

    /**
     * see @ref borderStyle
     */
    void setBorderStyle( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-border-top">
     * border-top property definition </a> in CSS2.
     *
     */
    DOM::DOMString borderTop() const;

    /**
     * see @ref borderTop
     */
    void setBorderTop( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-border-right">
     * border-right property definition </a> in CSS2.
     *
     */
    DOM::DOMString borderRight() const;

    /**
     * see @ref borderRight
     */
    void setBorderRight( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-border-bottom">
     * border-bottom property definition </a> in CSS2.
     *
     */
    DOM::DOMString borderBottom() const;

    /**
     * see @ref borderBottom
     */
    void setBorderBottom( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-border-left">
     * border-left property definition </a> in CSS2.
     *
     */
    DOM::DOMString borderLeft() const;

    /**
     * see @ref borderLeft
     */
    void setBorderLeft( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-border-top-color">
     * border-top-color property definition </a> in CSS2.
     *
     */
    DOM::DOMString borderTopColor() const;

    /**
     * see @ref borderTopColor
     */
    void setBorderTopColor( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-border-right-color">
     * border-right-color property definition </a> in CSS2.
     *
     */
    DOM::DOMString borderRightColor() const;

    /**
     * see @ref borderRightColor
     */
    void setBorderRightColor( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/#propdef-border-bottom-color">
     * border-bottom-color property definition </a> in CSS2.
     *
     */
    DOM::DOMString borderBottomColor() const;

    /**
     * see @ref borderBottomColor
     */
    void setBorderBottomColor( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-border-left-color">
     * border-left-color property definition </a> in CSS2.
     *
     */
    DOM::DOMString borderLeftColor() const;

    /**
     * see @ref borderLeftColor
     */
    void setBorderLeftColor( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-border-top-style">
     * border-top-style property definition </a> in CSS2.
     *
     */
    DOM::DOMString borderTopStyle() const;

    /**
     * see @ref borderTopStyle
     */
    void setBorderTopStyle( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-border-right-style">
     * border-right-style property definition </a> in CSS2.
     *
     */
    DOM::DOMString borderRightStyle() const;

    /**
     * see @ref borderRightStyle
     */
    void setBorderRightStyle( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-border-bottom-style">
     * border-bottom-style property definition </a> in CSS2.
     *
     */
    DOM::DOMString borderBottomStyle() const;

    /**
     * see @ref borderBottomStyle
     */
    void setBorderBottomStyle( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-border-left-style">
     * border-left-style property definition </a> in CSS2.
     *
     */
    DOM::DOMString borderLeftStyle() const;

    /**
     * see @ref borderLeftStyle
     */
    void setBorderLeftStyle( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-border-top-width">
     * border-top-width property definition </a> in CSS2.
     *
     */
    DOM::DOMString borderTopWidth() const;

    /**
     * see @ref borderTopWidth
     */
    void setBorderTopWidth( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-border-right-width">
     * border-right-width property definition </a> in CSS2.
     *
     */
    DOM::DOMString borderRightWidth() const;

    /**
     * see @ref borderRightWidth
     */
    void setBorderRightWidth( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-border-bottom-width">
     * border-bottom-width property definition </a> in CSS2.
     *
     */
    DOM::DOMString borderBottomWidth() const;

    /**
     * see @ref borderBottomWidth
     */
    void setBorderBottomWidth( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-border-left-width">
     * border-left-width property definition </a> in CSS2.
     *
     */
    DOM::DOMString borderLeftWidth() const;

    /**
     * see @ref borderLeftWidth
     */
    void setBorderLeftWidth( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-border-width">
     * border-width property definition </a> in CSS2.
     *
     */
    DOM::DOMString borderWidth() const;

    /**
     * see @ref borderWidth
     */
    void setBorderWidth( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/visuren.html#propdef-bottom">
     * bottom property definition </a> in CSS2.
     *
     */
    DOM::DOMString bottom() const;

    /**
     * see @ref bottom
     */
    void setBottom( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/tables.html#propdef-caption-side">
     * caption-side property definition </a> in CSS2.
     *
     */
    DOM::DOMString captionSide() const;

    /**
     * see @ref captionSide
     */
    void setCaptionSide( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/visuren.html#propdef-clear">
     * clear property definition </a> in CSS2.
     *
     */
    DOM::DOMString clear() const;

    /**
     * see @ref clear
     */
    void setClear( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/visufx#propdef-clip"> clip
     * property definition </a> in CSS2.
     *
     */
    DOM::DOMString clip() const;

    /**
     * see @ref clip
     */
    void setClip( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/colors.html#propdef-color">
     * color property definition </a> in CSS2.
     *
     */
    DOM::DOMString color() const;

    /**
     * see @ref color
     */
    void setColor( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/generate.html#propdef-content">
     * content property definition </a> in CSS2.
     *
     */
    DOM::DOMString content() const;

    /**
     * see @ref content
     */
    void setContent( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/generate.html#propdef-counter-increment">
     * counter-increment property definition </a> in CSS2.
     *
     */
    DOM::DOMString counterIncrement() const;

    /**
     * see @ref counterIncrement
     */
    void setCounterIncrement( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/generate.html#propdef-counter-reset">
     * counter-reset property definition </a> in CSS2.
     *
     */
    DOM::DOMString counterReset() const;

    /**
     * see @ref counterReset
     */
    void setCounterReset( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/aural.html#propdef-cue">
     * cue property definition </a> in CSS2.
     *
     */
    DOM::DOMString cue() const;

    /**
     * see @ref cue
     */
    void setCue( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/aural.html#propdef-cue-fter">
     * cue-after property definition </a> in CSS2.
     *
     */
    DOM::DOMString cueAfter() const;

    /**
     * see @ref cueAfter
     */
    void setCueAfter( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/aural.html#propdef-cue-before">
     * cue-before property definition </a> in CSS2.
     *
     */
    DOM::DOMString cueBefore() const;

    /**
     * see @ref cueBefore
     */
    void setCueBefore( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/ui.html#propdef-cursor">
     * cursor property definition </a> in CSS2.
     *
     */
    DOM::DOMString cursor() const;

    /**
     * see @ref cursor
     */
    void setCursor( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/visuren.html#propdef-direction">
     * direction property definition </a> in CSS2.
     *
     */
    DOM::DOMString direction() const;

    /**
     * see @ref direction
     */
    void setDirection( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/visuren.html#propdef-display">
     * display property definition </a> in CSS2.
     *
     */
    DOM::DOMString display() const;

    /**
     * see @ref display
     */
    void setDisplay( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/aural.html#propdef-elevation">
     * elevation property definition </a> in CSS2.
     *
     */
    DOM::DOMString elevation() const;

    /**
     * see @ref elevation
     */
    void setElevation( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/tables.html#propdef-empty-cells">
     * empty-cells property definition </a> in CSS2.
     *
     */
    DOM::DOMString emptyCells() const;

    /**
     * see @ref emptyCells
     */
    void setEmptyCells( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/visuren.html#propdef-float">
     * float property definition </a> in CSS2.
     *
     */
    DOM::DOMString cssFloat() const;

    /**
     * see @ref cssFloat
     */
    void setCssFloat( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/fonts.html#propdef-font">
     * font property definition </a> in CSS2.
     *
     */
    DOM::DOMString font() const;

    /**
     * see @ref font
     */
    void setFont( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/fonts.html#propdef-font-family">
     * font-family property definition </a> in CSS2.
     *
     */
    DOM::DOMString fontFamily() const;

    /**
     * see @ref fontFamily
     */
    void setFontFamily( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/fonts.html#propdef-font-size">
     * font-size property definition </a> in CSS2.
     *
     */
    DOM::DOMString fontSize() const;

    /**
     * see @ref fontSize
     */
    void setFontSize( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/fonts.html#propdef-font-size-adjust">
     * font-size-adjust property definition </a> in CSS2.
     *
     */
    DOM::DOMString fontSizeAdjust() const;

    /**
     * see @ref fontSizeAdjust
     */
    void setFontSizeAdjust( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/fonts.html#propdef-font-stretch">
     * font-stretch property definition </a> in CSS2.
     *
     */
    DOM::DOMString fontStretch() const;

    /**
     * see @ref fontStretch
     */
    void setFontStretch( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/fonts.html#propdef-font-style">
     * font-style property definition </a> in CSS2.
     *
     */
    DOM::DOMString fontStyle() const;

    /**
     * see @ref fontStyle
     */
    void setFontStyle( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/fonts.html#propdef-font-variant">
     * font-variant property definition </a> in CSS2.
     *
     */
    DOM::DOMString fontVariant() const;

    /**
     * see @ref fontVariant
     */
    void setFontVariant( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/fonts.html#propdef-font-weight">
     * font-weight property definition </a> in CSS2.
     *
     */
    DOM::DOMString fontWeight() const;

    /**
     * see @ref fontWeight
     */
    void setFontWeight( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/visudet.html#propdef-height">
     * height property definition </a> in CSS2.
     *
     */
    DOM::DOMString height() const;

    /**
     * see @ref height
     */
    void setHeight( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/visuren.html#propdef-left">
     * left property definition </a> in CSS2.
     *
     */
    DOM::DOMString left() const;

    /**
     * see @ref left
     */
    void setLeft( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/text.html#propdef-letter-spacing">
     * letter-spacing property definition </a> in CSS2.
     *
     */
    DOM::DOMString letterSpacing() const;

    /**
     * see @ref letterSpacing
     */
    void setLetterSpacing( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/visudet.html#propdef-line-height">
     * line-height property definition </a> in CSS2.
     *
     */
    DOM::DOMString lineHeight() const;

    /**
     * see @ref lineHeight
     */
    void setLineHeight( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/generate.html#propdef-list-style">
     * list-style property definition </a> in CSS2.
     *
     */
    DOM::DOMString listStyle() const;

    /**
     * see @ref listStyle
     */
    void setListStyle( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/generate.html#propdef-list-style-image">
     * list-style-image property definition </a> in CSS2.
     *
     */
    DOM::DOMString listStyleImage() const;

    /**
     * see @ref listStyleImage
     */
    void setListStyleImage( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/generate.html#propdef-list-style-position">
     * list-style-position property definition </a> in CSS2.
     *
     */
    DOM::DOMString listStylePosition() const;

    /**
     * see @ref listStylePosition
     */
    void setListStylePosition( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/generate.html#propdef-list-style-type">
     * list-style-type property definition </a> in CSS2.
     *
     */
    DOM::DOMString listStyleType() const;

    /**
     * see @ref listStyleType
     */
    void setListStyleType( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-margin">
     * margin property definition </a> in CSS2.
     *
     */
    DOM::DOMString margin() const;

    /**
     * see @ref margin
     */
    void setMargin( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-margin-top">
     * margin-top property definition </a> in CSS2.
     *
     */
    DOM::DOMString marginTop() const;

    /**
     * see @ref marginTop
     */
    void setMarginTop( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-margin-right">
     * margin-right property definition </a> in CSS2.
     *
     */
    DOM::DOMString marginRight() const;

    /**
     * see @ref marginRight
     */
    void setMarginRight( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-margin-bottom">
     * margin-bottom property definition </a> in CSS2.
     *
     */
    DOM::DOMString marginBottom() const;

    /**
     * see @ref marginBottom
     */
    void setMarginBottom( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-margin-left">
     * margin-left property definition </a> in CSS2.
     *
     */
    DOM::DOMString marginLeft() const;

    /**
     * see @ref marginLeft
     */
    void setMarginLeft( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/generate.html#propdef-marker-offset">
     * marker-offset property definition </a> in CSS2.
     *
     */
    DOM::DOMString markerOffset() const;

    /**
     * see @ref markerOffset
     */
    void setMarkerOffset( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/page.html#propdef-marks">
     * marks property definition </a> in CSS2.
     *
     */
    DOM::DOMString marks() const;

    /**
     * see @ref marks
     */
    void setMarks( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/visudet.html#propdef-max-height">
     * max-height property definition </a> in CSS2.
     *
     */
    DOM::DOMString maxHeight() const;

    /**
     * see @ref maxHeight
     */
    void setMaxHeight( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/visudet.html#propdef-max-width">
     * max-width property definition </a> in CSS2.
     *
     */
    DOM::DOMString maxWidth() const;

    /**
     * see @ref maxWidth
     */
    void setMaxWidth( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/visudet.html#propdef-min-height">
     * min-height property definition </a> in CSS2.
     *
     */
    DOM::DOMString minHeight() const;

    /**
     * see @ref minHeight
     */
    void setMinHeight( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/visudet.html#propdef-min-width">
     * min-width property definition </a> in CSS2.
     *
     */
    DOM::DOMString minWidth() const;

    /**
     * see @ref minWidth
     */
    void setMinWidth( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/page.html#propdef-orphans">
     * orphans property definition </a> in CSS2.
     *
     */
    DOM::DOMString orphans() const;

    /**
     * see @ref orphans
     */
    void setOrphans( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/ui.html#propdef-outline">
     * outline property definition </a> in CSS2.
     *
     */
    DOM::DOMString outline() const;

    /**
     * see @ref outline
     */
    void setOutline( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/ui.html#propdef-outline-color">
     * outline-color property definition </a> in CSS2.
     *
     */
    DOM::DOMString outlineColor() const;

    /**
     * see @ref outlineColor
     */
    void setOutlineColor( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/ui.html#propdef-outline-style">
     * outline-style property definition </a> in CSS2.
     *
     */
    DOM::DOMString outlineStyle() const;

    /**
     * see @ref outlineStyle
     */
    void setOutlineStyle( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/ui.html#propdef-outline-width">
     * outline-width property definition </a> in CSS2.
     *
     */
    DOM::DOMString outlineWidth() const;

    /**
     * see @ref outlineWidth
     */
    void setOutlineWidth( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/visufx.html#propdef-overflow">
     * overflow property definition </a> in CSS2.
     *
     */
    DOM::DOMString overflow() const;

    /**
     * see @ref overflow
     */
    void setOverflow( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-padding">
     * padding property definition </a> in CSS2.
     *
     */
    DOM::DOMString padding() const;

    /**
     * see @ref padding
     */
    void setPadding( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-padding-top">
     * padding-top property definition </a> in CSS2.
     *
     */
    DOM::DOMString paddingTop() const;

    /**
     * see @ref paddingTop
     */
    void setPaddingTop( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-padding-right">
     * padding-right property definition </a> in CSS2.
     *
     */
    DOM::DOMString paddingRight() const;

    /**
     * see @ref paddingRight
     */
    void setPaddingRight( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-padding-bottom">
     * padding-bottom property definition </a> in CSS2.
     *
     */
    DOM::DOMString paddingBottom() const;

    /**
     * see @ref paddingBottom
     */
    void setPaddingBottom( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/box.html#propdef-padding-left">
     * padding-left property definition </a> in CSS2.
     *
     */
    DOM::DOMString paddingLeft() const;

    /**
     * see @ref paddingLeft
     */
    void setPaddingLeft( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/page.html#propdef-page">
     * page property definition </a> in CSS2.
     *
     */
    DOM::DOMString page() const;

    /**
     * see @ref page
     */
    void setPage( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/page.html#propdef-page-break-after">
     * page-break-after property definition </a> in CSS2.
     *
     */
    DOM::DOMString pageBreakAfter() const;

    /**
     * see @ref pageBreakAfter
     */
    void setPageBreakAfter( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/page.html#propdef-page-break-before">
     * page-break-before property definition </a> in CSS2.
     *
     */
    DOM::DOMString pageBreakBefore() const;

    /**
     * see @ref pageBreakBefore
     */
    void setPageBreakBefore( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/page.html#propdef-page-break-inside">
     * page-break-inside property definition </a> in CSS2.
     *
     */
    DOM::DOMString pageBreakInside() const;

    /**
     * see @ref pageBreakInside
     */
    void setPageBreakInside( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/aural.html#propdef-pause">
     * pause property definition </a> in CSS2.
     *
     */
    DOM::DOMString pause() const;

    /**
     * see @ref pause
     */
    void setPause( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/aural.html#propdef-pause-after">
     * pause-after property definition </a> in CSS2.
     *
     */
    DOM::DOMString pauseAfter() const;

    /**
     * see @ref pauseAfter
     */
    void setPauseAfter( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/aural.html#propdef-pause-before">
     * pause-before property definition </a> in CSS2.
     *
     */
    DOM::DOMString pauseBefore() const;

    /**
     * see @ref pauseBefore
     */
    void setPauseBefore( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/aural.html#propdef-pitch">
     * pitch property definition </a> in CSS2.
     *
     */
    DOM::DOMString pitch() const;

    /**
     * see @ref pitch
     */
    void setPitch( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/aural.html#propdef-pitch-range">
     * pitch-range property definition </a> in CSS2.
     *
     */
    DOM::DOMString pitchRange() const;

    /**
     * see @ref pitchRange
     */
    void setPitchRange( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/aural.html#propdef-play-during">
     * play-during property definition </a> in CSS2.
     *
     */
    DOM::DOMString playDuring() const;

    /**
     * see @ref playDuring
     */
    void setPlayDuring( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/visuren.html#propdef-position">
     * position property definition </a> in CSS2.
     *
     */
    DOM::DOMString position() const;

    /**
     * see @ref position
     */
    void setPosition( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/generate.html#propdef-quotes">
     * quotes property definition </a> in CSS2.
     *
     */
    DOM::DOMString quotes() const;

    /**
     * see @ref quotes
     */
    void setQuotes( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/aural.html#propdef-richness">
     * richness property definition </a> in CSS2.
     *
     */
    DOM::DOMString richness() const;

    /**
     * see @ref richness
     */
    void setRichness( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/visuren.html#propdef-right">
     * right property definition </a> in CSS2.
     *
     */
    DOM::DOMString right() const;

    /**
     * see @ref right
     */
    void setRight( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/page.html#propdef-size">
     * size property definition </a> in CSS2.
     *
     */
    DOM::DOMString size() const;

    /**
     * see @ref size
     */
    void setSize( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/aural.html#propdef-speak">
     * speak property definition </a> in CSS2.
     *
     */
    DOM::DOMString speak() const;

    /**
     * see @ref speak
     */
    void setSpeak( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/tables.html#propdef-speak-header">
     * speak-header property definition </a> in CSS2.
     *
     */
    DOM::DOMString speakHeader() const;

    /**
     * see @ref speakHeader
     */
    void setSpeakHeader( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/aural.html#propdef-speak-numeral">
     * speak-numeral property definition </a> in CSS2.
     *
     */
    DOM::DOMString speakNumeral() const;

    /**
     * see @ref speakNumeral
     */
    void setSpeakNumeral( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/aural.html#propdef-speak-punctuation">
     * speak-punctuation property definition </a> in CSS2.
     *
     */
    DOM::DOMString speakPunctuation() const;

    /**
     * see @ref speakPunctuation
     */
    void setSpeakPunctuation( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/aural.html#propdef-speech-rate">
     * speech-rate property definition </a> in CSS2.
     *
     */
    DOM::DOMString speechRate() const;

    /**
     * see @ref speechRate
     */
    void setSpeechRate( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/aural.html#propdef-stress">
     * stress property definition </a> in CSS2.
     *
     */
    DOM::DOMString stress() const;

    /**
     * see @ref stress
     */
    void setStress( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/tables.html#propdef-table-layout">
     * table-layout property definition </a> in CSS2.
     *
     */
    DOM::DOMString tableLayout() const;

    /**
     * see @ref tableLayout
     */
    void setTableLayout( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/text.html#propdef-text-align">
     * text-align property definition </a> in CSS2.
     *
     */
    DOM::DOMString textAlign() const;

    /**
     * see @ref textAlign
     */
    void setTextAlign( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/text.html#propdef-text-decoration">
     * text-decoration property definition </a> in CSS2.
     *
     */
    DOM::DOMString textDecoration() const;

    /**
     * see @ref textDecoration
     */
    void setTextDecoration( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/text.html#propdef-text-indent">
     * text-indent property definition </a> in CSS2.
     *
     */
    DOM::DOMString textIndent() const;

    /**
     * see @ref textIndent
     */
    void setTextIndent( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/text.html#propdef-text-shadow">
     * text-shadow property definition </a> in CSS2.
     *
     */
    DOM::DOMString textShadow() const;

    /**
     * see @ref textShadow
     */
    void setTextShadow( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/text.html#propdef-text-transform">
     * text-transform property definition </a> in CSS2.
     *
     */
    DOM::DOMString textTransform() const;

    /**
     * see @ref textTransform
     */
    void setTextTransform( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/visuren.html#propdef-top">
     * top property definition </a> in CSS2.
     *
     */
    DOM::DOMString top() const;

    /**
     * see @ref top
     */
    void setTop( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/visuren.html#propdef-unicode-bidi">
     * unicode-bidi property definition </a> in CSS2.
     *
     */
    DOM::DOMString unicodeBidi() const;

    /**
     * see @ref unicodeBidi
     */
    void setUnicodeBidi( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/visudet.html#propdef-vertical-align">
     * vertical-align property definition </a> in CSS2.
     *
     */
    DOM::DOMString verticalAlign() const;

    /**
     * see @ref verticalAlign
     */
    void setVerticalAlign( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/visufx.html#propdef-visibility">
     * visibility property definition </a> in CSS2.
     *
     */
    DOM::DOMString visibility() const;

    /**
     * see @ref visibility
     */
    void setVisibility( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/aural.html#propdef-voice-family">
     * voice-family property definition </a> in CSS2.
     *
     */
    DOM::DOMString voiceFamily() const;

    /**
     * see @ref voiceFamily
     */
    void setVoiceFamily( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/aural.html#propdef-volume">
     * volume property definition </a> in CSS2.
     *
     */
    DOM::DOMString volume() const;

    /**
     * see @ref volume
     */
    void setVolume( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/text.html#propdef-white-space">
     * white-space property definition </a> in CSS2.
     *
     */
    DOM::DOMString whiteSpace() const;

    /**
     * see @ref whiteSpace
     */
    void setWhiteSpace( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/page.html#propdef-widows">
     * widows property definition </a> in CSS2.
     *
     */
    DOM::DOMString widows() const;

    /**
     * see @ref widows
     */
    void setWidows( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/visudet.html#propdef-width">
     * width property definition </a> in CSS2.
     *
     */
    DOM::DOMString width() const;

    /**
     * see @ref width
     */
    void setWidth( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/text.html#propdef-word-spacing">
     * word-spacing property definition </a> in CSS2.
     *
     */
    DOM::DOMString wordSpacing() const;

    /**
     * see @ref wordSpacing
     */
    void setWordSpacing( const DOM::DOMString & );

    /**
     * See the <a
     * href="http://www.w3.org/TR/REC-CSS2/visufx.html#propdef-z-index">
     * z-index property definition </a> in CSS2.
     *
     */
    DOM::DOMString zIndex() const;

    /**
     * see @ref zIndex
     */
    void setZIndex( const DOM::DOMString & );
};


class CSS2TextShadowImpl;
class CSSValue;

/**
 * The <code> CSS2TextShadow </code> interface represents a simple
 * value for the <a
 * href="http://www.w3.org/TR/REC-CSS2/text.html#propdef-text-shadow">
 * text-shadow </a> CSS Level 2 property.
 *
 */
class CSS2TextShadow
{
public:
    CSS2TextShadow();
    CSS2TextShadow(const CSS2TextShadow &other);
    CSS2TextShadow(CSS2TextShadowImpl *impl);
public:

    CSS2TextShadow & operator = (const CSS2TextShadow &other);

    ~CSS2TextShadow();

    /**
     * Specified the color of the text shadow. The CSS Value can
     * contain an empty string if no color has been specified.
     *
     */
    CSSValue color() const;

    /**
     * The horizontal position of the text shadow. <code> 0 </code> if
     * no length has been specified.
     *
     */
    CSSValue horizontal() const;

    /**
     * The vertical position of the text shadow. <code> 0 </code> if
     * no length has been specified.
     *
     */
    CSSValue vertical() const;

    /**
     * The blur radius of the text shadow. <code> 0 </code> if no
     * length has been specified.
     *
     */
    CSSValue blur() const;
};


}; // namespace

#endif
