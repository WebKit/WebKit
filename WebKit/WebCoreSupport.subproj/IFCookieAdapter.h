//
//  IFCookieAdapter.h
//  WebKit
//
//  Created by Maciej Stachowiak on Thu Jun 27 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <WebCoreCookieAdapter.h>

@interface IFCookieAdapter : WebCoreCookieAdapter
{
}

+ (void)createSharedAdapter;

- (BOOL)cookiesEnabled;
- (NSString *)cookiesForURL:(NSURL *)url;
- (void)setCookies:(NSString *)cookies forURL:(NSURL *)url;


@end
