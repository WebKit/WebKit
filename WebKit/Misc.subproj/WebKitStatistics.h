//
//  WebKitStatistics.h
//  WebKit
//
//  Created by Darin Adler on Fri Jul 19 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface WebKitStatistics : NSObject
{
}

+ (int)webViewCount;

+ (int)frameCount;
+ (int)dataSourceCount;
+ (int)viewCount;
+ (int)HTMLRepresentationCount;
+ (int)bridgeCount;

@end
