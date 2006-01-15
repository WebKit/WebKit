/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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

#ifndef _CSS_css_valueimpl_h_
#define _CSS_css_valueimpl_h_

#include "dom/css_value.h"
#include "css/css_base.h"
#include "CachedObjectClient.h"
#include <qvaluelist.h>

namespace khtml {
    class RenderStyle;
    class CachedImage;
    class DocLoader;
}

namespace DOM {

class CSSMutableStyleDeclarationImpl;
class CounterImpl;
class DashboardRegionImpl;
class RectImpl;
class PairImpl;
class NodeImpl;

extern const int inheritableProperties[];
extern const unsigned numInheritableProperties;

class CSSStyleDeclarationImpl : public StyleBaseImpl
{
public:
    virtual bool isStyleDeclaration();

    static bool isPropertyName(const DOMString &propertyName);

    CSSRuleImpl *parentRule() const;

    virtual DOMString cssText() const = 0;
    virtual void setCssText(const DOMString &, int &exceptionCode) = 0;

    virtual unsigned length() const = 0;
    virtual DOMString item(unsigned index) const = 0;

    CSSValueImpl *getPropertyCSSValue(const DOMString &propertyName);
    DOMString getPropertyValue(const DOMString &propertyName);
    DOMString getPropertyPriority(const DOMString &propertyName);
    DOMString getPropertyShorthand(const DOMString& propertyName);
    bool isPropertyImplicit(const DOMString& propertyName);
    
    virtual CSSValueImpl *getPropertyCSSValue(int propertyID) const = 0;
    virtual DOMString getPropertyValue(int propertyID) const = 0;
    virtual bool getPropertyPriority(int propertyID) const = 0;
    virtual int getPropertyShorthand(int propertyID) const = 0;
    virtual bool isPropertyImplicit(int propertyID) const = 0;

    void setProperty(const DOMString &propertyName, const DOMString &value, const DOMString &priority, int &exception);
    DOMString removeProperty(const DOMString &propertyName, int &exception);
    virtual void setProperty(int propertyId, const DOMString &value, bool important, int &exceptionCode) = 0;
    virtual DOMString removeProperty(int propertyID, int &exceptionCode) = 0;

    virtual CSSMutableStyleDeclarationImpl *copy() const = 0;
    virtual CSSMutableStyleDeclarationImpl *makeMutable() = 0;
 
    void diff(CSSMutableStyleDeclarationImpl *) const;

    CSSMutableStyleDeclarationImpl *copyPropertiesInSet(const int *set, unsigned length) const;

protected:
    CSSStyleDeclarationImpl(CSSRuleImpl *parentRule = 0);


private:
    CSSStyleDeclarationImpl(const CSSStyleDeclarationImpl &);
    CSSStyleDeclarationImpl& operator=(const CSSStyleDeclarationImpl &);
};

class CSSValueImpl : public StyleBaseImpl
{
public:
    virtual unsigned short cssValueType() const { return CSSValue::CSS_CUSTOM; }
    virtual DOMString cssText() const = 0;
    void setCssText(const DOMString &) { } // FIXME: Not implemented.

    virtual bool isValue() { return true; }
    virtual bool isFontValue() { return false; }
};

class CSSInheritedValueImpl : public CSSValueImpl
{
public:
    virtual unsigned short cssValueType() const;
    virtual DOMString cssText() const;
};

class CSSInitialValueImpl : public CSSValueImpl
{
public:
    virtual unsigned short cssValueType() const;
    virtual DOMString cssText() const;
};

class CSSValueListImpl : public CSSValueImpl
{
public:
    CSSValueListImpl();

    virtual ~CSSValueListImpl();

    unsigned length() const { return m_values.count(); }
    CSSValueImpl *item ( unsigned index ) { return m_values.at(index); }

    virtual bool isValueList() { return true; }

    virtual unsigned short cssValueType() const;

    void append(CSSValueImpl *val);
    virtual DOMString cssText() const;

protected:
    QPtrList<CSSValueImpl> m_values;
};


class CSSPrimitiveValueImpl : public CSSValueImpl
{
public:
    CSSPrimitiveValueImpl();
    CSSPrimitiveValueImpl(int ident);
    CSSPrimitiveValueImpl(double num, CSSPrimitiveValue::UnitTypes type);
    CSSPrimitiveValueImpl(const DOMString &str, CSSPrimitiveValue::UnitTypes type);
    CSSPrimitiveValueImpl(CounterImpl *c);
    CSSPrimitiveValueImpl(RectImpl *r);
    CSSPrimitiveValueImpl(DashboardRegionImpl *r); // FIXME: Why is dashboard region a primitive value? This makes no sense.
    CSSPrimitiveValueImpl(QRgb color);
    CSSPrimitiveValueImpl(PairImpl* p);

    virtual ~CSSPrimitiveValueImpl();

    void cleanup();

    unsigned short primitiveType() const {
	    return m_type;
    }

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
    int computeLength( khtml::RenderStyle *style, QPaintDeviceMetrics *devMetrics );
    int computeLength( khtml::RenderStyle *style, QPaintDeviceMetrics *devMetrics, double multiplier );
    double computeLengthFloat( khtml::RenderStyle *style, QPaintDeviceMetrics *devMetrics,
                               bool applyZoomFactor = true );

    // use with care!!!
    void setPrimitiveType(unsigned short type) { m_type = type; }
    void setFloatValue ( unsigned short unitType, double floatValue, int &exceptioncode );
    double getFloatValue ( unsigned short/* unitType */) const {
	return m_value.num;
    }

    void setStringValue ( unsigned short stringType, const DOM::DOMString &stringValue, int &exceptioncode );
    DOM::DOMString getStringValue() const;
    
    CounterImpl *getCounterValue () const {
        return ( m_type != CSSPrimitiveValue::CSS_COUNTER ? 0 : m_value.counter );
    }

    RectImpl *getRectValue () const {
	return ( m_type != CSSPrimitiveValue::CSS_RECT ? 0 : m_value.rect );
    }

    QRgb getRGBColorValue () const {
	return ( m_type != CSSPrimitiveValue::CSS_RGBCOLOR ? 0 : m_value.rgbcolor );
    }

    PairImpl* getPairValue() const {
	return (m_type != CSSPrimitiveValue::CSS_PAIR ? 0 : m_value.pair);
    }

    DashboardRegionImpl *getDashboardRegionValue () const {
	return ( m_type != CSSPrimitiveValue::CSS_DASHBOARD_REGION ? 0 : m_value.region );
    }

    virtual bool isPrimitiveValue() const { return true; }
    virtual unsigned short cssValueType() const;

    int getIdent();

    virtual bool parseString( const DOMString &string, bool = false);
    virtual DOMString cssText() const;

    virtual bool isQuirkValue() { return false; }

protected:
    int m_type;
    union {
	int ident;
	double num;
	DOMStringImpl *string;
	CounterImpl *counter;
	RectImpl *rect;
        QRgb rgbcolor;
        PairImpl *pair;
        DashboardRegionImpl *region;
    } m_value;
};

// This value is used to handle quirky margins in reflow roots (body, td, and th) like WinIE.
// The basic idea is that a stylesheet can use the value __qem (for quirky em) instead of em
// in a stylesheet.  When the quirky value is used, if you're in quirks mode, the margin will
// collapse away inside a table cell.
class CSSQuirkPrimitiveValueImpl : public CSSPrimitiveValueImpl
{
public:
    CSSQuirkPrimitiveValueImpl(double num, CSSPrimitiveValue::UnitTypes type)
      :CSSPrimitiveValueImpl(num, type) {}

    virtual bool isQuirkValue() { return true; }
};

class CounterImpl : public khtml::Shared<CounterImpl> {
public:
    DOMString identifier() const { return m_identifier; }
    DOMString listStyle() const { return m_listStyle; }
    DOMString separator() const { return m_separator; }

    DOMString m_identifier;
    DOMString m_listStyle;
    DOMString m_separator;
};

class RectImpl : public khtml::Shared<RectImpl> {
public:
    RectImpl();
    virtual ~RectImpl();

    CSSPrimitiveValueImpl *top() const { return m_top.get(); }
    CSSPrimitiveValueImpl *right() const { return m_right.get(); }
    CSSPrimitiveValueImpl *bottom() const { return m_bottom.get(); }
    CSSPrimitiveValueImpl *left() const { return m_left.get(); }

    void setTop( CSSPrimitiveValueImpl *top );
    void setRight( CSSPrimitiveValueImpl *right );
    void setBottom( CSSPrimitiveValueImpl *bottom );
    void setLeft( CSSPrimitiveValueImpl *left );
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
class PairImpl : public khtml::Shared<PairImpl> {
public:
    PairImpl();
    virtual ~PairImpl();

    CSSPrimitiveValueImpl *first() const { return m_first.get(); }
    CSSPrimitiveValueImpl *second() const { return m_second.get(); }

    void setFirst(CSSPrimitiveValueImpl *first);
    void setSecond(CSSPrimitiveValueImpl *second);

protected:
    RefPtr<CSSPrimitiveValueImpl> m_first;
    RefPtr<CSSPrimitiveValueImpl> m_second;
};


class DashboardRegionImpl : public RectImpl {
public:
    DashboardRegionImpl() : m_next(0), m_isCircle(0), m_isRectangle(0) { }
    ~DashboardRegionImpl() {
        if (m_next)
            m_next->deref();
    }

    void setNext (DashboardRegionImpl *next)
    {
        m_next = next;
    }
    
public:
    RefPtr<DashboardRegionImpl> m_next;
    QString m_label;
    QString m_geometryType;
    unsigned int m_isCircle:1;
    unsigned int m_isRectangle:1;
};


class CSSImageValueImpl : public CSSPrimitiveValueImpl, public khtml::CachedObjectClient
{
public:
    CSSImageValueImpl();
    CSSImageValueImpl(const DOMString &url, StyleBaseImpl *style);
    virtual ~CSSImageValueImpl();

    khtml::CachedImage *image(khtml::DocLoader* loader);

protected:
    khtml::CachedImage* m_image;
    bool m_accessedImage;
};

class CSSBorderImageValueImpl : public CSSValueImpl
{
public:
    CSSBorderImageValueImpl();
    CSSBorderImageValueImpl(CSSImageValueImpl* image, RectImpl* imageRect,
                            int horizontalRule, int verticalRule);
    virtual ~CSSBorderImageValueImpl();

    virtual DOMString cssText() const;

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
    const QString &fontName() const { return parsedFontName; }
    int genericFamilyType() const { return _genericFamilyType; }

    virtual DOMString cssText() const;

    QString parsedFontName;
private:
    int _genericFamilyType;
};

class FontValueImpl : public CSSValueImpl
{
public:
    FontValueImpl();
    virtual ~FontValueImpl();
        
    virtual DOMString cssText() const;
    
    virtual bool isFontValue() { return true; }

    CSSPrimitiveValueImpl *style;
    CSSPrimitiveValueImpl *variant;
    CSSPrimitiveValueImpl *weight;
    CSSPrimitiveValueImpl *size;
    CSSPrimitiveValueImpl *lineHeight;
    CSSValueListImpl *family;
};

// Used for text-shadow and box-shadow
class ShadowValueImpl : public CSSValueImpl
{
public:
    ShadowValueImpl(CSSPrimitiveValueImpl* _x, CSSPrimitiveValueImpl* _y,
                    CSSPrimitiveValueImpl* _blur, CSSPrimitiveValueImpl* _color);
    virtual ~ShadowValueImpl();
    
    virtual DOMString cssText() const;

    CSSPrimitiveValueImpl* x;
    CSSPrimitiveValueImpl* y;
    CSSPrimitiveValueImpl* blur;
    CSSPrimitiveValueImpl* color;
};

// ------------------------------------------------------------------------------

// another helper class
class CSSProperty
{
public:
    CSSProperty(int propID, CSSValueImpl *value, bool important = false, int shorthandID = 0, bool implicit = false)
        : m_id(propID), m_shorthandID(shorthandID), m_important(important), m_implicit(implicit), m_value(value)
    {
    }
    
    CSSProperty(const CSSProperty& o)
    {
        m_id = o.m_id;
        m_shorthandID = o.m_shorthandID;
        m_important = o.m_important;
        m_implicit = o.m_implicit;
        m_value = o.m_value;
    }
    
    CSSProperty &operator=(const CSSProperty& o)
    {
        m_id = o.m_id;
        m_shorthandID = o.m_shorthandID;
        m_important = o.m_important;
        m_value = o.m_value;
        return *this;
    }
    
    ~CSSProperty() {}

    void setValue(CSSValueImpl *val) {
        m_value = val;
    }

    int id() const { return m_id; }
    int shorthandID() const { return m_shorthandID; }
    
    bool isImportant() const { return m_important; }
    bool isImplicit() const { return m_implicit; }

    CSSValueImpl *value() const { return m_value.get(); }
    
    DOMString cssText() const;

    // make sure the following fits in 4 bytes.
    int m_id;
    int m_shorthandID;  // If this property was set as part of a shorthand, gives the shorthand.
    bool m_important : 1;
    bool m_implicit  : 1; // Whether or not the property was set implicitly as the result of a shorthand.

    friend bool operator==(const CSSProperty &, const CSSProperty &);

protected:
    RefPtr<CSSValueImpl> m_value;
};

class CSSMutableStyleDeclarationImpl : public CSSStyleDeclarationImpl
{
public:
    CSSMutableStyleDeclarationImpl();
    CSSMutableStyleDeclarationImpl(CSSRuleImpl *parentRule);
    CSSMutableStyleDeclarationImpl(CSSRuleImpl *parentRule, const QValueList<CSSProperty> &);
    CSSMutableStyleDeclarationImpl(CSSRuleImpl *parentRule, const CSSProperty * const *, int numProperties);
    virtual ~CSSMutableStyleDeclarationImpl();

    CSSMutableStyleDeclarationImpl &operator=(const CSSMutableStyleDeclarationImpl &);

    void setNode(NodeImpl *node) { m_node = node; }

    virtual DOMString cssText() const;
    virtual void setCssText(const DOMString &, int &exceptionCode);

    virtual unsigned length() const;
    virtual DOMString item(unsigned index) const;

    virtual CSSValueImpl *getPropertyCSSValue(int propertyID) const;
    virtual DOMString getPropertyValue(int propertyID) const;
    virtual bool getPropertyPriority(int propertyID) const;
    virtual int getPropertyShorthand(int propertyID) const;
    virtual bool isPropertyImplicit(int propertyID) const;

    virtual void setProperty(int propertyId, const DOMString &value, bool important, int &exceptionCode);
    virtual DOMString removeProperty(int propertyID, int &exceptionCode);

    virtual CSSMutableStyleDeclarationImpl *copy() const;
    virtual CSSMutableStyleDeclarationImpl *makeMutable();

    QValueListConstIterator<CSSProperty> valuesIterator() const { return m_values.begin(); }

    bool setProperty(int propertyID, int value, bool important = false, bool notifyChanged = true);
    bool setProperty(int propertyID, const DOMString &value, bool important, bool notifyChanged, int &exceptionCode);
    bool setProperty(int propertyId, const DOMString &value, bool important = false, bool notifyChanged = true)
        { int exceptionCode; return setProperty(propertyId, value, important, notifyChanged, exceptionCode); }

    DOMString removeProperty(int propertyID, bool notifyChanged, int &exceptionCode);
    DOMString removeProperty(int propertyID, bool notifyChanged = true)
        { int exceptionCode; return removeProperty(propertyID, notifyChanged, exceptionCode); }

    void clear();

    void setChanged();
 
    // setLengthProperty treats integers as pixels! (Needed for conversion of HTML attributes.)
    void setLengthProperty(int propertyId, const DOMString &value, bool important, bool multiLength = false);
    void setStringProperty(int propertyId, const DOMString &value, CSSPrimitiveValue::UnitTypes, bool important = false); // parsed string value
    void setImageProperty(int propertyId, const DOMString &URL, bool important = false);
 
    // The following parses an entire new style declaration.
    void parseDeclaration(const DOMString &styleDeclaration);

    // Besides adding the properties, this also removes any existing properties with these IDs.
    // It does no notification since it's called by the parser.
    void addParsedProperties(const CSSProperty * const *, int numProperties);
 
    CSSMutableStyleDeclarationImpl *copyBlockProperties() const;
    void removeBlockProperties();
    void removeInheritableProperties();
    void removePropertiesInSet(const int *set, unsigned length);

    void merge(CSSMutableStyleDeclarationImpl *, bool argOverridesOnConflict = true);
 
private:
    DOMString getShortHandValue(const int* properties, int number) const;
    DOMString get4Values(const int* properties) const;
 
    QValueList<CSSProperty> m_values;
    NodeImpl *m_node;
};

} // namespace

#endif
