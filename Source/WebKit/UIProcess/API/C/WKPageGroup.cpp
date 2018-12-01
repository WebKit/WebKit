/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "WKPageGroup.h"

#include "APIArray.h"
#include "APIContentRuleList.h"
#include "APIUserContentWorld.h"
#include "APIUserScript.h"
#include "APIUserStyleSheet.h"
#include "InjectUserScriptImmediately.h"
#include "WKAPICast.h"
#include "WebPageGroup.h"
#include "WebPreferences.h"
#include "WebUserContentController.h"
#include "WebUserContentControllerProxy.h"

using namespace WebKit;

WKTypeID WKPageGroupGetTypeID()
{
    return toAPI(WebPageGroup::APIType);
}

WKPageGroupRef WKPageGroupCreateWithIdentifier(WKStringRef identifier)
{
    auto pageGroup = WebPageGroup::create(toWTFString(identifier));
    return toAPI(&pageGroup.leakRef());
}

void WKPageGroupSetPreferences(WKPageGroupRef pageGroupRef, WKPreferencesRef preferencesRef)
{
    toImpl(pageGroupRef)->setPreferences(toImpl(preferencesRef));
}

WKPreferencesRef WKPageGroupGetPreferences(WKPageGroupRef pageGroupRef)
{
    return toAPI(&toImpl(pageGroupRef)->preferences());
}

WKUserContentControllerRef WKPageGroupGetUserContentController(WKPageGroupRef pageGroupRef)
{
    return toAPI(&toImpl(pageGroupRef)->userContentController());
}

void WKPageGroupAddUserStyleSheet(WKPageGroupRef pageGroupRef, WKStringRef sourceRef, WKURLRef baseURLRef, WKArrayRef whitelistedURLPatterns, WKArrayRef blacklistedURLPatterns, WKUserContentInjectedFrames injectedFrames)
{
    auto source = toWTFString(sourceRef);

    if (source.isEmpty())
        return;

    auto baseURLString = toWTFString(baseURLRef);
    auto whitelist = toImpl(whitelistedURLPatterns);
    auto blacklist = toImpl(blacklistedURLPatterns);

    Ref<API::UserStyleSheet> userStyleSheet = API::UserStyleSheet::create(WebCore::UserStyleSheet { source, (baseURLString.isEmpty() ? WTF::blankURL() :  URL(URL(), baseURLString)), whitelist ? whitelist->toStringVector() : Vector<String>(), blacklist ? blacklist->toStringVector() : Vector<String>(), toUserContentInjectedFrames(injectedFrames), WebCore::UserStyleUserLevel }, API::UserContentWorld::normalWorld());

    toImpl(pageGroupRef)->userContentController().addUserStyleSheet(userStyleSheet.get());
}

void WKPageGroupRemoveAllUserStyleSheets(WKPageGroupRef pageGroup)
{
    toImpl(pageGroup)->userContentController().removeAllUserStyleSheets();
}

void WKPageGroupAddUserScript(WKPageGroupRef pageGroupRef, WKStringRef sourceRef, WKURLRef baseURLRef, WKArrayRef whitelistedURLPatterns, WKArrayRef blacklistedURLPatterns, WKUserContentInjectedFrames injectedFrames, _WKUserScriptInjectionTime injectionTime)
{
    auto source = toWTFString(sourceRef);

    if (source.isEmpty())
        return;

    auto baseURLString = toWTFString(baseURLRef);
    auto whitelist = toImpl(whitelistedURLPatterns);
    auto blacklist = toImpl(blacklistedURLPatterns);
    
    auto url = baseURLString.isEmpty() ? WTF::blankURL() :  URL(URL(), baseURLString);
    Ref<API::UserScript> userScript = API::UserScript::create(WebCore::UserScript { WTFMove(source), WTFMove(url), whitelist ? whitelist->toStringVector() : Vector<String>(), blacklist ? blacklist->toStringVector() : Vector<String>(), toUserScriptInjectionTime(injectionTime), toUserContentInjectedFrames(injectedFrames) }, API::UserContentWorld::normalWorld());
    toImpl(pageGroupRef)->userContentController().addUserScript(userScript.get(), InjectUserScriptImmediately::No);
}

void WKPageGroupRemoveAllUserScripts(WKPageGroupRef pageGroupRef)
{
    toImpl(pageGroupRef)->userContentController().removeAllUserScripts();
}
