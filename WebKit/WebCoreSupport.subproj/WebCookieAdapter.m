//
//  WebCookieAdapter.m
//  WebKit
//
//  Created by Maciej Stachowiak on Thu Jun 27 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebCookieAdapter.h"
#import <WebFoundation/WebCookieManager.h>
#import <WebKit/WebKitDebug.h>


@implementation WebCookieAdapter

+ (void)createSharedAdapter
{
    if (![self sharedAdapter]) {
        [[[self alloc] init] release];
    }
    WEBKIT_ASSERT([[self sharedAdapter] isMemberOfClass:self]);
}

- (BOOL)cookiesEnabled
{
    BOOL result;
    id acceptCookiesPref = (id)CFPreferencesCopyAppValue((CFStringRef)WebAcceptCookiesPreference, (CFStringRef)WebFoundationPreferenceDomain);

    if ([acceptCookiesPref isEqualTo:WebAcceptCookiesPreferenceNever]) {
        result = NO;
    } else if ([acceptCookiesPref isEqualTo:WebAcceptCookiesPreferenceAsk]) {
        result = YES;
    } else if ([acceptCookiesPref isEqualTo:WebAcceptCookiesPreferenceAlways]) {
        result = YES;
    } else {
        // Treat missing or bad value as always accept
        result = YES;
    }
    [acceptCookiesPref release];

    return result;
}

- (NSString *)cookiesForURL:(NSURL *)url
{
    return [[[WebCookieManager sharedCookieManager] cookieRequestHeadersForURL:url] objectForKey:@"Cookie"];
}

- (void)setCookies:(NSString *)cookies forURL:(NSURL *)url
{
    [[WebCookieManager sharedCookieManager] setCookiesFromResponseHeaders:[NSDictionary dictionaryWithObject:cookies forKey:@"Set-Cookie"] forURL:url];    
}

@end
