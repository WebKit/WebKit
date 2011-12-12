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
#import "WKBrowsingContextGroup.h"
#import "WKBrowsingContextGroupInternal.h"

#import "WKPageGroup.h"
#import "WKPreferences.h"
#import "WKRetainPtr.h"
#import "WKStringCF.h"

@interface WKBrowsingContextGroupData : NSObject {
@public
    WKRetainPtr<WKPageGroupRef> _pageGroupRef;
}
@end

@implementation WKBrowsingContextGroupData
@end

@implementation WKBrowsingContextGroup

- (id)initWithIdentifier:(NSString *)identifier
{
    self = [super init];
    if (!self)
        return nil;

    _data = [[WKBrowsingContextGroupData alloc] init];
    _data->_pageGroupRef = adoptWK(WKPageGroupCreateWithIdentifier(adoptWK(WKStringCreateWithCFString((CFStringRef)identifier)).get()));

    return self;
}

- (void)dealloc
{
    [_data release];
    [super dealloc];
}

- (BOOL)allowsJavaScript
{
    return WKPreferencesGetJavaScriptEnabled(WKPageGroupGetPreferences(self._pageGroupRef));
}

- (void)setAllowsJavaScript:(BOOL)allowsJavaScript
{
    WKPreferencesSetJavaScriptEnabled(WKPageGroupGetPreferences(self._pageGroupRef), allowsJavaScript);
}

- (BOOL)allowsPlugIns
{
    return WKPreferencesGetPluginsEnabled(WKPageGroupGetPreferences(self._pageGroupRef));
}

- (void)setAllowsPlugIns:(BOOL)allowsPlugIns
{
    WKPreferencesSetPluginsEnabled(WKPageGroupGetPreferences(self._pageGroupRef), allowsPlugIns);
}

@end

@implementation WKBrowsingContextGroup (Internal)

- (WKPageGroupRef)_pageGroupRef
{
    return _data->_pageGroupRef.get();
}

@end
