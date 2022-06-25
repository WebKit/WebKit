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
#include "WKUserContentControllerRef.h"

#include "APIArray.h"
#include "APIContentRuleList.h"
#include "APIUserScript.h"
#include "InjectUserScriptImmediately.h"
#include "WKAPICast.h"
#include "WebUserContentControllerProxy.h"

using namespace WebKit;

WKTypeID WKUserContentControllerGetTypeID()
{
    return toAPI(WebUserContentControllerProxy::APIType);
}

WKUserContentControllerRef WKUserContentControllerCreate()
{
    return toAPI(&WebUserContentControllerProxy::create().leakRef());
}

WKArrayRef WKUserContentControllerCopyUserScripts(WKUserContentControllerRef userContentControllerRef)
{
    Ref<API::Array> userScripts = toImpl(userContentControllerRef)->userScripts().copy();
    return toAPI(&userScripts.leakRef());
}

void WKUserContentControllerAddUserScript(WKUserContentControllerRef userContentControllerRef, WKUserScriptRef userScriptRef)
{
    toImpl(userContentControllerRef)->addUserScript(*toImpl(userScriptRef), InjectUserScriptImmediately::No);
}

void WKUserContentControllerRemoveAllUserScripts(WKUserContentControllerRef userContentControllerRef)
{
    toImpl(userContentControllerRef)->removeAllUserScripts();
}

void WKUserContentControllerAddUserContentFilter(WKUserContentControllerRef userContentControllerRef, WKUserContentFilterRef userContentFilterRef)
{
#if ENABLE(CONTENT_EXTENSIONS)
    toImpl(userContentControllerRef)->addContentRuleList(*toImpl(userContentFilterRef));
#endif
}

void WKUserContentControllerRemoveAllUserContentFilters(WKUserContentControllerRef userContentControllerRef)
{
#if ENABLE(CONTENT_EXTENSIONS)
    toImpl(userContentControllerRef)->removeAllContentRuleLists();
#endif
}
