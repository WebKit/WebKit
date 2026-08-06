/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSUnitValue.h"
#include "CSSUnits.h"
#include "Supplementable.h"

#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Document;
class DOMCSSNamespace;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSNumericFactory);
class CSSNumericFactory final : public Supplement<DOMCSSNamespace> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(CSSNumericFactory, CSSNumericFactory);
public:
    explicit CSSNumericFactory(DOMCSSNamespace&) { }

    static Ref<CSSUnitValue> number(double value) { return CSSUnitValue::create(value, CSSUnitType::Number); }
    static Ref<CSSUnitValue> percent(double value) { return CSSUnitValue::create(value, CSSUnitType::Percentage); }


    // <length>
    static Ref<CSSUnitValue> em(double value) { return CSSUnitValue::create(value, CSSUnitType::Em); }
    static Ref<CSSUnitValue> rem(double value) { return CSSUnitValue::create(value, CSSUnitType::RootEm); }
    static Ref<CSSUnitValue> ex(double value) { return CSSUnitValue::create(value, CSSUnitType::Ex); }
    static Ref<CSSUnitValue> rex(double value) { return CSSUnitValue::create(value, CSSUnitType::RootEx); }
    static Ref<CSSUnitValue> cap(double value) { return CSSUnitValue::create(value, CSSUnitType::Cap); }
    static Ref<CSSUnitValue> rcap(double value) { return CSSUnitValue::create(value, CSSUnitType::RootCap); }
    static Ref<CSSUnitValue> ch(double value) { return CSSUnitValue::create(value, CSSUnitType::Ch); }
    static Ref<CSSUnitValue> rch(double value) { return CSSUnitValue::create(value, CSSUnitType::RootCh); }
    static Ref<CSSUnitValue> ic(double value) { return CSSUnitValue::create(value, CSSUnitType::Ic); }
    static Ref<CSSUnitValue> ric(double value) { return CSSUnitValue::create(value, CSSUnitType::RootIc); }
    static Ref<CSSUnitValue> lh(double value) { return CSSUnitValue::create(value, CSSUnitType::LineHeight); }
    static Ref<CSSUnitValue> rlh(double value) { return CSSUnitValue::create(value, CSSUnitType::RootLineHeight); }
    static Ref<CSSUnitValue> vw(double value) { return CSSUnitValue::create(value, CSSUnitType::ViewportWidth); }
    static Ref<CSSUnitValue> vh(double value) { return CSSUnitValue::create(value, CSSUnitType::ViewportHeight); }
    static Ref<CSSUnitValue> vi(double value) { return CSSUnitValue::create(value, CSSUnitType::ViewportInline); }
    static Ref<CSSUnitValue> vb(double value) { return CSSUnitValue::create(value, CSSUnitType::ViewportBlock); }
    static Ref<CSSUnitValue> vmin(double value) { return CSSUnitValue::create(value, CSSUnitType::ViewportMin); }
    static Ref<CSSUnitValue> vmax(double value) { return CSSUnitValue::create(value, CSSUnitType::ViewportMax); }
    static Ref<CSSUnitValue> cm(double value) { return CSSUnitValue::create(value, CSSUnitType::Centimeter); }
    static Ref<CSSUnitValue> mm(double value) { return CSSUnitValue::create(value, CSSUnitType::Millimeter); }
    static Ref<CSSUnitValue> q(double value) { return CSSUnitValue::create(value, CSSUnitType::QuarterMillimeter); }
    static Ref<CSSUnitValue> in(double value) { return CSSUnitValue::create(value, CSSUnitType::Inch); }
    static Ref<CSSUnitValue> pt(double value) { return CSSUnitValue::create(value, CSSUnitType::Point); }
    static Ref<CSSUnitValue> pc(double value) { return CSSUnitValue::create(value, CSSUnitType::Pica); }
    static Ref<CSSUnitValue> px(double value) { return CSSUnitValue::create(value, CSSUnitType::Pixel); }
    static Ref<CSSUnitValue> cqw(double value) { return CSSUnitValue::create(value, CSSUnitType::ContainerQueryWidth); }
    static Ref<CSSUnitValue> cqh(double value) { return CSSUnitValue::create(value, CSSUnitType::ContainerQueryHeight); }
    static Ref<CSSUnitValue> cqi(double value) { return CSSUnitValue::create(value, CSSUnitType::ContainerQueryInline); }
    static Ref<CSSUnitValue> cqb(double value) { return CSSUnitValue::create(value, CSSUnitType::ContainerQueryBlock); }
    static Ref<CSSUnitValue> cqmin(double value) { return CSSUnitValue::create(value, CSSUnitType::ContainerQueryMin); }
    static Ref<CSSUnitValue> cqmax(double value) { return CSSUnitValue::create(value, CSSUnitType::ContainerQueryMax); }
    static Ref<CSSUnitValue> svw(double value) { return CSSUnitValue::create(value, CSSUnitType::SmallViewportWidth); }
    static Ref<CSSUnitValue> svh(double value) { return CSSUnitValue::create(value, CSSUnitType::SmallViewportHeight); }
    static Ref<CSSUnitValue> svi(double value) { return CSSUnitValue::create(value, CSSUnitType::SmallViewportInline); }
    static Ref<CSSUnitValue> svb(double value) { return CSSUnitValue::create(value, CSSUnitType::SmallViewportBlock); }
    static Ref<CSSUnitValue> svmin(double value) { return CSSUnitValue::create(value, CSSUnitType::SmallViewportMin); }
    static Ref<CSSUnitValue> svmax(double value) { return CSSUnitValue::create(value, CSSUnitType::SmallViewportMax); }
    static Ref<CSSUnitValue> lvw(double value) { return CSSUnitValue::create(value, CSSUnitType::LargeViewportWidth); }
    static Ref<CSSUnitValue> lvh(double value) { return CSSUnitValue::create(value, CSSUnitType::LargeViewportHeight); }
    static Ref<CSSUnitValue> lvi(double value) { return CSSUnitValue::create(value, CSSUnitType::LargeViewportInline); }
    static Ref<CSSUnitValue> lvb(double value) { return CSSUnitValue::create(value, CSSUnitType::LargeViewportBlock); }
    static Ref<CSSUnitValue> lvmin(double value) { return CSSUnitValue::create(value, CSSUnitType::LargeViewportMin); }
    static Ref<CSSUnitValue> lvmax(double value) { return CSSUnitValue::create(value, CSSUnitType::LargeViewportMax); }
    static Ref<CSSUnitValue> dvw(double value) { return CSSUnitValue::create(value, CSSUnitType::DynamicViewportWidth); }
    static Ref<CSSUnitValue> dvh(double value) { return CSSUnitValue::create(value, CSSUnitType::DynamicViewportHeight); }
    static Ref<CSSUnitValue> dvi(double value) { return CSSUnitValue::create(value, CSSUnitType::DynamicViewportInline); }
    static Ref<CSSUnitValue> dvb(double value) { return CSSUnitValue::create(value, CSSUnitType::DynamicViewportBlock); }
    static Ref<CSSUnitValue> dvmin(double value) { return CSSUnitValue::create(value, CSSUnitType::DynamicViewportMin); }
    static Ref<CSSUnitValue> dvmax(double value) { return CSSUnitValue::create(value, CSSUnitType::DynamicViewportMax); }


    // <angle>
    static Ref<CSSUnitValue> deg(double value) { return CSSUnitValue::create(value, CSSUnitType::Degree); }
    static Ref<CSSUnitValue> grad(double value) { return CSSUnitValue::create(value, CSSUnitType::Gradian); }
    static Ref<CSSUnitValue> rad(double value) { return CSSUnitValue::create(value, CSSUnitType::Radian); }
    static Ref<CSSUnitValue> turn(double value) { return CSSUnitValue::create(value, CSSUnitType::Turn); }


    // <time>
    static Ref<CSSUnitValue> s(double value) { return CSSUnitValue::create(value, CSSUnitType::Second); }
    static Ref<CSSUnitValue> ms(double value) { return CSSUnitValue::create(value, CSSUnitType::Millisecond); }


    // <frequency>
    static Ref<CSSUnitValue> hz(double value) { return CSSUnitValue::create(value, CSSUnitType::Hertz); }
    static Ref<CSSUnitValue> kHz(double value) { return CSSUnitValue::create(value, CSSUnitType::Kilohertz); }


    // <resolution>
    static Ref<CSSUnitValue> dpi(double value) { return CSSUnitValue::create(value, CSSUnitType::DotsPerInch); }
    static Ref<CSSUnitValue> dpcm(double value) { return CSSUnitValue::create(value, CSSUnitType::DotsPerCentimeter); }
    static Ref<CSSUnitValue> dppx(double value) { return CSSUnitValue::create(value, CSSUnitType::DotsPerPixel); }


    // <flex>
    static Ref<CSSUnitValue> fr(double value) { return CSSUnitValue::create(value, CSSUnitType::Fraction); }


private:
    static CSSNumericFactory* from(DOMCSSNamespace&);
    static ASCIILiteral supplementName() { return "CSSNumericFactory"_s; }
    bool isCSSNumericFactory() const final { return true; }
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CSSNumericFactory)
    static bool isType(const WebCore::SupplementBase& supplement) { return supplement.isCSSNumericFactory(); }
SPECIALIZE_TYPE_TRAITS_END()
