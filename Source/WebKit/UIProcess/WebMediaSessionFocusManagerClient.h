/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef WebMediaSessionFocusManagerClient_h
#define WebMediaSessionFocusManagerClient_h

#if ENABLE(MEDIA_SESSION)

#include "APIClient.h"
#include "WKMediaSessionFocusManager.h"
#include <wtf/Forward.h>

namespace API {
template<> struct ClientTraits<WKMediaSessionFocusManagerClientBase> {
    typedef std::tuple<WKMediaSessionFocusManagerClientV0> Versions;
};
}

namespace WebKit {

class WebMediaSessionFocusManager;

class WebMediaSessionFocusManagerClient : public API::Client<WKMediaSessionFocusManagerClientBase> {
public:
    void didChangePlaybackAttribute(WebMediaSessionFocusManager*, WKMediaSessionFocusManagerPlaybackAttribute, bool);
};

} // namespace WebKit

#endif // ENABLE(MEDIA_SESSION)

#endif // WebMediaSessionFocusManagerClient_h
