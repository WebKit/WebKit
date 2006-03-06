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
#include <qptrlist.h>
#include <qvaluelist.h>
#include <kxmlcore/PassRefPtr.h>

class QPaintDeviceMetrics;

namespace WebCore {

class CSSMutableStyleDeclarationImpl;
class CachedImage;
class CounterImpl;
class DocLoader;
class NodeImpl;
class PairImpl;
class RectImpl;
class RenderStyle;

#if __APPLE__
class DashboardRegionImpl;
#endif

typedef int ExceptionCode;

extern const int inheritableProperties[];
extern const unsigned numInheritableProperties;

class CSSStyleDeclarationImpl : public StyleBaseImpl {
public:
    virtual bool isStyleDeclaration();

    static bool isPropertyName(const String& propertyName);

    CSSRuleImpl* parentRule() const;

    virtual String cssText() const = 0;
    virtual void setCssText(const String&, ExceptionCode&) = 0;

    virtual unsigned length() const = 0;
    virtual String item(unsigned index) const = 0;

    PassRefPtr<CSSValueImpl> getPropertyCSSValue(const String& propertyName);
    String getPropertyValue(const String& propertyName);
    String getPropertyPriority(const String& propertyName);
    String getPropertyShorthand(const String& propertyName);
    bool isPropertyImplicit(const String& propertyName);
    
    virtual PassRefPtr<CSSValueImpl> getPropertyCSSValue(int propertyID) const = 0;
    virtual String getPropertyValue(int propertyID) const = 0;
    virtual bool getPropertyPriority(int propertyID) const = 0;
    virtual int getPropertyShorthand(int propertyID) const = 0;
    virtual bool isPropertyImplicit(int propertyID) const = 0;

    void setProperty(const String& propertyName, const String& value, const String& priority, ExceptionCode&);
    String removeProperty(const String& propertyName, ExceptionCode&);
    virtual void setProperty(int propertyId, const String& value, bool important, ExceptionCode&) = 0;
    virtual String removeProperty(int propertyID, ExceptionCode&) = 0;

    virtual PassRefPtr<CSSMutableStyleDeclarationImpl> copy() const = 0;
    virtual PassRefPtr<CSSMutableStyleDeclarationImpl> makeMutable() = 0;
 
    void diff(CSSMutableStyleDeclarationImpl*) const;

    PassRefPtr<CSSMutableStyleDeclarationImpl> copyPropertiesInSet(const int* set, unsigned length) const;

protected:
    CSSStyleDeclarationImpl(CSSRuleImpl* parentRule = 0);

private:
    CSSStyleDeclarationImpl(const CSSStyleDeclarationImpl &);
    CSSStyleDeclarationImpl& operator=(const CSSStyleDeclarationImpl &);
};

class CSSValueImpl : public StyleBaseImpl
{
public:
    enum UnitTypes {
        CSS_INHERIT = 0,
        CSS_PRIMITIVE_VALUE = 1,
        CSS_VALUE_LIST = 2,
        CSS_CUSTOM = 3,
        CSS_INITIAL = 4
    };

    CSSValueImpl() : StyleBaseImpl(0) { }

    virtual unsigned short cssValueType() const { return CSS_CUSTOM; }
    virtual String cssText() const = 0;
    void setCssText(const String&) { } // FIXME: Not implemented.

    virtual bool isValue() { return true; }
    virtual bool isFontValue() { return false; }
};

class CSSInheritedValueImpl : public CSSValueImpl
{
public:
    virtual unsigned short cssValueType() const;
    virtual String cssText() const;
};

class CSSInitialValueImpl : public CSSValueImpl
{
public:
    virtual unsigned short cssValueType() const;
    virtual String cssText() const;
};

class CSSValueListImpl : public CSSValueImpl
{
public:
    virtual ~CSSValueListImpl();

    unsigned length() const { return m_values.count(); }
    CSSValueImpl* item (unsigned index) { return m_values.at(index); }

    virtual bool isValueList() { return true; }

    virtual unsigned short cssValueType() const;

    void append(PassRefPtr<CSSValueImpl>);
    virtual String cssText() const;

protected:
    QPtrList<CSSValueImpl> m_values;
};


class CSSPrimitiveValueImpl : public CSSValueImpl
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
    CSSPrimitiveValueImpl();
    CSSPrimitiveValueImpl(int ident);
    CSSPrimitiveValueImpl(double, UnitTypes);
    CSSPrimitiveValueImpl(const String&, UnitTypes);
    CSSPrimitiveValueImpl(PassRefPtr<CounterImpl>);
    CSSPrimitiveValueImpl(PassRefPtr<RectImpl>);
    CSSPrimitiveValueImpl(unsigned color); // RGB value
    CSSPrimitiveValueImpl(PassRefPtr<PairImpl>);

#if __APPLE__
    CSSPrimitiveValueImpl(PassRefPtr<DashboardRegionImpl>); // FIXME: Why is dashboard region a primitive value? This makes no sense.
#endif

    virtual ~CSSPrimitiveValueImpl();

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
    int computeLength(RenderStyle*);
    int computeLength(RenderStyle*, double multiplier);
    double computeLengthFloat(RenderStyle*, bool applyZoomFactor = true);

    // use with care!!!
    void setPrimitiveType(unsigned short type) { m_type = type; }
    void setFloatValue(unsigned short unitType, double floatValue, ExceptionCode&);
    double getFloatValue(unsigned short /* unitType */) const { return m_value.num; }

    void setStringValue(unsigned short stringType, const String& stringValue, ExceptionCode&);
    String getStringValue() const;
    
    CounterImpl* getCounterValue () const {
        return m_type != CSS_COUNTER ? 0 : m_value.counter;
    }

    RectImpl* getRectValue () const {
        return m_type != CSS_RECT ? 0 : m_value.rect;
    }

    unsigned getRGBColorValue() const {
        return m_type != CSS_RGBCOLOR ? 0 : m_value.rgbcolor;
    }

    PairImpl* getPairValue() const {
        return m_type != CSS_PAIR ? 0 : m_value.pair;
    }

#if __APPLE__
    DashboardRegionImpl *getDashboardRegionValue () const {
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
        CounterImpl* counter;
        RectImpl* rect;
        unsigned rgbcolor;
        PairImpl* pair;
#if __APPLE__
        DashboardRegionImpl* region;
#endif
    } m_value;
};

// FIXME: Remove when we rename everything to remove Impl.
typedef CSSPrimitiveValueImpl CSSPrimitiveValue;

// This value is used to handle quirky margins in reflow roots (body, td, and th) like WinIE.
// The basic idea is that a stylesheet can use the value __qem (for quirky em) instead of em
// in a stylesheet.  When the quirky value is used, if you're in quirks mode, the margin will
// collapse away inside a table cell.
class CSSQuirkPrimitiveValueImpl : public CSSPrimitiveValueImpl
{
public:
    CSSQuirkPrimitiveValueImpl(double num, UnitTypes type)
        : CSSPrimitiveValueImpl(num, type) {}

    virtual bool isQuirkValue() { return true; }
};

class CounterImpl : public Shared<CounterImpl> {
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

    CSSPrimitiveValueImpl* top() const { return m_top.get(); }
    CSSPrimitiveValueImpl* right() const { return m_right.get(); }
    CSSPrimitiveValueImpl* bottom() const { return m_bottom.get(); }
    CSSPrimitiveValueImpl* left() const { return m_left.get(); }

    void setTop(PassRefPtr<CSSPrimitiveValueImpl> top) { m_top = top; }
    void setRight(PassRefPtr<CSSPrimitiveValueImpl> right) { m_right = right; }
    void setBottom(PassRefPtr<CSSPrimitiveValueImpl> bottom) { m_bottom = bottom; }
    void setLeft(PassRefPtr<CSSPrimitiveValueImpl> left) { m_left = left; }

protected:
    RefPtr<CSSPrimitiveValueImpl> m_top;
    RefPtr<CSSPrimitiveValueImpl> m_right;
    RefPtr<CSSPrimitiveValueImpl> m_bottom;
    RefPtr<CSSPrimitiveValueImpl> m_left;
};

// A primitive value representing a pair.  This is useful for properties like border-radius, background-size/position,
// and border-spacing (all of which are space-separated sets of two values).  At the moment we are only using it for
// border-radius, but (FIXME) border-spacing and background-position could be converted over to use it
// (eliminating some extra -khtml- internal properties).
class PairImpl : public Shared<PairImpl> {
public:
    virtual ~PairImpl();

    CSSPrimitiveValueImpl* first() const { return m_first.get(); }
    CSSPrimitiveValueImpl* second() const { return m_second.get(); }

    void setFirst(PassRefPtr<CSSPrimitiveValueImpl> first) { m_first = first; }
    void setSecond(PassRefPtr<CSSPrimitiveValueImpl> second) { m_second = second; }

protected:
    RefPtr<CSSPrimitiveValueImpl> m_first;
    RefPtr<CSSPrimitiveValueImpl> m_second;
};

class DashboardRegionImpl : public RectImpl {
public:
    DashboardRegionImpl() : m_isCircle(0), m_isRectangle(0) { }

    RefPtr<DashboardRegionImpl> m_next;
    QString m_label;
    QString m_geometryType;
    bool m_isCircle : 1;
    bool m_isRectangle : 1;
};

class CSSImageValueImpl : public CSSPrimitiveValueImpl, public CachedObjectClient
{
public:
    CSSImageValueImpl();
    CSSImageValueImpl(const String& url, StyleBaseImpl*);
    virtual ~CSSImageValueImpl();

    CachedImage* image(DocLoader*);

protected:
    CachedImage* m_image;
    bool m_accessedImage;
};

class CSSBorderImageValueImpl : public CSSValueImpl
{
public:
    CSSBorderImageValueImpl();
    CSSBorderImageValueImpl(PassRefPtr<CSSImageValueImpl>,
        PassRefPtr<RectImpl>, int horizontalRule, int verticalRule);

    virtual String cssText() const;

public:
    // The border image.
    RefPtr<CSSImageValueImpl> m_image;

    // These four values are used to make "cuts" in the image.  They can be numbers
    // or percentages.
    RefPtr<RectImpl> m_imageSliceRect;
    
    // Values for how to handle the scaling/stretching/tiling of the image slices.
    int m_horizontalSizeRule; // Rule for how to adjust the widths of the top/middle/bottom
    int m_verticalSizeRule; // Rule for how to adjust the heights of the left/middle/right
};

class FontFamilyValueImpl : public CSSPrimitiveValueImpl
{
public:
    FontFamilyValueImpl( const QString &string);
    const QString& fontName() const { return parsedFontName; }
    int genericFamilyType() const { return _genericFamilyType; }

    virtual String cssText() const;

    QString parsedFontName;
private:
    int _genericFamilyType;
};

class FontValueImpl : public CSSValueImpl
{
public:
    virtual String cssText() const;
    
    virtual bool isFontValue() { return true; }

    RefPtr<CSSPrimitiveValueImpl> style;
    RefPtr<CSSPrimitiveValueImpl> variant;
    RefPtr<CSSPrimitiveValueImpl> weight;
    RefPtr<CSSPrimitiveValueImpl> size;
    RefPtr<CSSPrimitiveValueImpl> lineHeight;
    RefPtr<CSSValueListImpl> family;
};

// Used for text-shadow and box-shadow
class ShadowValueImpl : public CSSValueImpl
{
public:
    ShadowValueImpl(PassRefPtr<CSSPrimitiveValueImpl> x,
        PassRefPtr<CSSPrimitiveValueImpl> y,
        PassRefPtr<CSSPrimitiveValueImpl> blur,
        PassRefPtr<CSSPrimitiveValueImpl> color);
    
    virtual String cssText() const;

    RefPtr<CSSPrimitiveValueImpl> x;
    RefPtr<CSSPrimitiveValueImpl> y;
    RefPtr<CSSPrimitiveValueImpl> blur;
    RefPtr<CSSPrimitiveValueImpl> color;
};

// ------------------------------------------------------------------------------

// another helper class
class CSSProperty
{
public:
    CSSProperty(int propID, PassRefPtr<CSSValueImpl> value, bool important = false, int shorthandID = 0, bool implicit = false)
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

    CSSValueImpl* value() const { return m_value.get(); }
    
    String cssText() const;

    // make sure the following fits in 4 bytes.
    int m_id;
    int m_shorthandID;  // If this property was set as part of a shorthand, gives the shorthand.
    bool m_important : 1;
    bool m_implicit : 1; // Whether or not the property was set implicitly as the result of a shorthand.

    friend bool operator==(const CSSProperty &, const CSSProperty &);

    RefPtr<CSSValueImpl> m_value;
};

class CSSMutableStyleDeclarationImpl : public CSSStyleDeclarationImpl
{
public:
    CSSMutableStyleDeclarationImpl();
    CSSMutableStyleDeclarationImpl(CSSRuleImpl* parentRule);
    CSSMutableStyleDeclarationImpl(CSSRuleImpl* parentRule, const QValueList<CSSProperty> &);
    CSSMutableStyleDeclarationImpl(CSSRuleImpl* parentRule, const CSSProperty * const *, int numProperties);

    CSSMutableStyleDeclarationImpl &operator=(const CSSMutableStyleDeclarationImpl &);

    void setNode(NodeImpl* node) { m_node = node; }

    virtual String cssText() const;
    virtual void setCssText(const String&, ExceptionCode&);

    virtual unsigned length() const;
    virtual String item(unsigned index) const;

    virtual PassRefPtr<CSSValueImpl> getPropertyCSSValue(int propertyID) const;
    virtual String getPropertyValue(int propertyID) const;
    virtual bool getPropertyPriority(int propertyID) const;
    virtual int getPropertyShorthand(int propertyID) const;
    virtual bool isPropertyImplicit(int propertyID) const;

    virtual void setProperty(int propertyId, const String& value, bool important, ExceptionCode&);
    virtual String removeProperty(int propertyID, ExceptionCode&);

    virtual PassRefPtr<CSSMutableStyleDeclarationImpl> copy() const;
    virtual PassRefPtr<CSSMutableStyleDeclarationImpl> makeMutable();

    QValueListConstIterator<CSSProperty> valuesIterator() const { return m_values.begin(); }

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
    void setStringProperty(int propertyId, const String& value, CSSPrimitiveValueImpl::UnitTypes, bool important = false); // parsed string value
    void setImageProperty(int propertyId, const String& URL, bool important = false);
 
    // The following parses an entire new style declaration.
    void parseDeclaration(const String& styleDeclaration);

    // Besides adding the properties, this also removes any existing properties with these IDs.
    // It does no notification since it's called by the parser.
    void addParsedProperties(const CSSProperty * const *, int numProperties);
 
    PassRefPtr<CSSMutableStyleDeclarationImpl> copyBlockProperties() const;
    void removeBlockProperties();
    void removeInheritableProperties();
    void removePropertiesInSet(const int* set, unsigned length);

    void merge(CSSMutableStyleDeclarationImpl*, bool argOverridesOnConflict = true);
 
private:
    String getShortHandValue(const int* properties, int number) const;
    String get4Values(const int* properties) const;
 
    QValueList<CSSProperty> m_values;
    NodeImpl* m_node;
};

} // namespace

#endif
