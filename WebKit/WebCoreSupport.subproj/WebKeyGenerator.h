//
//  WebKeyGenerator.h
//  WebKit
//
//  Created by Chris Blumenberg on Thu Nov 20 2003.
//  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
//

#import <WebCore/WebCoreKeyGenerator.h>


@interface WebKeyGenerator : WebCoreKeyGenerator
{
    NSArray *strengthMenuItemTitles;
}
+ (void)createSharedGenerator;
- (BOOL)addCertificatesToKeychainFromData:(NSData *)data;
@end
