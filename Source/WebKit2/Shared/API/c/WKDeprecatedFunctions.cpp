/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "APIDictionary.h"
#include "APIUserContentExtension.h"
#include "WKAPICast.h"
#include "WKArray.h"
#include "WKContextPrivate.h"
#include "WKMutableDictionary.h"
#include "WKPreferencesRefPrivate.h"
#include "WebPageGroup.h"
#include "WebUserContentControllerProxy.h"

#if PLATFORM(MAC)
#include "WKContextPrivateMac.h"
#endif

// Deprecated functions that should be removed from the framework once nobody uses them.

extern "C" {
WK_EXPORT WKStringRef WKPageGroupCopyIdentifier(WKPageGroupRef pageGroup);
WK_EXPORT void WKPageGroupAddUserContentFilter(WKPageGroupRef pageGroup, WKUserContentFilterRef userContentFilter);
WK_EXPORT void WKPageGroupRemoveUserContentFilter(WKPageGroupRef pageGroup, WKStringRef userContentFilterName);
WK_EXPORT void WKPageGroupRemoveAllUserContentFilters(WKPageGroupRef pageGroup);
}

using namespace WebKit;

void WKContextSetUsesNetworkProcess(WKContextRef, bool)
{
}

void WKContextSetProcessModel(WKContextRef, WKProcessModel)
{
}

WKStringRef WKPageGroupCopyIdentifier(WKPageGroupRef)
{
    return nullptr;
}

void WKPageGroupAddUserContentFilter(WKPageGroupRef pageGroupRef, WKUserContentFilterRef userContentFilterRef)
{
#if ENABLE(CONTENT_EXTENSIONS)
    toImpl(pageGroupRef)->userContentController().addUserContentExtension(*toImpl(userContentFilterRef));
#else
    UNUSED_PARAM(pageGroupRef);
    UNUSED_PARAM(userContentFilterRef);
#endif
}

void WKPageGroupRemoveUserContentFilter(WKPageGroupRef pageGroupRef, WKStringRef userContentFilterNameRef)
{
#if ENABLE(CONTENT_EXTENSIONS)
    toImpl(pageGroupRef)->userContentController().removeUserContentExtension(toWTFString(userContentFilterNameRef));
#else
    UNUSED_PARAM(pageGroupRef);
    UNUSED_PARAM(userContentFilterNameRef);
#endif
}

void WKPageGroupRemoveAllUserContentFilters(WKPageGroupRef pageGroupRef)
{
#if ENABLE(CONTENT_EXTENSIONS)
    toImpl(pageGroupRef)->userContentController().removeAllUserContentExtensions();
#else
    UNUSED_PARAM(pageGroupRef);
#endif
}
