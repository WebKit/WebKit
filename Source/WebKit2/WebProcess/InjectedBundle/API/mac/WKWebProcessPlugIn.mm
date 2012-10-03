/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#import "WKWebProcessPlugIn.h"

#import "InjectedBundle.h"
#import "WKWebProcessPlugInInternal.h"
#import <wtf/RetainPtr.h>

struct WKWebProcessPlugInControllerData {
    RetainPtr<id<WKWebProcessPlugIn> > _principalClassInstance;
    RefPtr<WebKit::InjectedBundle> _injectedBundle;
};

@implementation WKWebProcessPlugInController (Internal)

static WKWebProcessPlugInController *sharedInstance;

+ (WKWebProcessPlugInController *)_shared
{
    ASSERT_WITH_MESSAGE(sharedInstance, "+[WKWebProcessPlugIn _shared] called without first initializing it.");
    return sharedInstance;
}

- (id)_initWithPrincipalClassInstance:(id<WKWebProcessPlugIn>)principalClassInstance injectedBundle:(WebKit::InjectedBundle*)injectedBundle
{
    self = [super init];
    if (!self)
        return nil;

    _private = new WKWebProcessPlugInControllerData;
    static_cast<WKWebProcessPlugInControllerData*>(_private)->_principalClassInstance = principalClassInstance;
    static_cast<WKWebProcessPlugInControllerData*>(_private)->_injectedBundle = injectedBundle;

    ASSERT_WITH_MESSAGE(!sharedInstance, "WKWebProcessPlugInController initialized multiple times.");
    sharedInstance = self;

    return self;
}

- (void)dealloc
{
    delete static_cast<WKWebProcessPlugInControllerData*>(_private);
    [super dealloc];
}

@end

@implementation WKWebProcessPlugInController

@end
