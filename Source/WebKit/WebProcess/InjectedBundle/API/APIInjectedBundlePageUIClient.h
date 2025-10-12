/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "WebEvent.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
class HitTestResult;
}

namespace WebKit {
class InjectedBundleNodeHandle;
class WebFrame;
class WebPage;
}

namespace API {

class Object;
class SecurityOrigin;

namespace InjectedBundle {

class PageUIClient {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(PageUIClient);
public:
    virtual ~PageUIClient() { }

#if !PLATFORM(COCOA)
    virtual void willAddMessageToConsole(WebKit::WebPage*, JSC::MessageSource, JSC::MessageLevel, const WTF::String& message, unsigned lineNumber, unsigned columnNumber, const WTF::String& sourceID) { UNUSED_PARAM(message); UNUSED_PARAM(lineNumber); UNUSED_PARAM(columnNumber); UNUSED_PARAM(sourceID); }
#endif

    virtual void didClickAutoFillButton(WebKit::WebPage&, WebKit::InjectedBundleNodeHandle&, RefPtr<API::Object>&) { }
};

} // namespace InjectedBundle

} // namespace API
