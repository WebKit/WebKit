/*
 * Copyright (C) 2013 University of Washington. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights resernved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ReplayInputTypes_h
#define ReplayInputTypes_h

#if ENABLE(WEB_REPLAY)

#include "ThreadGlobalData.h"
#include "WebReplayInputs.h"
#include <JavaScriptCore/JSReplayInputs.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class ReplayInputTypes {
    WTF_MAKE_NONCOPYABLE(ReplayInputTypes); WTF_MAKE_FAST_ALLOCATED;
    int dummy; // Needed to make initialization macro work.
public:
    ReplayInputTypes();

#define DECLARE_REPLAY_INPUT_TYPES(name) AtomicString name;
    JS_REPLAY_INPUT_NAMES_FOR_EACH(DECLARE_REPLAY_INPUT_TYPES)
    WEB_REPLAY_INPUT_NAMES_FOR_EACH(DECLARE_REPLAY_INPUT_TYPES)
    DECLARE_REPLAY_INPUT_TYPES(MemoizedDOMResult);
#undef DECLARE_REPLAY_INPUT_TYPES
};

inline ReplayInputTypes& inputTypes()
{
    return threadGlobalData().inputTypes();
}

} // namespace WebCore

#endif // ENABLE(WEB_REPLAY)

#endif // ReplayInputTypes_h
