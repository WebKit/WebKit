//
//  WebCookieAdapter.m
//  WebKit
//
//  Created by Maciej Stachowiak on Thu Jun 27 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebCookieAdapter.h"

#import <WebKit/WebAssertions.h>
#import <WebKit/WebNSURLExtras.h>
#import <Foundation/NSHTTPCookie.h>
#import <Foundation/NSHTTPCookieStorage.h>

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
    NSURL *URL = [NSURL _web_URLWithDataAsString:URLString];
    NSArray *cookiesForURL = [[NSHTTPCookieStorage sharedHTTPCookieStorage] cookiesForURL:URL];
    NSDictionary *header = [NSHTTPCookie requestHeaderFieldsWithCookies:cookiesForURL];
    return [header objectForKey:@"Cookie"];
}

- (void)setCookies:(NSString *)cookieString forURL:(NSString *)URLString policyBaseURL:(NSString *)policyBaseURL
{
    NSURL *URL = [NSURL _web_URLWithDataAsString:URLString];
    NSArray *cookies = [NSHTTPCookie cookiesWithResponseHeaderFields:[NSDictionary dictionaryWithObject:cookieString forKey:@"Set-Cookie"] forURL:URL];
    [[NSHTTPCookieStorage sharedHTTPCookieStorage] setCookies:cookies forURL:URL mainDocumentURL:[NSURL _web_URLWithDataAsString:policyBaseURL]];    
}

@end
