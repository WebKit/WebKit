/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#import "_WKWebsitePoliciesInternal.h"

#if WK_API_ENABLED

@implementation _WKWebsitePolicies

- (void)dealloc
{
    _websitePolicies->API::WebsitePolicies::~WebsitePolicies();
    
    [super dealloc];
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;
    
    API::Object::constructInWrapper<API::WebsitePolicies>(self);
    
    return self;
}

- (void)setContentBlockersEnabled:(BOOL)contentBlockersEnabled
{
    _websitePolicies->setContentBlockersEnabled(contentBlockersEnabled);
}

- (BOOL)contentBlockersEnabled
{
    return _websitePolicies->contentBlockersEnabled();
}

- (void)setAutoplayPolicy:(_WKWebsiteAutoplayPolicy)policy
{
    switch (policy) {
    case _WKWebsiteAutoplayPolicyDefault:
        _websitePolicies->setAutoplayPolicy(WebKit::WebsiteAutoplayPolicy::Default);
        break;
    case _WKWebsiteAutoplayPolicyAllow:
        _websitePolicies->setAutoplayPolicy(WebKit::WebsiteAutoplayPolicy::Allow);
        break;
    case _WKWebsiteAutoplayPolicyAllowWithoutSound:
        _websitePolicies->setAutoplayPolicy(WebKit::WebsiteAutoplayPolicy::AllowWithoutSound);
        break;
    case _WKWebsiteAutoplayPolicyDeny:
        _websitePolicies->setAutoplayPolicy(WebKit::WebsiteAutoplayPolicy::Deny);
        break;
    }
}

- (_WKWebsiteAutoplayPolicy)autoplayPolicy
{
    switch (_websitePolicies->autoplayPolicy()) {
    case WebKit::WebsiteAutoplayPolicy::Default:
        return _WKWebsiteAutoplayPolicyDefault;
    case WebKit::WebsiteAutoplayPolicy::Allow:
        return _WKWebsiteAutoplayPolicyAllow;
    case WebKit::WebsiteAutoplayPolicy::AllowWithoutSound:
        return _WKWebsiteAutoplayPolicyAllowWithoutSound;
    case WebKit::WebsiteAutoplayPolicy::Deny:
        return _WKWebsiteAutoplayPolicyDeny;
    }
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p; contentBlockersEnabled = %d>", NSStringFromClass(self.class), self, self.contentBlockersEnabled];
}

- (API::Object&)_apiObject
{
    return *_websitePolicies;
}

@end

#endif
