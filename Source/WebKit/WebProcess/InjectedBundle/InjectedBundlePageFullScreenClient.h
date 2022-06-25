/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef InjectedBundlePageFullScreenClient_h
#define InjectedBundlePageFullScreenClient_h

#if ENABLE(FULLSCREEN_API)

#include "APIClient.h"
#include "WKBundlePageFullScreenClient.h"
#include <wtf/Forward.h>

namespace API {
template<> struct ClientTraits<WKBundlePageFullScreenClientBase> {
    typedef std::tuple<WKBundlePageFullScreenClientV0, WKBundlePageFullScreenClientV1> Versions;
};
}

namespace WebCore {
class Element;
class IntRect;
}

namespace WebKit {

class WebPage;

class InjectedBundlePageFullScreenClient : public API::Client<WKBundlePageFullScreenClientBase> {
public:
    bool supportsFullScreen(WebPage*, bool withKeyboard);
    void enterFullScreenForElement(WebPage*, WebCore::Element*, bool blocksReturnToFullscreenFromPictureInPicture);
    void exitFullScreenForElement(WebPage*, WebCore::Element*);
    void beganEnterFullScreen(WebPage*, WebCore::IntRect& initialFrame, WebCore::IntRect& finalFrame);
    void beganExitFullScreen(WebPage*, WebCore::IntRect& initialFrame, WebCore::IntRect& finalFrame);
    void closeFullScreen(WebPage*);
};

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)

#endif // InjectedBundlePageFullScreenClient_h
