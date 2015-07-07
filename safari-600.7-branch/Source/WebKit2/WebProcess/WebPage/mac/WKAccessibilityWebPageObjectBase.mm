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

#import "config.h"
#import "WKAccessibilityWebPageObjectBase.h"

#import "WebFrame.h"
#import "WebPage.h"
#import "WKArray.h"
#import "WKNumber.h"
#import "WKRetainPtr.h"
#import "WKSharedAPICast.h"
#import "WKString.h"
#import "WKStringCF.h"
#import <WebCore/AXObjectCache.h>
#import <WebCore/Document.h>
#import <WebCore/FrameView.h>
#import <WebCore/MainFrame.h>
#import <WebCore/Page.h>
#import <WebCore/ScrollView.h>
#import <WebCore/Scrollbar.h>
#import <WebKitSystemInterface.h>
#import <wtf/ObjcRuntimeExtras.h>

using namespace WebCore;
using namespace WebKit;

@implementation WKAccessibilityWebPageObjectBase

- (id)accessibilityRootObjectWrapper
{
    if (!WebCore::AXObjectCache::accessibilityEnabled())
        WebCore::AXObjectCache::enableAccessibility();

    NSObject* mainFramePluginAccessibilityObjectWrapper = m_page->accessibilityObjectForMainFramePlugin();
    if (mainFramePluginAccessibilityObjectWrapper)
        return mainFramePluginAccessibilityObjectWrapper;

    WebCore::Page* page = m_page->corePage();
    if (!page)
        return nil;
    
    WebCore::Frame& core = page->mainFrame();
    if (!core.document())
        return nil;
    
    WebCore::AXObjectCache* cache = core.document()->axObjectCache();
    if (!cache)
        return nil;
    
    if (AccessibilityObject* root = cache->rootObject())
        return root->wrapper();
    
    return nil;
}

- (void)setWebPage:(WebPage*)page
{
    m_page = page;
}

- (void)setRemoteParent:(id)parent
{
    if (parent != m_parent) {
        [m_parent release];
        m_parent = [parent retain];
    }
}

- (id)accessibilityFocusedUIElement
{
    return [[self accessibilityRootObjectWrapper] accessibilityFocusedUIElement];
}


@end
