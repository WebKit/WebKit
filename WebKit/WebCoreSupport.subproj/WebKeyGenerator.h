//
//  WebKeyGenerator.h
//  WebKit
//
//  Created by Chris Blumenberg on Thu Nov 20 2003.
//  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
//

#ifndef BUILDING_ON_PANTHER
#define USE_NEW_KEY_GENERATION
#endif

typedef enum {
    WebCertificateParseResultSucceeded  = 0,
    WebCertificateParseResultFailed     = 1,
    WebCertificateParseResultPKCS7      = 2,
} WebCertificateParseResult;

#ifdef __OBJC__

#import <WebCore/WebCoreKeyGenerator.h>

@interface WebKeyGenerator : WebCoreKeyGenerator
{
    NSArray *strengthMenuItemTitles;
}
+ (void)createSharedGenerator;
- (WebCertificateParseResult)addCertificatesToKeychainFromData:(NSData *)data;
@end

#endif
