//
//  WebCookieAdapter.m
//  WebKit
//
//  Created by Maciej Stachowiak on Thu Jun 27 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebCookieAdapter.h"

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebCookieConstants.h>
#import <WebFoundation/WebCookieManager.h>
#import <WebFoundation/WebNSURLExtras.h>

@implementation WebCookieAdapter

+ (void)createSharedAdapter
{
    if (![self sharedAdapter]) {
        [[[self alloc] init] release];
    }
    ASSERT([[self sharedAdapter] isKindOfClass:self]);
}

- (BOOL)cookiesEnabled
{
    BOOL result;

    WebCookieAcceptPolicy acceptPolicy = [[WebCookieManager sharedCookieManager] acceptPolicy];
    result = (acceptPolicy == WebCookieAcceptPolicyAlways || acceptPolicy == WebCookieAcceptPolicyOnlyFromMainDocumentDomain);
    
    return result;
}

- (NSString *)cookiesForURL:(NSString *)URL
{
    return [[[WebCookieManager sharedCookieManager] cookieRequestHeadersForURL:[NSURL _web_URLWithString:URL]] objectForKey:@"Cookie"];
}

- (void)setCookies:(NSString *)cookies forURL:(NSString *)URL policyBaseURL:(NSString *)policyBaseURL
{
    [[WebCookieManager sharedCookieManager] setCookiesFromResponseHeaders:[NSDictionary dictionaryWithObject:cookies forKey:@"Set-Cookie"]
        forURL:[NSURL _web_URLWithString:URL] policyBaseURL:[NSURL _web_URLWithString:policyBaseURL]];    
}

@end
