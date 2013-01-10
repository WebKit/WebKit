/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WebWindowFeatures_h
#define WebWindowFeatures_h

#include "platform/WebCommon.h"
#include "platform/WebString.h"
#include "platform/WebVector.h"

#if WEBKIT_IMPLEMENTATION
#include "WindowFeatures.h"
#endif

namespace WebKit {

struct WebWindowFeatures {
    float x;
    bool xSet;
    float y;
    bool ySet;
    float width;
    bool widthSet;
    float height;
    bool heightSet;

    bool menuBarVisible;
    bool statusBarVisible;
    bool toolBarVisible;
    bool locationBarVisible;
    bool scrollbarsVisible;
    bool resizable;

    bool fullscreen;
    bool dialog;
    WebVector<WebString> additionalFeatures;

    WebWindowFeatures()
        : xSet(false)
        , ySet(false)
        , widthSet(false)
        , heightSet(false)
        , menuBarVisible(true)
        , statusBarVisible(true)
        , toolBarVisible(true)
        , locationBarVisible(true)
        , scrollbarsVisible(true)
        , resizable(true)
        , fullscreen(false)
        , dialog(false)
    {
    }


#if WEBKIT_IMPLEMENTATION
    WebWindowFeatures(const WebCore::WindowFeatures& f)
        : x(f.x)
        , xSet(f.xSet)
        , y(f.y)
        , ySet(f.ySet)
        , width(f.width)
        , widthSet(f.widthSet)
        , height(f.height)
        , heightSet(f.heightSet)
        , menuBarVisible(f.menuBarVisible)
        , statusBarVisible(f.statusBarVisible)
        , toolBarVisible(f.toolBarVisible)
        , locationBarVisible(f.locationBarVisible)
        , scrollbarsVisible(f.scrollbarsVisible)
        , resizable(f.resizable)
        , fullscreen(f.fullscreen)
        , dialog(f.dialog)
        , additionalFeatures(f.additionalFeatures)
    {
    }
#endif
};

} // namespace WebKit

#endif
