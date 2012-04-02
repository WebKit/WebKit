// Copyright (C) 2011, Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.

#ifndef WebGamepads_h
#define WebGamepads_h

#include "WebGamepad.h"

#if WEBKIT_IMPLEMENTATION
#include <wtf/Assertions.h>
#endif

namespace WebKit {

#pragma pack(push, 1)

// This structure is intentionally POD and fixed size so that it can be stored
// in shared memory between hardware polling threads and the rest of the
// browser.
class WebGamepads {
public:
    WebGamepads()
        : length(0) { }

    static const size_t itemsLengthCap = 4;

    // Number of valid entries in the items array.
    unsigned length;

    // Gamepad data for N separate gamepad devices.
    WebGamepad items[itemsLengthCap];
};

#if WEBKIT_IMPLEMENTATION
COMPILE_ASSERT(sizeof(WebGamepads) == 1864, WebGamepads_has_wrong_size);
#endif

#pragma pack(pop)

}

#endif // WebGamepads_h
