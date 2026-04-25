/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(GAMEPAD)

namespace WebCore {

enum KnownGamepad {
    Dualshock3 = 0x054c0268,
    Dualshock4_1 = 0x054c05c4,
    Dualshock4_2 = 0x054c09cc,
    GamesirM2 = 0x0ec20475,
    HoripadUltimate = 0x0f0d0090,
    LogitechF310 = 0x046dc216,
    LogitechF710 = 0x046dc219,
    Nimbus1 = 0x01111420,
    Nimbus2 = 0x10381420,
    StadiaA = 0x18d19400,
    StratusXL1 = 0x01111418,
    StratusXL2 = 0x10381418,
    StratusXL3 = 0x01111419,
    StratusXL4 = 0x10381419,
    XboxOne1 = 0x045e02e0,
    XboxOne2 = 0x045e02ea,
    XboxOne3 = 0x045e02fd,
};

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
