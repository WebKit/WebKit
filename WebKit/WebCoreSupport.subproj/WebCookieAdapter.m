//
//  WebCookieAdapter.m
//  WebKit
//
//  Created by Maciej Stachowiak on Thu Jun 27 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebCookieAdapter.h"

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/NSHTTPCookieStorage.h>
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

    NSHTTPCookieAcceptPolicy cookieAcceptPolicy = [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookieAcceptPolicy];
    result = (cookieAcceptPolicy == NSHTTPCookieAcceptPolicyAlways || cookieAcceptPolicy == NSHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain);
    
    return result;
}

- (NSString *)cookiesForURL:(NSString *)URLString
{
    NSURL *URL = [NSURL _web_URLWithString:URLString];
    NSArray *cookiesForURL = [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookiesForURL:URL];
    NSDictionary *header = [NSHTTPCookie requestHeaderFieldsWithCookies:cookiesForURL];
    return [header objectForKey:@"Cookie"];
}

- (void)setCookies:(NSString *)cookieString forURL:(NSString *)URLString policyBaseURL:(NSString *)policyBaseURL
{
    NSURL *URL = [NSURL _web_URLWithString:URLString];
    NSArray *cookies = [NSHTTPCookie cookiesWithResponseHeaderFields:[NSDictionary dictionaryWithObject:cookieString forKey:@"Set-Cookie"] forURL:URL];
    [[NSHTTPCookieStorage sharedHTTPCookieStorage] setCookies:cookies forURL:URL mainDocumentURL:[NSURL _web_URLWithString:policyBaseURL]];    
}

@end
