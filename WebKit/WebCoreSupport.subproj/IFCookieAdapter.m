//
//  IFCookieAdapter.m
//  WebKit
//
//  Created by Maciej Stachowiak on Thu Jun 27 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "IFCookieAdapter.h"
#import <WebFoundation/IFCookieManager.h>
#import <WebKit/WebKitDebug.h>


@implementation IFCookieAdapter

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
    id acceptCookiesPref = (id)CFPreferencesCopyAppValue((CFStringRef)IFAcceptCookiesPreference, (CFStringRef)IFWebFoundationPreferenceDomain);

    if ([acceptCookiesPref isEqualTo:IFAcceptCookiesPreferenceNever]) {
        result = NO;
    } else if ([acceptCookiesPref isEqualTo:IFAcceptCookiesPreferenceAsk]) {
        result = YES;
    } else if ([acceptCookiesPref isEqualTo:IFAcceptCookiesPreferenceAlways]) {
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
    return [[[IFCookieManager sharedCookieManager] cookieRequestHeadersForURL:url] objectForKey:@"Cookie"];
}

- (void)setCookies:(NSString *)cookies forURL:(NSURL *)url
{
    [[IFCookieManager sharedCookieManager] setCookiesFromResponseHeaders:[NSDictionary dictionaryWithObject:cookies forKey:@"Set-Cookie"] forURL:url];    
}

@end
