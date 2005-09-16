/*
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
 * Level 2 Specification (Style)
 * http://www.w3.org/TR/DOM-Level-2-Style/
 * Copyright © 2000 W3C® (MIT, INRIA, Keio), All Rights Reserved.
 *
 */
#ifndef _CSS_css_value_h_
#define _CSS_css_value_h_

#include <dom/dom_string.h>

#include <qcolor.h>

namespace DOM {

class CSSStyleDeclarationImpl;
class CSSRule;
class CSSValue;

#if !KHTML_NO_CPLUSPLUS_DOM

/**
 * The <code> CSSStyleDeclaration </code> interface represents a
 * single <a href="http://www.w3.org/TR/REC-CSS2/syndata.html#block">
 * CSS declaration block </a> . This interface may be used to
 * determine the style properties currently set in a block or to set
 * style properties explicitly within the block.
 *
 *  While an implementation may not recognize all CSS properties
 * within a CSS declaration block, it is expected to provide access to
 * all specified properties through the <code> CSSStyleDeclaration
 * </code> interface. Furthermore, implementations that support a
 * specific level of CSS should correctly handle <a
 * href="http://www.w3.org/TR/REC-CSS2/about.html#shorthand"> CSS
 * shorthand </a> properties for that level. For a further discussion
 * of shorthand properties, see the <code> CSS2Properties </code>
 * interface.
 *
 */
class CSSStyleDeclaration
{
public:
    CSSStyleDeclaration();
    CSSStyleDeclaration(const CSSStyleDeclaration &other);
    CSSStyleDeclaration(CSSStyleDeclarationImpl *impl);
public:

    CSSStyleDeclaration & operator = (const CSSStyleDeclaration &other);

    ~CSSStyleDeclaration();

    /**
     * The parsable textual representation of the declaration block
     * (including the surrounding curly braces). Setting this
     * attribute will result in the parsing of the new value and
     * resetting of the properties in the declaration block.
     *
     */
    DOMString cssText() const;

    /**
     * see @ref cssText
     * @exception CSSException
     * SYNTAX_ERR: Raised if the specified CSS string value has a
     * syntax error and is unparsable.
     *
     * @exception DOMException
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this declaration is
     * readonly.
     *
     */
    void setCssText( const DOMString & );

    /**
     * The number of properties that have been explicitly set in this
     * declaration block.
     *
     */
    unsigned length() const;

    /**
     * The CSS rule that contains this declaration block.
     *
     */
    CSSRule parentRule() const;

    /**
     * Used to retrieve the value of a CSS property if it has been
     * explicitly set within this declaration block.
     *
     * @param propertyName The name of the CSS property. See the <a
     * href="http://www.w3.org/TR/REC-CSS2/propidx.html"> CSS property
     * index </a> .
     *
     * @return Returns the value of the property if it has been
     * explicitly set for this declaration block. Returns the empty
     * string if the property has not been set.
     *
     */
    DOMString getPropertyValue ( const DOMString &propertyName );

    /**
     * Used to retrieve the object representation of the value of a
     * CSS property if it has been explicitly set within this
     * declaration block. This method returns null if the property is
     * a <a href="http://www.w3.org/TR/REC-CSS2/about.html#shorthand">
     * shorthand </a> property. Shorthand property values can only be
     * accessed and modified as strings, using the <code>
     * getPropertyValue </code> and <code> setProperty </code>
     * methods.
     *
     * @param propertyName The name of the CSS property. See the <a
     * href="http://www.w3.org/TR/REC-CSS2/propidx.html"> CSS property
     * index </a> .
     *
     * @return Returns the value of the property if it has been
     * explicitly set for this declaration block. Returns the <code>
     * null </code> if the property has not been set.
     *
     */
    CSSValue getPropertyCSSValue ( const DOMString &propertyName );

    /**
     * Used to remove a CSS property if it has been explicitly set
     * within this declaration block.
     *
     * @param propertyName The name of the CSS property. See the <a
     * href="http://www.w3.org/TR/REC-CSS2/propidx.html"> CSS property
     * index </a> .
     *
     * @return Returns the value of the property if it has been
     * explicitly set for this declaration block. Returns the empty
     * string if the property has not been set or the property name
     * does not correspond to a valid CSS2 property.
     *
     * @exception DOMException
     * NO_MODIFICATION_ALLOWED_ERR: Raised if this declaration is
     * readonly.
     *
     */
    DOMString removeProperty ( const DOMString &propertyName );

    /**
     * Used to retrieve the priority of a CSS property (e.g. the
     * <code> "important" </code> qualifier) if the property has been
     * explicitly set in this declaration block.
     *
     * @param propertyName The name of the CSS property. See the <a
     * href="http://www.w3.org/TR/REC-CSS2/propidx.html"> CSS property
     * index </a> .
     *
     * @return A string representing the priority (e.g. <code>
     * "important" </code> ) if one exists. The empty string if none
     * exists.
     *
     */
    DOMString getPropertyPriority ( const DOMString &propertyName );

    /**
     * Used to set a property value and priority within this
     * declaration block.
     *
     * @param propertyName The name of the CSS property. See the <a
     * href="http://www.w3.org/TR/REC-CSS2/propidx.html"> CSS property
     * index </a> .
     *
     * @param value The new value of the property.
     *
     * @param priority The new priority of the property (e.g. <code>
     * "important" </code> ).
     *
     * @return
     *
     * @exception CSSException
     * SYNTAX_ERR: Raised if the specified value has a syntax error
     * and is unparsable.
     *
     * @exception DOMException
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this declaration is
     * readonly.
     *
     */
    void setProperty ( const DOMString &propertyName, const DOMString &value, const DOMString &priority );

    /**
     * Used to retrieve the properties that have been explicitly set
     * in this declaration block. The order of the properties
     * retrieved using this method does not have to be the order in
     * which they were set. This method can be used to iterate over
     * all properties in this declaration block.
     *
     * @param index Index of the property name to retrieve.
     *
     * @return The name of the property at this ordinal position. The
     * empty string if no property exists at this position.
     *
     */
    DOMString item ( unsigned index );

    /**
     * @internal
     * not part of the DOM
     */
    CSSStyleDeclarationImpl *handle() const;
    bool isNull() const;

protected:
    CSSStyleDeclarationImpl *impl;
};


class CSSValueImpl;

#endif


/**
 * The <code> CSSValue </code> interface represents a simple or a
 * complexe value.
 *
 */
class CSSValue
{

public:

#if !KHTML_NO_CPLUSPLUS_DOM

    CSSValue();
    CSSValue(const CSSValue &other);
    CSSValue(CSSValueImpl *impl);
public:

    CSSValue & operator = (const CSSValue &other);

    ~CSSValue();

#endif

    /**
     * An integer indicating which type of unit applies to the value.
     *
     *  All CSS2 constants are not supposed to be required by the
     * implementation since all CSS2 interfaces are optionals.
     *
     */
    enum UnitTypes {
	CSS_INHERIT = 0,
        CSS_PRIMITIVE_VALUE = 1,
        CSS_VALUE_LIST = 2,
        CSS_CUSTOM = 3,
        CSS_INITIAL = 4
    };

#if !KHTML_NO_CPLUSPLUS_DOM

    /**
     * A string representation of the current value.
     *
     */
    DOMString cssText() const;

    /**
     * see @ref cssText
     * @exception CSSException
     * SYNTAX_ERR: Raised if the specified CSS string value has a
     * syntax error and is unparsable.
     *
     * @exception DOMException
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this declaration is
     * readonly.
     *
     */
    void setCssText( const DOMString & );

    /**
     * A code defining the type of the value as defined above.
     *
     */
    unsigned short cssValueType() const;

    /**
     * @internal
     * not part of the DOM
     */
    bool isCSSValueList() const;
    bool isCSSPrimitiveValue() const;
    CSSValueImpl *handle() const;
    bool isNull() const;

protected:
    CSSValueImpl *impl;

#endif

};


#if !KHTML_NO_CPLUSPLUS_DOM

class CSSValueListImpl;
class CSSValue;

/**
 * The <code> CSSValueList </code> interface provides the absraction
 * of an ordered collection of CSS values.
 *
 */
class CSSValueList : public CSSValue
{
public:
    CSSValueList();
    CSSValueList(const CSSValueList &other);
    CSSValueList(const CSSValue &other);
    CSSValueList(CSSValueListImpl *impl);
public:

    CSSValueList & operator = (const CSSValueList &other);
    CSSValueList & operator = (const CSSValue &other);

    ~CSSValueList();

    /**
     * The number of <code> CSSValue </code> s in the list. The range
     * of valid values indices is <code> 0 </code> to <code> length-1
     * </code> inclusive.
     *
     */
    unsigned length() const;

    /**
     * Used to retrieve a CSS rule by ordinal index. The order in this
     * collection represents the order of the values in the CSS style
     * property.
     *
     * @param index Index into the collection.
     *
     * @return The style rule at the <code> index </code> position in
     * the <code> CSSValueList </code> , or <code> null </code> if
     * that is not valid index.
     *
     */
    CSSValue item ( unsigned index );

protected:
    CSSValueListImpl *vimpl;
};


class CSSPrimitiveValueImpl;
class Counter;
class RGBColor;
class Rect;

#endif

/**
 * The <code> CSSPrimitiveValue </code> interface represents a single
 * <a href="http://www.w3.org/TR/REC-CSS2/syndata.html#values"> CSS
 * value </a> . This interface may be used to determine the value of a
 * specific style property currently set in a block or to set a
 * specific style properties explicitly within the block. An instance
 * of this interface can be obtained from the <code>
 * getPropertyCSSValue </code> method of the <code>
 * CSSStyleDeclaration </code> interface.
 *
 */
class CSSPrimitiveValue : public CSSValue
{

public:

#if !KHTML_NO_CPLUSPLUS_DOM

    CSSPrimitiveValue();
    CSSPrimitiveValue(const CSSPrimitiveValue &other);
    CSSPrimitiveValue(const CSSValue &other);
    CSSPrimitiveValue(CSSPrimitiveValueImpl *impl);
public:

    CSSPrimitiveValue & operator = (const CSSPrimitiveValue &other);
    CSSPrimitiveValue & operator = (const CSSValue &other);

    ~CSSPrimitiveValue();

#endif

    /**
     * An integer indicating which type of unit applies to the value.
     *
     */
    enum UnitTypes {
        CSS_UNKNOWN = 0,
        CSS_NUMBER = 1,
        CSS_PERCENTAGE = 2,
        CSS_EMS = 3,
        CSS_EXS = 4,
        CSS_PX = 5,
        CSS_CM = 6,
        CSS_MM = 7,
        CSS_IN = 8,
        CSS_PT = 9,
        CSS_PC = 10,
        CSS_DEG = 11,
        CSS_RAD = 12,
        CSS_GRAD = 13,
        CSS_MS = 14,
        CSS_S = 15,
        CSS_HZ = 16,
        CSS_KHZ = 17,
        CSS_DIMENSION = 18,
        CSS_STRING = 19,
        CSS_URI = 20,
        CSS_IDENT = 21,
        CSS_ATTR = 22,
        CSS_COUNTER = 23,
        CSS_RECT = 24,
        CSS_RGBCOLOR = 25,
        CSS_PAIR = 26, // We envision this being exposed as a means of getting computed style values for pairs (border-spacing/radius, background-position, etc.)
        CSS_DASHBOARD_REGION = 27, // FIXME: What on earth is this doing as a primitive value? This is insane.
        CSS_HTML_RELATIVE = 255
    };

#if !KHTML_NO_CPLUSPLUS_DOM

    /**
     * The type of the value as defined by the constants specified
     * above.
     *
     */
    unsigned short primitiveType() const;

    /**
     * A method to set the float value with a specified unit. If the
     * property attached with this value can not accept the specified
     * unit or the float value, the value will be unchanged and a
     * <code> DOMException </code> will be raised.
     *
     * @param unitType A unit code as defined above. The unit code can
     * only be a float unit type (e.g. <code> NUMBER </code> , <code>
     * PERCENTAGE </code> , <code> CSS_EMS </code> , <code> CSS_EXS
     * </code> , <code> CSS_PX </code> , <code> CSS_PX </code> ,
     * <code> CSS_CM </code> , <code> CSS_MM </code> , <code> CSS_IN
     * </code> , <code> CSS_PT </code> , <code> CSS_PC </code> ,
     * <code> CSS_DEG </code> , <code> CSS_RAD </code> , <code>
     * CSS_GRAD </code> , <code> CSS_MS </code> , <code> CSS_S </code>
     * , <code> CSS_HZ </code> , <code> CSS_KHZ </code> , <code>
     * CSS_DIMENSION </code> ).
     *
     * @param floatValue The new float value.
     *
     * @return
     * @exception DOMException
     * INVALID_ACCESS_ERR: Raises if the attached property doesn't
     * support the float value or the unit type.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this property is
     * readonly.
     *
     */
    void setFloatValue ( unsigned short unitType, float floatValue );

    /**
     * This method is used to get a float value in a specified unit.
     * If this CSS value doesn't contain a float value or can't be
     * converted into the specified unit, a <code> DOMException
     * </code> is raised.
     *
     * @param unitType A unit code to get the float value. The unit
     * code can only be a float unit type (e.g. <code> CSS_NUMBER
     * </code> , <code> CSS_PERCENTAGE </code> , <code> CSS_EMS
     * </code> , <code> CSS_EXS </code> , <code> CSS_PX </code> ,
     * <code> CSS_PX </code> , <code> CSS_CM </code> , <code> CSS_MM
     * </code> , <code> CSS_IN </code> , <code> CSS_PT </code> ,
     * <code> CSS_PC </code> , <code> CSS_DEG </code> , <code> CSS_RAD
     * </code> , <code> CSS_GRAD </code> , <code> CSS_MS </code> ,
     * <code> CSS_S </code> , <code> CSS_HZ </code> , <code> CSS_KHZ
     * </code> , <code> CSS_DIMENSION </code> ).
     *
     * @return The float value in the specified unit.
     *
     * @exception DOMException
     * INVALID_ACCESS_ERR: Raises if the CSS value doesn't contain a
     * float value or if the float value can't be converted into the
     * specified unit.
     *
     */
    float getFloatValue ( unsigned short unitType );

    /**
     * A method to set the string value with a specified unit. If the
     * property attached to this value can't accept the specified unit
     * or the string value, the value will be unchanged and a <code>
     * DOMException </code> will be raised.
     *
     * @param stringType A string code as defined above. The string
     * code can only be a string unit type (e.g. <code> CSS_URI
     * </code> , <code> CSS_IDENT </code> , <code> CSS_INHERIT </code>
     * and <code> CSS_ATTR </code> ).
     *
     * @param stringValue The new string value. If the <code>
     * stringType </code> is equal to <code> CSS_INHERIT </code> , the
     * <code> stringValue </code> should be <code> inherit </code> .
     *
     * @return
     * @exception DOMException
     * INVALID_ACCESS_ERR: Raises if the CSS value doesn't contain a
     * string value or if the string value can't be converted into the
     * specified unit.
     *
     *  NO_MODIFICATION_ALLOWED_ERR: Raised if this property is
     * readonly.
     *
     */
    void setStringValue ( unsigned short stringType, const DOMString &stringValue );

    /**
     * This method is used to get the string value in a specified
     * unit. If the CSS value doesn't contain a string value, a <code>
     * DOMException </code> is raised.
     *
     * @return The string value in the current unit. The current
     * <code> valueType </code> can only be a string unit type (e.g.
     * <code> CSS_URI </code> , <code> CSS_IDENT </code> and <code>
     * CSS_ATTR </code> ).
     *
     * @exception DOMException
     * INVALID_ACCESS_ERR: Raises if the CSS value doesn't contain a
     * string value.
     *
     */
    DOMString getStringValue (  );

    /**
     * This method is used to get the Counter value. If this CSS value
     * doesn't contain a counter value, a <code> DOMException </code>
     * is raised. Modification to the corresponding style property can
     * be achieved using the <code> Counter </code> interface.
     *
     * @return The Counter value.
     *
     * @exception DOMException
     * INVALID_ACCESS_ERR: Raises if the CSS value doesn't contain a
     * Counter value.
     *
     */
    Counter getCounterValue (  );

    /**
     * This method is used to get the Rect value. If this CSS value
     * doesn't contain a rect value, a <code> DOMException </code> is
     * raised. Modification to the corresponding style property can be
     * achieved using the <code> Rect </code> interface.
     *
     * @return The Rect value.
     *
     * @exception DOMException
     * INVALID_ACCESS_ERR: Raises if the CSS value doesn't contain a
     * Rect value.
     *
     */
    Rect getRectValue (  );

    /**
     * This method is used to get the RGB color. If this CSS value
     * doesn't contain a RGB color value, a <code> DOMException
     * </code> is raised. Modification to the corresponding style
     * property can be achieved using the <code> RGBColor </code>
     * interface.
     *
     * @return the RGB color value.
     *
     * @exception DOMException
     * INVALID_ACCESS_ERR: Raises if the attached property can't
     * return a RGB color value.
     *
     */
    RGBColor getRGBColorValue (  );

#endif

};


#if !KHTML_NO_CPLUSPLUS_DOM

/**
 * The <code> RGBColor </code> interface is used to represent any <a
 * href="http://www.w3.org/TR/REC-CSS2/syndata.html#value-def-color">
 * RGB color </a> value. This interface reflects the values in the
 * underlying style property. Hence, modifications made through this
 * interface modify the style property.
 *
 */
class RGBColor
{
public:
    RGBColor();
    RGBColor(const RGBColor &other);
    RGBColor(const QColor &color);

    RGBColor & operator = (const RGBColor &other);

    ~RGBColor();

    /**
     * This attribute is used for the red value of the RGB color.
     *
     */
    CSSPrimitiveValue red() const;

    /**
     * This attribute is used for the green value of the RGB color.
     *
     */
    CSSPrimitiveValue green() const;

    /**
     * This attribute is used for the blue value of the RGB color.
     *
     */
    CSSPrimitiveValue blue() const;

    const QColor &color() const { return m_color; }

protected:
    QColor m_color;
};

class RectImpl;

/**
 * The <code> Rect </code> interface is used to represent any <a
 * href="http://www.w3.org/TR/REC-CSS2/visufx.html#value-def-shape">
 * rect </a> value. This interface reflects the values in the
 * underlying style property. Hence, modifications made through this
 * interface modify the style property.
 *
 */
class Rect
{
    friend class CSSPrimitiveValue;
public:
    Rect();
    Rect(const Rect &other);

    Rect & operator = (const Rect &other);

    ~Rect();

    /**
     * This attribute is used for the top of the rect.
     *
     */
    CSSPrimitiveValue top() const;

    /**
     * This attribute is used for the right of the rect.
     *
     */
    CSSPrimitiveValue right() const;

    /**
     * This attribute is used for the bottom of the rect.
     *
     */
    CSSPrimitiveValue bottom() const;

    /**
     * This attribute is used for the left of the rect.
     *
     */
    CSSPrimitiveValue left() const;

    /**
     * @internal
     * not part of the DOM
     */
    RectImpl *handle() const;
    bool isNull() const;

protected:
    RectImpl *impl;
    Rect(RectImpl *i);
};

class CounterImpl;

/**
 * The <code> Counter </code> interface is used to represent any <a
 * href="http://www.w3.org/TR/REC-CSS2/syndata.html#value-def-counter">
 * counter or counters function </a> value. This interface reflects
 * the values in the underlying style property. Hence, modifications
 * made through this interface modify the style property.
 *
 */
class Counter
{
    friend class CSSPrimitiveValue;
public:
    Counter();
    Counter(const Counter &other);
public:

    Counter & operator = (const Counter &other);

    ~Counter();

    /**
     * This attribute is used for the identifier of the counter.
     *
     */
    DOMString identifier() const;

    /**
     * This attribute is used for the style of the list.
     *
     */
    DOMString listStyle() const;

    /**
     * This attribute is used for the separator of nested counters.
     *
     */
    DOMString separator() const;

    /**
     * @internal
     * not part of the DOM
     */
    CounterImpl *handle() const;
    bool isNull() const;

protected:
    CounterImpl *impl;
    Counter(CounterImpl *i);
};

#endif

} // namespace

#endif
