/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCQuadSink_h
#define CCQuadSink_h

#include <wtf/PassOwnPtr.h>

namespace WebCore {

class CCDrawQuad;

struct CCAppendQuadsData;
struct CCSharedQuadState;

class CCQuadSink {
public:
    virtual ~CCQuadSink() { }

    // Call this to add a SharedQuadState before appending quads that refer to it. Returns a pointer
    // to the given SharedQuadState for convenience, that can be set on the quads to append.
    virtual CCSharedQuadState* useSharedQuadState(PassOwnPtr<CCSharedQuadState>) = 0;

    // Returns true if the quad is added to the list, and false if the quad is entirely culled.
    virtual bool append(PassOwnPtr<CCDrawQuad> passDrawQuad, CCAppendQuadsData&) = 0;
};

}
#endif // CCQuadCuller_h
