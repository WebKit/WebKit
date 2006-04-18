/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 */

#ifndef CSS_css_valueimpl_h_
#define CSS_css_valueimpl_h_

#include "CachedObjectClient.h"
#include "css_base.h"
#include "DeprecatedPtrList.h"
#include "DeprecatedValueList.h"
#include <kxmlcore/PassRefPtr.h>

class QPaintDeviceMetrics;

namespace WebCore {

class CSSMutableStyleDeclaration;
class CachedImage;
class Counter;
class DocLoader;
class Node;
class Pair;
class RectImpl;
class RenderStyle;

#if __APPLE__
class DashboardRegion;
#endif

typedef int ExceptionCode;

extern const int inheritableProperties[];
extern const unsigned numInheritableProperties;

class CSSStyleDeclaration : public StyleBase {
public:
    virtual bool isStyleDeclaration();

    static bool isPropertyName(const String& propertyName);

    CSSRule* parentRule() const;

    virtual String cssText() const = 0;
    virtual void setCssText(const String&, ExceptionCode&) = 0;

    virtual unsigned length() const = 0;
    virtual String item(unsigned index) const = 0;

    PassRefPtr<CSSValue> getPropertyCSSValue(const String& propertyName);
    String getPropertyValue(const String& propertyName);
    String getPropertyPriority(const String& propertyName);
    String getPropertyShorthand(const String& propertyName);
    bool isPropertyImplicit(const String& propertyName);
    
    virtual PassRefPtr<CSSValue> getPropertyCSSValue(int propertyID) const = 0;
    virtual String getPropertyValue(int propertyID) const = 0;
    virtual bool getPropertyPriority(int propertyID) const = 0;
    virtual int getPropertyShorthand(int propertyID) const = 0;
    virtual bool isPropertyImplicit(int propertyID) const = 0;

    void setProperty(const String& propertyName, const String& value, const String& priority, ExceptionCode&);
    String removeProperty(const String& propertyName, ExceptionCode&);
    virtual void setProperty(int propertyId, const String& value, bool important, ExceptionCode&) = 0;
    virtual String removeProperty(int propertyID, ExceptionCode&) = 0;

    virtual PassRefPtr<CSSMutableStyleDeclaration> copy() const = 0;
    virtual PassRefPtr<CSSMutableStyleDeclaration> makeMutable() = 0;
 
    void diff(CSSMutableStyleDeclaration*) const;

    PassRefPtr<CSSMutableStyleDeclaration> copyPropertiesInSet(const int* set, unsigned length) const;

protected:
    CSSStyleDeclaration(CSSRule* parentRule = 0);

private:
    CSSStyleDeclaration(const CSSStyleDeclaration &);
    CSSStyleDeclaration& operator=(const CSSStyleDeclaration &);
};

class CSSValue : public StyleBase
{
public:
    enum UnitTypes {
        CSS_INHERIT = 0,
        CSS_PRIMITIVE_VALUE = 1,
        CSS_VALUE_LIST = 2,
        CSS_CUSTOM = 3,
        CSS_INITIAL = 4
    };

    CSSValue() : StyleBase(0) { }

    virtual unsigned short cssValueType() const { return CSS_CUSTOM; }
    virtual String cssText() const = 0;
    void setCssText(const String&) { } // FIXME: Not implemented.

    virtual bool isValue() { return true; }
    virtual bool isFontValue() { return false; }
};

class CSSInheritedValue : public CSSValue
{
public:
    virtual unsigned short cssValueType() const;
    virtual String cssText() const;
};

class CSSInitialValue : public CSSValue
{
public:
    virtual unsigned short cssValueType() const;
    virtual String cssText() const;
};

class CSSValueList : public CSSValue
{
public:
    virtual ~CSSValueList();

    unsigned length() const { return m_values.count(); }
    CSSValue* item (unsigned index) { return m_values.at(index); }

    virtual bool isValueList() { return true; }

    virtual unsigned short cssValueType() const;

    void append(PassRefPtr<CSSValue>);
    virtual String cssText() const;

protected:
    DeprecatedPtrList<CSSValue> m_values;
};


class CSSPrimitiveValue : public CSSValue
{
public:
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
        CSS_PAIR = 100, // We envision this being exposed as a means of getting computed style values for pairs (border-spacing/radius, background-position, etc.)
        CSS_DASHBOARD_REGION = 101 // FIXME: What on earth is this doing as a primitive value? It should not be!
    };

    // FIXME: int vs. unsigned overloading is too tricky for color vs. ident
    CSSPrimitiveValue();
    CSSPrimitiveValue(int ident);
    CSSPrimitiveValue(double, UnitTypes);
    CSSPrimitiveValue(const String&, UnitTypes);
    CSSPrimitiveValue(PassRefPtr<Counter>);
    CSSPrimitiveValue(PassRefPtr<RectImpl>);
    CSSPrimitiveValue(unsigned color); // RGB value
    CSSPrimitiveValue(PassRefPtr<Pair>);

#if __APPLE__
    CSSPrimitiveValue(PassRefPtr<DashboardRegion>); // FIXME: Why is dashboard region a primitive value? This makes no sense.
#endif

    virtual ~CSSPrimitiveValue();

    void cleanup();

    unsigned short primitiveType() const { return m_type; }

    /*
     * computes a length in pixels out of the given CSSValue. Need the RenderStyle to get
     * the fontinfo in case val is defined in em or ex.
     *
     * The metrics have to be a bit different for screen and printer output.
     * For screen output we assume 1 inch == 72 px, for printer we assume 300 dpi
     *
     * this is screen/printer dependent, so we probably need a config option for this,
     * and some tool to calibrate.
     */
    int computeLengthInt(RenderStyle*);
    int computeLengthInt(RenderStyle*, double multiplier);
    int computeLengthIntForLength(RenderStyle*);
    int computeLengthIntForLength(RenderStyle*, double multiplier);
    short computeLengthShort(RenderStyle*);
    short computeLengthShort(RenderStyle*, double multiplier);
    double computeLengthFloat(RenderStyle*, bool applyZoomFactor = true);

    // use with care!!!
    void setPrimitiveType(unsigned short type) { m_type = type; }
    void setFloatValue(unsigned short unitType, double floatValue, ExceptionCode&);
    double getFloatValue(unsigned short /* unitType */) const { return m_value.num; }

    void setStringValue(unsigned short stringType, const String& stringValue, ExceptionCode&);
    String getStringValue() const;
    
    Counter* getCounterValue () const {
        return m_type != CSS_COUNTER ? 0 : m_value.counter;
    }

    RectImpl* getRectValue () const {
        return m_type != CSS_RECT ? 0 : m_value.rect;
    }

    unsigned getRGBColorValue() const {
        return m_type != CSS_RGBCOLOR ? 0 : m_value.rgbcolor;
    }

    Pair* getPairValue() const {
        return m_type != CSS_PAIR ? 0 : m_value.pair;
    }

#if __APPLE__
    DashboardRegion *getDashboardRegionValue () const {
        return m_type != CSS_DASHBOARD_REGION ? 0 : m_value.region;
    }
#endif

    virtual bool isPrimitiveValue() const { return true; }
    virtual unsigned short cssValueType() const;

    int getIdent();

    virtual bool parseString(const String&, bool = false);
    virtual String cssText() const;

    virtual bool isQuirkValue() { return false; }

protected:
    int m_type;
    union {
        int ident;
        double num;
        StringImpl* string;
        Counter* counter;
        RectImpl* rect;
        unsigned rgbcolor;
        Pair* pair;
#if __APPLE__
        DashboardRegion* region;
#endif
    } m_value;
};

// FIXME: Remove when we rename everything to remove Impl.
typedef CSSPrimitiveValue CSSPrimitiveValue;

// This value is used to handle quirky margins in reflow roots (body, td, and th) like WinIE.
// The basic idea is that a stylesheet can use the value __qem (for quirky em) instead of em
// in a stylesheet.  When the quirky value is used, if you're in quirks mode, the margin will
// collapse away inside a table cell.
class CSSQuirkPrimitiveValue : public CSSPrimitiveValue
{
public:
    CSSQuirkPrimitiveValue(double num, UnitTypes type)
        : CSSPrimitiveValue(num, type) {}

    virtual bool isQuirkValue() { return true; }
};

class Counter : public Shared<Counter> {
public:
    String identifier() const { return m_identifier; }
    String listStyle() const { return m_listStyle; }
    String separator() const { return m_separator; }

    String m_identifier;
    String m_listStyle;
    String m_separator;
};

class RectImpl : public Shared<RectImpl> {
public:
    virtual ~RectImpl();

    CSSPrimitiveValue* top() const { return m_top.get(); }
    CSSPrimitiveValue* right() const { return m_right.get(); }
    CSSPrimitiveValue* bottom() const { return m_bottom.get(); }
    CSSPrimitiveValue* left() const { return m_left.get(); }

    void setTop(PassRefPtr<CSSPrimitiveValue> top) { m_top = top; }
    void setRight(PassRefPtr<CSSPrimitiveValue> right) { m_right = right; }
    void setBottom(PassRefPtr<CSSPrimitiveValue> bottom) { m_bottom = bottom; }
    void setLeft(PassRefPtr<CSSPrimitiveValue> left) { m_left = left; }

protected:
    RefPtr<CSSPrimitiveValue> m_top;
    RefPtr<CSSPrimitiveValue> m_right;
    RefPtr<CSSPrimitiveValue> m_bottom;
    RefPtr<CSSPrimitiveValue> m_left;
};

// A primitive value representing a pair.  This is useful for properties like border-radius, background-size/position,
// and border-spacing (all of which are space-separated sets of two values).  At the moment we are only using it for
// border-radius and background-size, but (FIXME) border-spacing and background-position could be converted over to use
// it (eliminating some extra -webkit- internal properties).
class Pair : public Shared<Pair> {
public:
    Pair() : m_first(0), m_second(0) { }
    Pair(PassRefPtr<CSSPrimitiveValue> first, PassRefPtr<CSSPrimitiveValue> second)
        : m_first(first), m_second(second) { }
    virtual ~Pair();

    CSSPrimitiveValue* first() const { return m_first.get(); }
    CSSPrimitiveValue* second() const { return m_second.get(); }

    void setFirst(PassRefPtr<CSSPrimitiveValue> first) { m_first = first; }
    void setSecond(PassRefPtr<CSSPrimitiveValue> second) { m_second = second; }

protected:
    RefPtr<CSSPrimitiveValue> m_first;
    RefPtr<CSSPrimitiveValue> m_second;
};

class DashboardRegion : public RectImpl {
public:
    DashboardRegion() : m_isCircle(0), m_isRectangle(0) { }

    RefPtr<DashboardRegion> m_next;
    String m_label;
    String m_geometryType;
    bool m_isCircle : 1;
    bool m_isRectangle : 1;
};

class CSSImageValue : public CSSPrimitiveValue, public CachedObjectClient
{
public:
    CSSImageValue();
    CSSImageValue(const String& url, StyleBase*);
    virtual ~CSSImageValue();

    CachedImage* image(DocLoader*);

protected:
    CachedImage* m_image;
    bool m_accessedImage;
};

class CSSBorderImageValue : public CSSValue
{
public:
    CSSBorderImageValue();
    CSSBorderImageValue(PassRefPtr<CSSImageValue>,
        PassRefPtr<RectImpl>, int horizontalRule, int verticalRule);

    virtual String cssText() const;

public:
    // The border image.
    RefPtr<CSSImageValue> m_image;

    // These four values are used to make "cuts" in the image.  They can be numbers
    // or percentages.
    RefPtr<RectImpl> m_imageSliceRect;
    
    // Values for how to handle the scaling/stretching/tiling of the image slices.
    int m_horizontalSizeRule; // Rule for how to adjust the widths of the top/middle/bottom
    int m_verticalSizeRule; // Rule for how to adjust the heights of the left/middle/right
};

class FontFamilyValue : public CSSPrimitiveValue
{
public:
    FontFamilyValue( const DeprecatedString &string);
    const DeprecatedString& fontName() const { return parsedFontName; }
    int genericFamilyType() const { return _genericFamilyType; }

    virtual String cssText() const;

    DeprecatedString parsedFontName;
private:
    int _genericFamilyType;
};

class FontValue : public CSSValue
{
public:
    virtual String cssText() const;
    
    virtual bool isFontValue() { return true; }

    RefPtr<CSSPrimitiveValue> style;
    RefPtr<CSSPrimitiveValue> variant;
    RefPtr<CSSPrimitiveValue> weight;
    RefPtr<CSSPrimitiveValue> size;
    RefPtr<CSSPrimitiveValue> lineHeight;
    RefPtr<CSSValueList> family;
};

// Used for text-shadow and box-shadow
class ShadowValue : public CSSValue
{
public:
    ShadowValue(PassRefPtr<CSSPrimitiveValue> x,
        PassRefPtr<CSSPrimitiveValue> y,
        PassRefPtr<CSSPrimitiveValue> blur,
        PassRefPtr<CSSPrimitiveValue> color);
    
    virtual String cssText() const;

    RefPtr<CSSPrimitiveValue> x;
    RefPtr<CSSPrimitiveValue> y;
    RefPtr<CSSPrimitiveValue> blur;
    RefPtr<CSSPrimitiveValue> color;
};

// ------------------------------------------------------------------------------

// another helper class
class CSSProperty
{
public:
    CSSProperty(int propID, PassRefPtr<CSSValue> value, bool important = false, int shorthandID = 0, bool implicit = false)
        : m_id(propID), m_shorthandID(shorthandID), m_important(important), m_implicit(implicit), m_value(value)
    {
    }
    
    CSSProperty &operator=(const CSSProperty& o)
    {
        m_id = o.m_id;
        m_shorthandID = o.m_shorthandID;
        m_important = o.m_important;
        m_value = o.m_value;
        return *this;
    }
    
    int id() const { return m_id; }
    int shorthandID() const { return m_shorthandID; }
    
    bool isImportant() const { return m_important; }
    bool isImplicit() const { return m_implicit; }

    CSSValue* value() const { return m_value.get(); }
    
    String cssText() const;

    // make sure the following fits in 4 bytes.
    int m_id;
    int m_shorthandID;  // If this property was set as part of a shorthand, gives the shorthand.
    bool m_important : 1;
    bool m_implicit : 1; // Whether or not the property was set implicitly as the result of a shorthand.

    friend bool operator==(const CSSProperty &, const CSSProperty &);

    RefPtr<CSSValue> m_value;
};

class CSSMutableStyleDeclaration : public CSSStyleDeclaration
{
public:
    CSSMutableStyleDeclaration();
    CSSMutableStyleDeclaration(CSSRule* parentRule);
    CSSMutableStyleDeclaration(CSSRule* parentRule, const DeprecatedValueList<CSSProperty> &);
    CSSMutableStyleDeclaration(CSSRule* parentRule, const CSSProperty * const *, int numProperties);

    CSSMutableStyleDeclaration &operator=(const CSSMutableStyleDeclaration &);

    void setNode(Node* node) { m_node = node; }

    virtual String cssText() const;
    virtual void setCssText(const String&, ExceptionCode&);

    virtual unsigned length() const;
    virtual String item(unsigned index) const;

    virtual PassRefPtr<CSSValue> getPropertyCSSValue(int propertyID) const;
    virtual String getPropertyValue(int propertyID) const;
    virtual bool getPropertyPriority(int propertyID) const;
    virtual int getPropertyShorthand(int propertyID) const;
    virtual bool isPropertyImplicit(int propertyID) const;

    virtual void setProperty(int propertyId, const String& value, bool important, ExceptionCode&);
    virtual String removeProperty(int propertyID, ExceptionCode&);

    virtual PassRefPtr<CSSMutableStyleDeclaration> copy() const;
    virtual PassRefPtr<CSSMutableStyleDeclaration> makeMutable();

    DeprecatedValueListConstIterator<CSSProperty> valuesIterator() const { return m_values.begin(); }

    bool setProperty(int propertyID, int value, bool important = false, bool notifyChanged = true);
    bool setProperty(int propertyID, const String& value, bool important, bool notifyChanged, ExceptionCode&);
    bool setProperty(int propertyId, const String& value, bool important = false, bool notifyChanged = true)
        { ExceptionCode ec; return setProperty(propertyId, value, important, notifyChanged, ec); }

    String removeProperty(int propertyID, bool notifyChanged, ExceptionCode&);
    String removeProperty(int propertyID, bool notifyChanged = true)
        { ExceptionCode ec; return removeProperty(propertyID, notifyChanged, ec); }

    void clear();

    void setChanged();
 
    // setLengthProperty treats integers as pixels! (Needed for conversion of HTML attributes.)
    void setLengthProperty(int propertyId, const String& value, bool important, bool multiLength = false);
    void setStringProperty(int propertyId, const String& value, CSSPrimitiveValue::UnitTypes, bool important = false); // parsed string value
    void setImageProperty(int propertyId, const String& URL, bool important = false);
 
    // The following parses an entire new style declaration.
    void parseDeclaration(const String& styleDeclaration);

    // Besides adding the properties, this also removes any existing properties with these IDs.
    // It does no notification since it's called by the parser.
    void addParsedProperties(const CSSProperty * const *, int numProperties);
 
    PassRefPtr<CSSMutableStyleDeclaration> copyBlockProperties() const;
    void removeBlockProperties();
    void removeInheritableProperties();
    void removePropertiesInSet(const int* set, unsigned length);

    void merge(CSSMutableStyleDeclaration*, bool argOverridesOnConflict = true);
 
private:
    String getShortHandValue(const int* properties, int number) const;
    String get4Values(const int* properties) const;
 
    DeprecatedValueList<CSSProperty> m_values;
    Node* m_node;
};

} // namespace

#endif
