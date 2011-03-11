/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSStyleApplyProperty.h"

#include "CSSPrimitiveValueMappings.h"
#include "CSSStyleSelector.h"
#include "RenderStyle.h"
#include <wtf/StdLibExtras.h>
#include <wtf/UnusedParam.h>

using namespace std;

namespace WebCore {

class ApplyPropertyNull : public ApplyPropertyBase {
public:
    virtual void inherit(CSSStyleSelector*) const {}
    virtual void initial(CSSStyleSelector*) const {}
    virtual void value(CSSStyleSelector*, CSSValue*) const {}
};

template <typename T>
class ApplyPropertyDefault : public ApplyPropertyBase {
public:
    ApplyPropertyDefault(T (RenderStyle::*getter)() const, void (RenderStyle::*setter)(T), T (*initial)())
        : m_getter(getter)
        , m_setter(setter)
        , m_initial(initial)
    {
    }

    virtual void inherit(CSSStyleSelector* selector) const
    {
        (selector->style()->*m_setter)((selector->parentStyle()->*m_getter)());
    }

    virtual void initial(CSSStyleSelector* selector) const
    {
        (selector->style()->*m_setter)((*m_initial)());
    }

    virtual void value(CSSStyleSelector* selector, CSSValue* value) const
    {
        if (value->isPrimitiveValue())
            (selector->style()->*m_setter)(*(static_cast<CSSPrimitiveValue*>(value)));
    }

protected:
    T (RenderStyle::*m_getter)() const;
    void (RenderStyle::*m_setter)(T);
    T (*m_initial)();
};

// CSSPropertyColor
class ApplyPropertyColorBase : public ApplyPropertyBase {
public:
    ApplyPropertyColorBase(const Color& (RenderStyle::*getter)() const, const Color& (RenderStyle::*defaultValue)() const, void (RenderStyle::*setter)(const Color&))
        : m_getter(getter)
        , m_defaultValue(defaultValue)
        , m_setter(setter)
    {
    }
    virtual void inherit(CSSStyleSelector* selector) const
    {
        const Color& color = (selector->parentStyle()->*m_getter)();
        if (m_defaultValue && !color.isValid())
            (selector->style()->*m_setter)((selector->parentStyle()->*m_defaultValue)());
        else
            (selector->style()->*m_setter)(color);
    }
    virtual void initial(CSSStyleSelector* selector) const
    {
        Color color;
        (selector->style()->*m_setter)(color);
    }
    virtual void value(CSSStyleSelector* selector, CSSValue* value) const
    {
        if (value->isPrimitiveValue())
            (selector->style()->*m_setter)(selector->getColorFromPrimitiveValue(static_cast<CSSPrimitiveValue*>(value)));
    }
protected:
    const Color& (RenderStyle::*m_getter)() const;
    const Color& (RenderStyle::*m_defaultValue)() const;
    void (RenderStyle::*m_setter)(const Color&);
};

class ApplyPropertyColor : public ApplyPropertyColorBase {
public:
    ApplyPropertyColor(const Color& (RenderStyle::*getter)() const, void (RenderStyle::*setter)(const Color&), Color (*initialValue)())
        : ApplyPropertyColorBase(getter, 0, setter)
        , m_initialValue(initialValue)
    {
    }

    virtual void initial(CSSStyleSelector* selector) const
    {
        (selector->style()->*m_setter)(m_initialValue());
    }

    virtual void value(CSSStyleSelector* selector, CSSValue* value) const
    {
        if (!value->isPrimitiveValue())
            return;

        if ((static_cast<CSSPrimitiveValue*>(value))->getIdent() == CSSValueCurrentcolor)
            inherit(selector);
        else
            ApplyPropertyColorBase::value(selector, value);
    }
protected:
    Color (*m_initialValue)();
};

const CSSStyleApplyProperty& CSSStyleApplyProperty::sharedCSSStyleApplyProperty()
{
    DEFINE_STATIC_LOCAL(CSSStyleApplyProperty, cssStyleApplyPropertyInstance, ());
    return cssStyleApplyPropertyInstance;
}


CSSStyleApplyProperty::CSSStyleApplyProperty()
{
    for (int i = 0; i < numCSSProperties; ++i)
       m_propertyMap[i] = 0;

    setPropertyValue(CSSPropertyColor, new ApplyPropertyColor(&RenderStyle::color, &RenderStyle::setColor, RenderStyle::initialColor));
    setPropertyValue(CSSPropertyBackgroundColor, new ApplyPropertyColorBase(&RenderStyle::backgroundColor, 0, &RenderStyle::setBackgroundColor));
    setPropertyValue(CSSPropertyBorderBottomColor, new ApplyPropertyColorBase(&RenderStyle::borderBottomColor, &RenderStyle::color, &RenderStyle::setBorderBottomColor));
    setPropertyValue(CSSPropertyBorderLeftColor, new ApplyPropertyColorBase(&RenderStyle::borderLeftColor, &RenderStyle::color, &RenderStyle::setBorderLeftColor));
    setPropertyValue(CSSPropertyBorderRightColor, new ApplyPropertyColorBase(&RenderStyle::borderRightColor, &RenderStyle::color, &RenderStyle::setBorderRightColor));
    setPropertyValue(CSSPropertyBorderTopColor, new ApplyPropertyColorBase(&RenderStyle::borderTopColor, &RenderStyle::color, &RenderStyle::setBorderTopColor));
    setPropertyValue(CSSPropertyOutlineColor, new ApplyPropertyColorBase(&RenderStyle::outlineColor, &RenderStyle::color, &RenderStyle::setOutlineColor));
    setPropertyValue(CSSPropertyWebkitColumnRuleColor, new ApplyPropertyColorBase(&RenderStyle::columnRuleColor, &RenderStyle::color, &RenderStyle::setColumnRuleColor));
    setPropertyValue(CSSPropertyWebkitTextEmphasisColor, new ApplyPropertyColorBase(&RenderStyle::textEmphasisColor, &RenderStyle::color, &RenderStyle::setTextEmphasisColor));
    setPropertyValue(CSSPropertyWebkitTextFillColor, new ApplyPropertyColorBase(&RenderStyle::textFillColor, &RenderStyle::color, &RenderStyle::setTextFillColor));
    setPropertyValue(CSSPropertyWebkitTextStrokeColor, new ApplyPropertyColorBase(&RenderStyle::textStrokeColor, &RenderStyle::color, &RenderStyle::setTextStrokeColor));
}


}
