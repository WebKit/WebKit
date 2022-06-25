/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(FULLSCREEN_API)

#include "Document.h"
#include "FullscreenManager.h"
#include <wtf/Forward.h>

namespace WebCore {

class Document;
class Element;

class DocumentFullscreen {
public:
    static bool webkitFullscreenEnabled(Document& document) { return document.fullscreenManager().isFullscreenEnabled(); }
    static Element* webkitFullscreenElement(Document& document) { return document.ancestorElementInThisScope(document.fullscreenManager().fullscreenElement()); }
    static void webkitExitFullscreen(Document& document) { document.fullscreenManager().exitFullscreen(); }
    static bool webkitIsFullScreen(Document& document) { return document.fullscreenManager().isFullscreen(); }
    static bool webkitFullScreenKeyboardInputAllowed(Document& document) { return document.fullscreenManager().isFullscreenKeyboardInputAllowed(); }
    static Element* webkitCurrentFullScreenElement(Document& document) { return document.ancestorElementInThisScope(document.fullscreenManager().currentFullscreenElement()); }
    static void webkitCancelFullScreen(Document& document) { document.fullscreenManager().cancelFullscreen(); }
};

}

#endif
