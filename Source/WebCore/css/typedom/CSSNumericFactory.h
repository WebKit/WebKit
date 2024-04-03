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
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(CSSNumericFactory);
public:
    explicit CSSNumericFactory(DOMCSSNamespace&) { }

    static Ref<CSSUnitValue> number(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_NUMBER); }
    static Ref<CSSUnitValue> percent(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_PERCENTAGE); }


    // <length>
    static Ref<CSSUnitValue> em(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_EM); }
    static Ref<CSSUnitValue> rem(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_REM); }
    static Ref<CSSUnitValue> ex(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_EX); }
    static Ref<CSSUnitValue> rex(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_REX); }
    static Ref<CSSUnitValue> cap(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_CAP); }
    static Ref<CSSUnitValue> rcap(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_RCAP); }
    static Ref<CSSUnitValue> ch(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_CH); }
    static Ref<CSSUnitValue> rch(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_RCH); }
    static Ref<CSSUnitValue> ic(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_IC); }
    static Ref<CSSUnitValue> ric(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_RIC); }
    static Ref<CSSUnitValue> lh(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_LH); }
    static Ref<CSSUnitValue> rlh(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_RLH); }
    static Ref<CSSUnitValue> vw(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_VW); }
    static Ref<CSSUnitValue> vh(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_VH); }
    static Ref<CSSUnitValue> vi(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_VI); }
    static Ref<CSSUnitValue> vb(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_VB); }
    static Ref<CSSUnitValue> vmin(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_VMIN); }
    static Ref<CSSUnitValue> vmax(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_VMAX); }
    static Ref<CSSUnitValue> cm(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_CM); }
    static Ref<CSSUnitValue> mm(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_MM); }
    static Ref<CSSUnitValue> q(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_Q); }
    static Ref<CSSUnitValue> in(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_IN); }
    static Ref<CSSUnitValue> pt(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_PT); }
    static Ref<CSSUnitValue> pc(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_PC); }
    static Ref<CSSUnitValue> px(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_PX); }
    static Ref<CSSUnitValue> cqw(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_CQW); }
    static Ref<CSSUnitValue> cqh(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_CQH); }
    static Ref<CSSUnitValue> cqi(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_CQI); }
    static Ref<CSSUnitValue> cqb(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_CQB); }
    static Ref<CSSUnitValue> cqmin(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_CQMIN); }
    static Ref<CSSUnitValue> cqmax(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_CQMAX); }
    static Ref<CSSUnitValue> svw(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_SVW); }
    static Ref<CSSUnitValue> svh(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_SVH); }
    static Ref<CSSUnitValue> svi(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_SVI); }
    static Ref<CSSUnitValue> svb(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_SVB); }
    static Ref<CSSUnitValue> svmin(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_SVMIN); }
    static Ref<CSSUnitValue> svmax(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_SVMAX); }
    static Ref<CSSUnitValue> lvw(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_LVW); }
    static Ref<CSSUnitValue> lvh(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_LVH); }
    static Ref<CSSUnitValue> lvi(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_LVI); }
    static Ref<CSSUnitValue> lvb(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_LVB); }
    static Ref<CSSUnitValue> lvmin(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_LVMIN); }
    static Ref<CSSUnitValue> lvmax(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_LVMAX); }
    static Ref<CSSUnitValue> dvw(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_DVW); }
    static Ref<CSSUnitValue> dvh(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_DVH); }
    static Ref<CSSUnitValue> dvi(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_DVI); }
    static Ref<CSSUnitValue> dvb(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_DVB); }
    static Ref<CSSUnitValue> dvmin(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_DVMIN); }
    static Ref<CSSUnitValue> dvmax(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_DVMAX); }


    // <angle>
    static Ref<CSSUnitValue> deg(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_DEG); }
    static Ref<CSSUnitValue> grad(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_GRAD); }
    static Ref<CSSUnitValue> rad(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_RAD); }
    static Ref<CSSUnitValue> turn(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_TURN); }


    // <time>
    static Ref<CSSUnitValue> s(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_S); }
    static Ref<CSSUnitValue> ms(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_MS); }


    // <frequency>
    static Ref<CSSUnitValue> hz(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_HZ); }
    static Ref<CSSUnitValue> kHz(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_KHZ); }


    // <resolution>
    static Ref<CSSUnitValue> dpi(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_DPI); }
    static Ref<CSSUnitValue> dpcm(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_DPCM); }
    static Ref<CSSUnitValue> dppx(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_DPPX); }


    // <flex>
    static Ref<CSSUnitValue> fr(double value) { return CSSUnitValue::create(value, CSSUnitType::CSS_FR); }


private:
    static CSSNumericFactory* from(DOMCSSNamespace&);
    static ASCIILiteral supplementName();
};

} // namespace WebCore
