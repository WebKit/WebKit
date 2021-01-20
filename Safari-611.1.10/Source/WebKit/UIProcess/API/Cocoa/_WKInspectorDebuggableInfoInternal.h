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

#import "_WKInspectorDebuggableInfo.h"

#import "APIDebuggableInfo.h"
#import "WKObject.h"

namespace WebKit {

template<> struct WrapperTraits<API::DebuggableInfo> {
    using WrapperClass = _WKInspectorDebuggableInfo;
};

}

@interface _WKInspectorDebuggableInfo () <WKObject> {
@package
    API::ObjectStorage<API::DebuggableInfo> _debuggableInfo;
}
@end

inline Inspector::DebuggableType fromWKInspectorDebuggableType(_WKInspectorDebuggableType debuggableType)
{
    switch (debuggableType) {
    case _WKInspectorDebuggableTypeITML:
        return Inspector::DebuggableType::ITML;
    case _WKInspectorDebuggableTypeJavaScript:
        return Inspector::DebuggableType::JavaScript;
    case _WKInspectorDebuggableTypePage:
        return Inspector::DebuggableType::Page;
    case _WKInspectorDebuggableTypeServiceWorker:
        return Inspector::DebuggableType::ServiceWorker;
    case _WKInspectorDebuggableTypeWebPage:
        return Inspector::DebuggableType::WebPage;
    }

    ASSERT_NOT_REACHED();
    return Inspector::DebuggableType::JavaScript;
}

inline _WKInspectorDebuggableType toWKInspectorDebuggableType(Inspector::DebuggableType debuggableType)
{
    switch (debuggableType) {
    case Inspector::DebuggableType::ITML:
        return _WKInspectorDebuggableTypeITML;
    case Inspector::DebuggableType::JavaScript:
        return _WKInspectorDebuggableTypeJavaScript;
    case Inspector::DebuggableType::Page:
        return _WKInspectorDebuggableTypePage;
    case Inspector::DebuggableType::ServiceWorker:
        return _WKInspectorDebuggableTypeServiceWorker;
    case Inspector::DebuggableType::WebPage:
        return _WKInspectorDebuggableTypeWebPage;
    }

    ASSERT_NOT_REACHED();
    return _WKInspectorDebuggableTypeJavaScript;
}
