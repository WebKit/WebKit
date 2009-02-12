/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "WebIconFetcher.h"

#import "WebFrameInternal.h"
#import "WebIconFetcherInternal.h"

#import <WebCore/Frame.h>
#import <WebCore/IconFetcher.h>
#import <WebCore/SharedBuffer.h>
#import <wtf/PassRefPtr.h>

using namespace WebCore;

class WebIconFetcherClient : public IconFetcherClient {
public:
    WebIconFetcherClient(id target, SEL selector)
        : m_target(target)
        , m_selector(selector)
    {
    }

    virtual void finishedFetchingIcon(PassRefPtr<SharedBuffer> iconData)
    {
        RetainPtr<NSData> data;
        if (iconData)
            data = iconData->createNSData();
        
        [m_target performSelector:m_selector withObject:m_fetcher.get() withObject:data.get()];

        delete this;
    }

    void setFetcher(WebIconFetcher *fetcher) { m_fetcher = fetcher; }
    
private:
    RetainPtr<WebIconFetcher> m_fetcher;
    id m_target;
    SEL m_selector;
};

@implementation WebIconFetcher

- (id)init
{
    return nil;
}

- (void)dealloc
{
    if (_private)
        reinterpret_cast<IconFetcher*>(_private)->deref();
    
    [super dealloc];
}

- (void)finalize
{
    if (_private)
        reinterpret_cast<IconFetcher*>(_private)->deref();
    
    [super finalize];
}

- (void)cancel
{
    reinterpret_cast<IconFetcher*>(_private)->cancel();
}

@end

@implementation WebIconFetcher (WebInternal)

- (id)_initWithIconFetcher:(PassRefPtr<IconFetcher>)iconFetcher client:(WebIconFetcherClient *)client
{
    ASSERT(iconFetcher);
    
    self = [super init];
    if (!self)
        return nil;

    client->setFetcher(self);
    _private = reinterpret_cast<WebIconFetcherPrivate*>(iconFetcher.releaseRef());

    return self;
}

+ (WebIconFetcher *)_fetchApplicationIconForFrame:(WebFrame *)webFrame
                                           target:(id)target
                                         selector:(SEL)selector
{
    Frame* frame = core(webFrame);
    
    WebIconFetcherClient* client = new WebIconFetcherClient(target, selector);
    
    RefPtr<IconFetcher> fetcher = IconFetcher::create(frame, client);
    
    if (!fetcher)
        return nil;
    
    return [[[WebIconFetcher alloc] _initWithIconFetcher:fetcher.release() client:client] autorelease];
}

@end

