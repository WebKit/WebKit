/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebFloatQuad_h
#define WebFloatQuad_h

#include "WebCommon.h"
#include "WebFloatPoint.h"
#include "WebRect.h"

#include <algorithm>
#include <cmath>

#if WEBKIT_IMPLEMENTATION
#include "FloatQuad.h"
#endif

namespace WebKit {

struct WebFloatQuad {
    WebFloatPoint p[4];

    WebFloatQuad()
    {
    }

    WebFloatQuad(const WebFloatPoint& p0, const WebFloatPoint& p1, const WebFloatPoint& p2, const WebFloatPoint& p3)
    {
        p[0] = p0;
        p[1] = p1;
        p[2] = p2;
        p[3] = p3;
    }

    WEBKIT_EXPORT WebRect enclosingRect() const;

#if WEBKIT_IMPLEMENTATION
    WebFloatQuad& operator=(const WebCore::FloatQuad& q)
    {
        p[0] = q.p1();
        p[1] = q.p2();
        p[2] = q.p3();
        p[3] = q.p4();
        return *this;
    }
    WebFloatQuad(const WebCore::FloatQuad& q)
    {
        *this = q;
    }
#endif
};

} // namespace WebKit

#endif
