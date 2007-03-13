/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"

#if ENABLE(XPATH)

#import "DOMXPath.h"

#import "DOMInternal.h"
#import "PlatformString.h"
#import "XPathNSResolver.h"

//------------------------------------------------------------------------------------------
// DOMNativeXPathNSResolver

@implementation DOMNativeXPathNSResolver

#define IMPL reinterpret_cast<WebCore::XPathNSResolver*>(_internal)

- (void)dealloc
{
    if (_internal)
        IMPL->deref();
    [super dealloc];
}

- (void)finalize
{
    if (_internal)
        IMPL->deref();
    [super finalize];
}

- (WebCore::XPathNSResolver *)_xpathNSResolver
{
    return IMPL;
}

- (id)_initWithXPathNSResolver:(WebCore::XPathNSResolver *)impl
{
    ASSERT(impl);
    
    [super _init];
    _internal = reinterpret_cast<DOMObjectInternal*>(impl);
    impl->ref();
    WebCore::addDOMWrapper(self, impl);
    return self;    
}

+ (DOMNativeXPathNSResolver *)_wrapXPathNSResolver:(WebCore::XPathNSResolver *)impl
{
    if (!impl)
        return nil;
    
    id cachedInstance;
    cachedInstance = WebCore::getDOMWrapper(impl);
    if (cachedInstance)
        return [[cachedInstance retain] autorelease];
    
    return [[[DOMNativeXPathNSResolver alloc] _initWithXPathNSResolver:impl] autorelease];    
}

- (NSString *)lookupNamespaceURI:(NSString *)prefix
{
    return IMPL->lookupNamespaceURI(prefix);
}

@end

#endif // ENABLE(XPATH)
