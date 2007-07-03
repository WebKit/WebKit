/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebDocumentLoaderMac.h"

#import "WebView.h"

using namespace WebCore;

WebDocumentLoaderMac::WebDocumentLoaderMac(const ResourceRequest& request, const SubstituteData& substituteData)
    : DocumentLoader(request, substituteData)
    , m_dataSource(nil)
    , m_isDataSourceRetained(false)
{
}

static inline bool needsAppKitWorkaround(WebView *webView)
{
#ifndef BUILDING_ON_TIGER
    return false;
#else    
    id frameLoadDelegate = [webView frameLoadDelegate];
    if (!frameLoadDelegate)
        return false;
    
    NSString *bundleIdentifier = [[NSBundle bundleForClass:[frameLoadDelegate class]] bundleIdentifier];
    return [bundleIdentifier isEqualToString:@"com.apple.AppKit"];
#endif
}

void WebDocumentLoaderMac::setDataSource(WebDataSource *dataSource, WebView *webView)
{
    ASSERT(!m_dataSource);
    ASSERT(!m_isDataSourceRetained);

    m_dataSource = dataSource;
    retainDataSource();

    m_resourceLoadDelegate = [webView resourceLoadDelegate];
    m_downloadDelegate = [webView downloadDelegate];
    
    // Work around a bug in Tiger AppKit's use of WebKit. The particular idiom used
    // won't allow the timer to run, so deferring the main resource load with a timer
    // causes a delay until something else wakes the run loop.
    // See <rdar://problem/5266289>
    if (needsAppKitWorkaround(webView))
        m_deferMainResourceDataLoad = false;
}

WebDataSource *WebDocumentLoaderMac::dataSource() const
{
    return m_dataSource;
}

void WebDocumentLoaderMac::attachToFrame()
{
    DocumentLoader::attachToFrame();

    retainDataSource();
}

void WebDocumentLoaderMac::detachFromFrame()
{
    DocumentLoader::detachFromFrame();

    if (m_loadingResources.isEmpty())
        releaseDataSource();

    // FIXME: What prevents the data source from getting deallocated while the
    // frame is not attached?
}

void WebDocumentLoaderMac::increaseLoadCount(unsigned long identifier)
{
    ASSERT(m_dataSource);

    if (m_loadingResources.contains(identifier))
        return;
    m_loadingResources.add(identifier);

    retainDataSource();
}

void WebDocumentLoaderMac::decreaseLoadCount(unsigned long identifier)
{
    ASSERT(m_loadingResources.contains(identifier));
    m_loadingResources.remove(identifier);
    
    if (m_loadingResources.isEmpty()) {
        m_resourceLoadDelegate = 0;
        m_downloadDelegate = 0;
        if (!frame())
            releaseDataSource();
    }
}

void WebDocumentLoaderMac::retainDataSource()
{
    if (m_isDataSourceRetained || !m_dataSource)
        return;
    m_isDataSourceRetained = true;
    CFRetain(m_dataSource);
}

void WebDocumentLoaderMac::releaseDataSource()
{
    if (!m_isDataSourceRetained)
        return;
    ASSERT(m_dataSource);
    m_isDataSourceRetained = false;
    CFRelease(m_dataSource);
}

void WebDocumentLoaderMac::detachDataSource()
{
    ASSERT(!m_isDataSourceRetained);
    m_dataSource = nil;
}
