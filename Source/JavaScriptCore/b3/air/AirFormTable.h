/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#if ENABLE(B3_JIT)

#include "AirArg.h"

namespace JSC { namespace B3 { namespace Air {

static constexpr uint8_t formRoleShift = 0;
static constexpr uint8_t formRoleMask = 15;
static constexpr uint8_t formBankShift = 4;
static constexpr uint8_t formBankMask = 1;
static constexpr uint8_t formWidthShift = 5;
static constexpr uint8_t formWidthMask = 3;
static constexpr uint8_t formInvalidShift = 7;

#define ENCODE_INST_FORM(role, bank, width) (static_cast<uint8_t>(role) << formRoleShift | static_cast<uint8_t>(bank) << formBankShift | static_cast<uint8_t>(width) << formWidthShift)

#define INVALID_INST_FORM (1 << formInvalidShift)

JS_EXPORT_PRIVATE extern const uint8_t g_formTable[];

inline Arg::Role decodeFormRole(uint8_t value)
{
    return static_cast<Arg::Role>((value >> formRoleShift) & formRoleMask);
}

inline Bank decodeFormBank(uint8_t value)
{
    return static_cast<Bank>((value >> formBankShift) & formBankMask);
}

inline Width decodeFormWidth(uint8_t value)
{
    return static_cast<Width>((value >> formWidthShift) & formWidthMask);
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

