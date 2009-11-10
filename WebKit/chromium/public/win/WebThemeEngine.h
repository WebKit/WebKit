/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebThemeEngine_h
#define WebThemeEngine_h

#include "../WebCanvas.h"
#include "../WebColor.h"

namespace WebKit {

struct WebRect;

// The part and state parameters correspond to values defined by the
// Windows Theme API (see
// http://msdn.microsoft.com/en-us/library/bb773187(VS.85).aspx ).
// The classicState parameter corresponds to the uState
// parameter of the Windows DrawFrameControl() function.
// See the definitions in <vsstyle.h> and <winuser.h>.
class WebThemeEngine {
public:
    virtual void paintButton(
        WebCanvas*, int part, int state, int classicState,
        const WebRect&) = 0;

    virtual void paintMenuList(
        WebCanvas*, int part, int state, int classicState,
        const WebRect&) = 0;

    virtual void paintScrollbarArrow(
        WebCanvas*, int state, int classicState,
        const WebRect&) = 0;

    virtual void paintScrollbarThumb(
        WebCanvas*, int part, int state, int classicState,
        const WebRect&) = 0;

    virtual void paintScrollbarTrack(
        WebCanvas*, int part, int state, int classicState,
        const WebRect&, const WebRect& alignRect) = 0;

    virtual void paintTextField(
        WebCanvas*, int part, int state, int classicState,
        const WebRect&, WebColor, bool fillContentArea, bool drawEdges) = 0;

    virtual void paintTrackbar(
        WebCanvas*, int part, int state, int classicState,
        const WebRect&) = 0;
};

} // namespace WebKit

#endif
