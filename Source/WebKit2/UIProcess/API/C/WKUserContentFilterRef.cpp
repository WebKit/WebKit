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

#include "config.h"
#include "WKUserContentFilterRef.h"

#include "APIUserContentExtension.h"
#include "WKAPICast.h"
#include "WebCompiledContentExtension.h"
#include <WebCore/ContentExtensionCompiler.h>
#include <WebCore/ContentExtensionError.h>

using namespace WebKit;

WKTypeID WKUserContentFilterGetTypeID()
{
    return toAPI(API::UserContentExtension::APIType);
}

WKUserContentFilterRef WKUserContentFilterCreate(WKStringRef nameRef, WKStringRef serializedRulesRef)
{
#if ENABLE(CONTENT_EXTENSIONS)
    WebCore::ContentExtensions::CompiledContentExtensionData data;
    LegacyContentExtensionCompilationClient client(data);

    auto compilerError = WebCore::ContentExtensions::compileRuleList(client, toWTFString(serializedRulesRef));
    if (compilerError)
        return nullptr;

    auto compiledContentExtension = WebKit::WebCompiledContentExtension::createFromCompiledContentExtensionData(data);

    return toAPI(&API::UserContentExtension::create(toWTFString(nameRef), WTF::move(compiledContentExtension)).leakRef());
#else
    UNUSED_PARAM(nameRef);
    UNUSED_PARAM(serializedRulesRef);
    return nullptr;
#endif
}
