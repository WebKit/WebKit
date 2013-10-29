/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Nix_Gamepad_h
#define Nix_Gamepad_h

#include "Common.h"

namespace Nix {

class Gamepad {
public:
    static const size_t idLengthCap = 128;
    static const size_t axesLengthCap = 16;
    static const size_t buttonsLengthCap = 32;

    Gamepad()
        : connected(false)
        , timestamp(0)
        , axesLength(0)
        , buttonsLength(0)
    {
        id[0] = 0;
    }

    // Is there a gamepad connected at this index?
    bool connected;

    // Device identifier (based on manufacturer, model, etc.).
    char id[idLengthCap];

    // Monotonically increasing value referring to when the data were last
    // updated.
    unsigned long long timestamp;

    // Number of valid entries in the axes array.
    unsigned axesLength;

    // Normalized values representing axes, in the range [-1..1].
    float axes[axesLengthCap];

    // Number of valid entries in the buttons array.
    unsigned buttonsLength;

    // Normalized values representing buttons, in the range [0..1].
    float buttons[buttonsLengthCap];
};

} // namespace Nix

#endif // Nix_Gamepad_h
