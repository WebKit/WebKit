//
//  WebNSPrintOperationExtras.m
//  WebKit
//
//  Created by John Sullivan on Wed Jan 28 2004.
//  Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
//

#import "WebNSPrintOperationExtras.h"


@implementation NSPrintOperation (WebKitExtras)

- (float)_web_pageSetupScaleFactor
{
    return [[[[self printInfo] dictionary] objectForKey:NSPrintScalingFactor] floatValue];
}

@end
