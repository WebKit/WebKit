//
//  WebKitStatistics.m
//  WebKit
//
//  Created by Darin Adler on Fri Jul 19 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebKitStatistics.h"

#import "WebKitStatisticsPrivate.h"

int WebBridgeCount;
int WebViewCount;
int WebDataSourceCount;
int WebFrameCount;
int WebHTMLRepresentationCount;
int WebFrameViewCount;

@implementation WebKitStatistics

+ (int)webViewCount
{
    return WebViewCount;
}

+ (int)frameCount
{
    return WebFrameCount;
}

+ (int)dataSourceCount
{
    return WebDataSourceCount;
}

+ (int)viewCount
{
    return WebFrameViewCount;
}

+ (int)bridgeCount
{
    return WebBridgeCount;
}

+ (int)HTMLRepresentationCount
{
    return WebHTMLRepresentationCount;
}

@end
