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

#import "WebPlatformStrategies.h"

#import "WebPluginDatabase.h"
#import "WebPluginPackage.h"
#import <WebCore/BlockExceptions.h>
#import <WebCore/Page.h>
#import <WebCore/PageGroup.h>

using namespace WebCore;

void WebPlatformStrategies::initialize()
{
    DEFINE_STATIC_LOCAL(WebPlatformStrategies, platformStrategies, ());
    setPlatformStrategies(&platformStrategies);
}

WebPlatformStrategies::WebPlatformStrategies()
{
}

CookiesStrategy* WebPlatformStrategies::createCookiesStrategy()
{
    return this;
}

PluginStrategy* WebPlatformStrategies::createPluginStrategy()
{
    return this;
}

VisitedLinkStrategy* WebPlatformStrategies::createVisitedLinkStrategy()
{
    return this;
}

void WebPlatformStrategies::notifyCookiesChanged()
{
}

void WebPlatformStrategies::refreshPlugins()
{
    [[WebPluginDatabase sharedDatabase] refresh];
}

void WebPlatformStrategies::getPluginInfo(const WebCore::Page*, Vector<WebCore::PluginInfo>& plugins)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSArray* pluginsArray = [[WebPluginDatabase sharedDatabase] plugins];
    for (unsigned int i = 0; i < [pluginsArray count]; ++i) {
        WebPluginPackage *plugin = [pluginsArray objectAtIndex:i];

        plugins.append([plugin pluginInfo]);
    }
    
    END_BLOCK_OBJC_EXCEPTIONS;
}

bool WebPlatformStrategies::isLinkVisited(Page* page, LinkHash hash)
{
    return page->group().isLinkVisited(hash);
}

void WebPlatformStrategies::addVisitedLink(Page* page, LinkHash hash)
{
    return page->group().addVisitedLinkHash(hash);
}
