/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
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

#include <JavaScriptCore/FPRInfo.h>
#include <JavaScriptCore/GPRInfo.h>
#include <JavaScriptCore/Reg.h>

#if ENABLE(JIT)

namespace JSC {

// SnippetReg is a polymorphic register class. It can refer to an FPRReg, a GPRReg, or a
// "JSValueRegs". Note that isGPR() returns false when the target Reg is "JSValueRegs".
//
// FIXME: Eventually we should move this class into JSC and make is available for other JIT code.
// https://bugs.webkit.org/show_bug.cgi?id=162990
class SnippetReg {
public:
    enum class Type : uint8_t {
        GPR = 0,
        FPR = 1,
        JSValue = 2,
    };

    SnippetReg(GPRReg reg)
        : m_variant(reg)
    {
    }

    SnippetReg(FPRReg reg)
        : m_variant(reg)
    {
    }

    SnippetReg(JSValueRegs regs)
        : m_variant(regs)
    {
    }

    bool isGPR() const { return m_variant.index() == static_cast<unsigned>(Type::GPR); }
    bool isFPR() const { return m_variant.index() == static_cast<unsigned>(Type::FPR); }
    bool isJSValueRegs() const { return m_variant.index() == static_cast<unsigned>(Type::JSValue); }

    GPRReg gpr() const
    {
        ASSERT(isGPR());
        return std::get<GPRReg>(m_variant);
    }
    FPRReg fpr() const
    {
        ASSERT(isFPR());
        return std::get<FPRReg>(m_variant);
    }
    JSValueRegs jsValueRegs() const
    {
        ASSERT(isJSValueRegs());
        return std::get<JSValueRegs>(m_variant);
    }

private:
    Variant<GPRReg, FPRReg, JSValueRegs> m_variant;
};

}

#endif
