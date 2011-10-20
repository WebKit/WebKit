/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef CSSStyleDeclaration_h
#define CSSStyleDeclaration_h

#include "CSSRule.h"
#include "StyleBase.h"
#include <wtf/Forward.h>

namespace WebCore {

class CSSMutableStyleDeclaration;
class CSSProperty;
class CSSStyleSheet;
class CSSValue;

typedef int ExceptionCode;

class CSSStyleDeclaration : public RefCounted<CSSStyleDeclaration> {
public:
    virtual ~CSSStyleDeclaration() { }

    static bool isPropertyName(const String&);

    // FIXME: Refactor so CSSStyleDeclaration never needs to have a style sheet parent.

    CSSRule* parentRule() const
    {
        return m_parentIsRule ? m_parentRule : 0;
    }

    void setParentRule(CSSRule* rule)
    {
        m_parentIsRule = true;
        m_parentRule = rule;
    }

    void setParentStyleSheet(CSSStyleSheet* styleSheet)
    {
        m_parentIsRule = false;
        m_parentStyleSheet = styleSheet;
    }

    CSSStyleSheet* parentStyleSheet() const
    {
        if (!m_parentIsRule)
            return m_parentStyleSheet;
        return m_parentRule ? m_parentRule->parentStyleSheet() : 0;
    }

    virtual String cssText() const = 0;
    virtual void setCssText(const String&, ExceptionCode&) = 0;

    unsigned length() const { return virtualLength(); }
    virtual unsigned virtualLength() const = 0;
    bool isEmpty() const { return !length(); }
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

    void setProperty(const String& propertyName, const String& value, ExceptionCode&);
    void setProperty(const String& propertyName, const String& value, const String& priority, ExceptionCode&);
    String removeProperty(const String& propertyName, ExceptionCode&);
    virtual void setProperty(int propertyId, const String& value, bool important, ExceptionCode&) = 0;
    virtual String removeProperty(int propertyID, ExceptionCode&) = 0;

    virtual PassRefPtr<CSSMutableStyleDeclaration> copy() const = 0;
    virtual PassRefPtr<CSSMutableStyleDeclaration> makeMutable() = 0;

    void diff(CSSMutableStyleDeclaration*) const;

    PassRefPtr<CSSMutableStyleDeclaration> copyPropertiesInSet(const int* set, unsigned length) const;

#ifndef NDEBUG
    void showStyle();
#endif

    virtual bool isMutableStyleDeclaration() const { return false; }

protected:
    CSSStyleDeclaration(CSSRule* parentRule = 0);

    virtual bool cssPropertyMatches(const CSSProperty*) const;

private:
    bool m_parentIsRule;
    union {
        CSSRule* m_parentRule;
        CSSStyleSheet* m_parentStyleSheet;
    };
};

} // namespace WebCore

#endif // CSSStyleDeclaration_h
