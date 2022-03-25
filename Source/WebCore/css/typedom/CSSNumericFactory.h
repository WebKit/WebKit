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

#if ENABLE(CSS_TYPED_OM)

#include "CSSUnitValue.h"
#include "Supplementable.h"

#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Document;
class DOMCSSNamespace;

class CSSNumericFactory final : public Supplement<DOMCSSNamespace> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit CSSNumericFactory(DOMCSSNamespace&) { }

    static Ref<CSSUnitValue> number(double value) { return CSSUnitValue::create(value, "number"_s); }
    static Ref<CSSUnitValue> percent(double value) { return CSSUnitValue::create(value, "percent"_s); }


    // <length>
    static Ref<CSSUnitValue> em(double value) { return CSSUnitValue::create(value, "em"_s); }
    static Ref<CSSUnitValue> ex(double value) { return CSSUnitValue::create(value, "ex"_s); }
    static Ref<CSSUnitValue> ch(double value) { return CSSUnitValue::create(value, "ch"_s); }
    static Ref<CSSUnitValue> ic(double value) { return CSSUnitValue::create(value, "ic"_s); }
    static Ref<CSSUnitValue> rem(double value) { return CSSUnitValue::create(value, "rem"_s); }
    static Ref<CSSUnitValue> lh(double value) { return CSSUnitValue::create(value, "lh"_s); }
    static Ref<CSSUnitValue> rlh(double value) { return CSSUnitValue::create(value, "rlh"_s); }
    static Ref<CSSUnitValue> vw(double value) { return CSSUnitValue::create(value, "vw"_s); }
    static Ref<CSSUnitValue> vh(double value) { return CSSUnitValue::create(value, "vh"_s); }
    static Ref<CSSUnitValue> vi(double value) { return CSSUnitValue::create(value, "vi"_s); }
    static Ref<CSSUnitValue> vb(double value) { return CSSUnitValue::create(value, "vb"_s); }
    static Ref<CSSUnitValue> vmin(double value) { return CSSUnitValue::create(value, "vmin"_s); }
    static Ref<CSSUnitValue> vmax(double value) { return CSSUnitValue::create(value, "vmax"_s); }
    static Ref<CSSUnitValue> cm(double value) { return CSSUnitValue::create(value, "cm"_s); }
    static Ref<CSSUnitValue> mm(double value) { return CSSUnitValue::create(value, "mm"_s); }
    static Ref<CSSUnitValue> q(double value) { return CSSUnitValue::create(value, "q"_s); }
    static Ref<CSSUnitValue> in(double value) { return CSSUnitValue::create(value, "in"_s); }
    static Ref<CSSUnitValue> pt(double value) { return CSSUnitValue::create(value, "pt"_s); }
    static Ref<CSSUnitValue> pc(double value) { return CSSUnitValue::create(value, "pc"_s); }
    static Ref<CSSUnitValue> px(double value) { return CSSUnitValue::create(value, "px"_s); }
    static Ref<CSSUnitValue> cqw(double value) { return CSSUnitValue::create(value, "cqw"_s); }
    static Ref<CSSUnitValue> cqh(double value) { return CSSUnitValue::create(value, "cqh"_s); }
    static Ref<CSSUnitValue> cqi(double value) { return CSSUnitValue::create(value, "cqi"_s); }
    static Ref<CSSUnitValue> cqb(double value) { return CSSUnitValue::create(value, "cqb"_s); }
    static Ref<CSSUnitValue> cqmin(double value) { return CSSUnitValue::create(value, "cqmin"_s); }
    static Ref<CSSUnitValue> cqmax(double value) { return CSSUnitValue::create(value, "cqmax"_s); }


    // <angle>
    static Ref<CSSUnitValue> deg(double value) { return CSSUnitValue::create(value, "deg"_s); }
    static Ref<CSSUnitValue> grad(double value) { return CSSUnitValue::create(value, "grad"_s); }
    static Ref<CSSUnitValue> rad(double value) { return CSSUnitValue::create(value, "rad"_s); }
    static Ref<CSSUnitValue> turn(double value) { return CSSUnitValue::create(value, "turn"_s); }


    // <time>
    static Ref<CSSUnitValue> s(double value) { return CSSUnitValue::create(value, "s"_s); }
    static Ref<CSSUnitValue> ms(double value) { return CSSUnitValue::create(value, "ms"_s); }


    // <frequency>
    static Ref<CSSUnitValue> hz(double value) { return CSSUnitValue::create(value, "hz"_s); }
    static Ref<CSSUnitValue> kHz(double value) { return CSSUnitValue::create(value, "khz"_s); }


    // <resolution>
    static Ref<CSSUnitValue> dpi(double value) { return CSSUnitValue::create(value, "dpi"_s); }
    static Ref<CSSUnitValue> dpcm(double value) { return CSSUnitValue::create(value, "dpcm"_s); }
    static Ref<CSSUnitValue> dppx(double value) { return CSSUnitValue::create(value, "dppx"_s); }


    // <flex>
    static Ref<CSSUnitValue> fr(double value) { return CSSUnitValue::create(value, "fr"_s); }


private:
    static CSSNumericFactory* from(DOMCSSNamespace&);
    static const char* supplementName();
};

} // namespace WebCore
#endif
