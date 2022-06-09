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

    static Ref<CSSUnitValue> number(double value) { return CSSUnitValue::create(value, "number"); }
    static Ref<CSSUnitValue> percent(double value) { return CSSUnitValue::create(value, "percent"); }


    // <length>
    static Ref<CSSUnitValue> em(double value) { return CSSUnitValue::create(value, "em"); }
    static Ref<CSSUnitValue> ex(double value) { return CSSUnitValue::create(value, "ex"); }
    static Ref<CSSUnitValue> ch(double value) { return CSSUnitValue::create(value, "ch"); }
    static Ref<CSSUnitValue> ic(double value) { return CSSUnitValue::create(value, "ic"); }
    static Ref<CSSUnitValue> rem(double value) { return CSSUnitValue::create(value, "rem"); }
    static Ref<CSSUnitValue> lh(double value) { return CSSUnitValue::create(value, "lh"); }
    static Ref<CSSUnitValue> rlh(double value) { return CSSUnitValue::create(value, "rlh"); }
    static Ref<CSSUnitValue> vw(double value) { return CSSUnitValue::create(value, "vw"); }
    static Ref<CSSUnitValue> vh(double value) { return CSSUnitValue::create(value, "vh"); }
    static Ref<CSSUnitValue> vi(double value) { return CSSUnitValue::create(value, "vi"); }
    static Ref<CSSUnitValue> vb(double value) { return CSSUnitValue::create(value, "vb"); }
    static Ref<CSSUnitValue> vmin(double value) { return CSSUnitValue::create(value, "vmin"); }
    static Ref<CSSUnitValue> vmax(double value) { return CSSUnitValue::create(value, "vmax"); }
    static Ref<CSSUnitValue> cm(double value) { return CSSUnitValue::create(value, "cm"); }
    static Ref<CSSUnitValue> mm(double value) { return CSSUnitValue::create(value, "mm"); }
    static Ref<CSSUnitValue> q(double value) { return CSSUnitValue::create(value, "q"); }
    static Ref<CSSUnitValue> in(double value) { return CSSUnitValue::create(value, "in"); }
    static Ref<CSSUnitValue> pt(double value) { return CSSUnitValue::create(value, "pt"); }
    static Ref<CSSUnitValue> pc(double value) { return CSSUnitValue::create(value, "pc"); }
    static Ref<CSSUnitValue> px(double value) { return CSSUnitValue::create(value, "px"); }


    // <angle>
    static Ref<CSSUnitValue> deg(double value) { return CSSUnitValue::create(value, "deg"); }
    static Ref<CSSUnitValue> grad(double value) { return CSSUnitValue::create(value, "grad"); }
    static Ref<CSSUnitValue> rad(double value) { return CSSUnitValue::create(value, "rad"); }
    static Ref<CSSUnitValue> turn(double value) { return CSSUnitValue::create(value, "turn"); }


    // <time>
    static Ref<CSSUnitValue> s(double value) { return CSSUnitValue::create(value, "s"); }
    static Ref<CSSUnitValue> ms(double value) { return CSSUnitValue::create(value, "ms"); }


    // <frequency>
    static Ref<CSSUnitValue> hz(double value) { return CSSUnitValue::create(value, "hz"); }
    static Ref<CSSUnitValue> kHz(double value) { return CSSUnitValue::create(value, "khz"); }


    // <resolution>
    static Ref<CSSUnitValue> dpi(double value) { return CSSUnitValue::create(value, "dpi"); }
    static Ref<CSSUnitValue> dpcm(double value) { return CSSUnitValue::create(value, "dpcm"); }
    static Ref<CSSUnitValue> dppx(double value) { return CSSUnitValue::create(value, "dppx"); }


    // <flex>
    static Ref<CSSUnitValue> fr(double value) { return CSSUnitValue::create(value, "fr"); }


private:
    static CSSNumericFactory* from(DOMCSSNamespace&);
    static const char* supplementName();
};

} // namespace WebCore
#endif
