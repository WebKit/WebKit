/*
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 *
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebFramePrivate_h
#define WebFramePrivate_h

#include <wtf/RefPtr.h>
#include <String.h>

class BMessenger;
class BWebPage;

namespace WebCore {
class Frame;
class FrameLoaderClientHaiku;
class HTMLFrameOwnerElement;
class Page;
}

class WebFramePrivate {
public:
    WebFramePrivate(WebCore::Page* page)
        : ownerElement(nullptr)
        , page(page)
        , frame(nullptr)
    {}


    WTF::String name;
    WTF::String requestedURL;
    WebCore::HTMLFrameOwnerElement* ownerElement;
    WebCore::Page* page;
    // NOTE: We don't keep a reference pointer for the WebCore::Frame, since
    // that will leave us with one too many references, which will in turn
    // prevent the shutdown mechanism from working, since that one is only
    // triggered from the FrameLoader destructor, i.e. when there are no more
    // references around. (FrameLoader and Frame used to be one class, they
    // can be considered as one object as far as object life-time goes.)
    WebCore::Frame* frame;
};

#endif // WebFramePrivate_h
