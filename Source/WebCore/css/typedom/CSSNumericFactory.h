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
    static Ref<CSSUnitValue> rem(double value) { return CSSUnitValue::create(value, CSSUnitType::Rem); }
    static Ref<CSSUnitValue> ex(double value) { return CSSUnitValue::create(value, CSSUnitType::Ex); }
    static Ref<CSSUnitValue> rex(double value) { return CSSUnitValue::create(value, CSSUnitType::Rex); }
    static Ref<CSSUnitValue> cap(double value) { return CSSUnitValue::create(value, CSSUnitType::Cap); }
    static Ref<CSSUnitValue> rcap(double value) { return CSSUnitValue::create(value, CSSUnitType::Rcap); }
    static Ref<CSSUnitValue> ch(double value) { return CSSUnitValue::create(value, CSSUnitType::Ch); }
    static Ref<CSSUnitValue> rch(double value) { return CSSUnitValue::create(value, CSSUnitType::Rch); }
    static Ref<CSSUnitValue> ic(double value) { return CSSUnitValue::create(value, CSSUnitType::Ic); }
    static Ref<CSSUnitValue> ric(double value) { return CSSUnitValue::create(value, CSSUnitType::Ric); }
    static Ref<CSSUnitValue> lh(double value) { return CSSUnitValue::create(value, CSSUnitType::Lh); }
    static Ref<CSSUnitValue> rlh(double value) { return CSSUnitValue::create(value, CSSUnitType::Rlh); }
    static Ref<CSSUnitValue> vw(double value) { return CSSUnitValue::create(value, CSSUnitType::Vw); }
    static Ref<CSSUnitValue> vh(double value) { return CSSUnitValue::create(value, CSSUnitType::Vh); }
    static Ref<CSSUnitValue> vi(double value) { return CSSUnitValue::create(value, CSSUnitType::Vi); }
    static Ref<CSSUnitValue> vb(double value) { return CSSUnitValue::create(value, CSSUnitType::Vb); }
    static Ref<CSSUnitValue> vmin(double value) { return CSSUnitValue::create(value, CSSUnitType::Vmin); }
    static Ref<CSSUnitValue> vmax(double value) { return CSSUnitValue::create(value, CSSUnitType::Vmax); }
    static Ref<CSSUnitValue> cm(double value) { return CSSUnitValue::create(value, CSSUnitType::Cm); }
    static Ref<CSSUnitValue> mm(double value) { return CSSUnitValue::create(value, CSSUnitType::Mm); }
    static Ref<CSSUnitValue> q(double value) { return CSSUnitValue::create(value, CSSUnitType::Q); }
    static Ref<CSSUnitValue> in(double value) { return CSSUnitValue::create(value, CSSUnitType::In); }
    static Ref<CSSUnitValue> pt(double value) { return CSSUnitValue::create(value, CSSUnitType::Pt); }
    static Ref<CSSUnitValue> pc(double value) { return CSSUnitValue::create(value, CSSUnitType::Pc); }
    static Ref<CSSUnitValue> px(double value) { return CSSUnitValue::create(value, CSSUnitType::Px); }
    static Ref<CSSUnitValue> cqw(double value) { return CSSUnitValue::create(value, CSSUnitType::Cqw); }
    static Ref<CSSUnitValue> cqh(double value) { return CSSUnitValue::create(value, CSSUnitType::Cqh); }
    static Ref<CSSUnitValue> cqi(double value) { return CSSUnitValue::create(value, CSSUnitType::Cqi); }
    static Ref<CSSUnitValue> cqb(double value) { return CSSUnitValue::create(value, CSSUnitType::Cqb); }
    static Ref<CSSUnitValue> cqmin(double value) { return CSSUnitValue::create(value, CSSUnitType::Cqmin); }
    static Ref<CSSUnitValue> cqmax(double value) { return CSSUnitValue::create(value, CSSUnitType::Cqmax); }
    static Ref<CSSUnitValue> svw(double value) { return CSSUnitValue::create(value, CSSUnitType::Svw); }
    static Ref<CSSUnitValue> svh(double value) { return CSSUnitValue::create(value, CSSUnitType::Svh); }
    static Ref<CSSUnitValue> svi(double value) { return CSSUnitValue::create(value, CSSUnitType::Svi); }
    static Ref<CSSUnitValue> svb(double value) { return CSSUnitValue::create(value, CSSUnitType::Svb); }
    static Ref<CSSUnitValue> svmin(double value) { return CSSUnitValue::create(value, CSSUnitType::Svmin); }
    static Ref<CSSUnitValue> svmax(double value) { return CSSUnitValue::create(value, CSSUnitType::Svmax); }
    static Ref<CSSUnitValue> lvw(double value) { return CSSUnitValue::create(value, CSSUnitType::Lvw); }
    static Ref<CSSUnitValue> lvh(double value) { return CSSUnitValue::create(value, CSSUnitType::Lvh); }
    static Ref<CSSUnitValue> lvi(double value) { return CSSUnitValue::create(value, CSSUnitType::Lvi); }
    static Ref<CSSUnitValue> lvb(double value) { return CSSUnitValue::create(value, CSSUnitType::Lvb); }
    static Ref<CSSUnitValue> lvmin(double value) { return CSSUnitValue::create(value, CSSUnitType::Lvmin); }
    static Ref<CSSUnitValue> lvmax(double value) { return CSSUnitValue::create(value, CSSUnitType::Lvmax); }
    static Ref<CSSUnitValue> dvw(double value) { return CSSUnitValue::create(value, CSSUnitType::Dvw); }
    static Ref<CSSUnitValue> dvh(double value) { return CSSUnitValue::create(value, CSSUnitType::Dvh); }
    static Ref<CSSUnitValue> dvi(double value) { return CSSUnitValue::create(value, CSSUnitType::Dvi); }
    static Ref<CSSUnitValue> dvb(double value) { return CSSUnitValue::create(value, CSSUnitType::Dvb); }
    static Ref<CSSUnitValue> dvmin(double value) { return CSSUnitValue::create(value, CSSUnitType::Dvmin); }
    static Ref<CSSUnitValue> dvmax(double value) { return CSSUnitValue::create(value, CSSUnitType::Dvmax); }


    // <angle>
    static Ref<CSSUnitValue> deg(double value) { return CSSUnitValue::create(value, CSSUnitType::Deg); }
    static Ref<CSSUnitValue> grad(double value) { return CSSUnitValue::create(value, CSSUnitType::Grad); }
    static Ref<CSSUnitValue> rad(double value) { return CSSUnitValue::create(value, CSSUnitType::Rad); }
    static Ref<CSSUnitValue> turn(double value) { return CSSUnitValue::create(value, CSSUnitType::Turn); }


    // <time>
    static Ref<CSSUnitValue> s(double value) { return CSSUnitValue::create(value, CSSUnitType::S); }
    static Ref<CSSUnitValue> ms(double value) { return CSSUnitValue::create(value, CSSUnitType::Ms); }


    // <frequency>
    static Ref<CSSUnitValue> hz(double value) { return CSSUnitValue::create(value, CSSUnitType::Hz); }
    static Ref<CSSUnitValue> kHz(double value) { return CSSUnitValue::create(value, CSSUnitType::Khz); }


    // <resolution>
    static Ref<CSSUnitValue> dpi(double value) { return CSSUnitValue::create(value, CSSUnitType::Dpi); }
    static Ref<CSSUnitValue> dpcm(double value) { return CSSUnitValue::create(value, CSSUnitType::Dpcm); }
    static Ref<CSSUnitValue> dppx(double value) { return CSSUnitValue::create(value, CSSUnitType::Dppx); }


    // <flex>
    static Ref<CSSUnitValue> fr(double value) { return CSSUnitValue::create(value, CSSUnitType::Fr); }


private:
    static CSSNumericFactory* from(DOMCSSNamespace&);
    static ASCIILiteral supplementName() { return "CSSNumericFactory"_s; }
    bool isCSSNumericFactory() const final { return true; }
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CSSNumericFactory)
    static bool isType(const WebCore::SupplementBase& supplement) { return supplement.isCSSNumericFactory(); }
SPECIALIZE_TYPE_TRAITS_END()
