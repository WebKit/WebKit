/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef SelectionState_h
#define SelectionState_h

#include "ArgumentCoders.h"
#include <wtf/NotFound.h>

namespace WebKit {

struct SelectionState {
    SelectionState()
        : isNone(true)
        , isContentEditable(false)
        , isInPasswordField(false)
        , hasComposition(false)
        , selectedRangeStart(notFound)
        , selectedRangeLength(0)
    {
    }

    // Whether there is a selection at all. This will be false when there is a caret selection.
    bool isNone;

    // Whether the selection is in a content editable area.
    bool isContentEditable;

    // Whether the selection is in a password field.
    bool isInPasswordField;

    // Whether the selection has a composition.
    bool hasComposition;

    // The start of the selected range.
    uint64_t selectedRangeStart;
    
    // The length of the selected range.
    uint64_t selectedRangeLength;
};

} // namespace WebKit

namespace CoreIPC {
template<> struct ArgumentCoder<WebKit::SelectionState> : SimpleArgumentCoder<WebKit::SelectionState> { };
};

#endif // SelectionState_h
