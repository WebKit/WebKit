//
//  WebCookieAdapter.h
//  WebKit
//
//  Created by Maciej Stachowiak on Thu Jun 27 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <WebCore/WebCoreCookieAdapter.h>

@interface WebCookieAdapter : WebCoreCookieAdapter
{
}

+ (void)createSharedAdapter;

@end
