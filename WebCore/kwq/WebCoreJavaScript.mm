//
//  WebCoreJavaScript.m
//  WebCore
//
//  Created by Darin Adler on Sun Jul 14 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebCoreJavaScript.h"

#import <JavaScriptCore/collector.h>

using KJS::Collector;

@implementation WebCoreJavaScript

+ (int)objectCount
{
    return Collector::size();
}

+ (int)interpreterCount
{
    return Collector::numInterpreters();
}

+ (int)noGCAllowedObjectCount
{
    return Collector::numGCNotAllowedObjects();
}

+ (int)referencedObjectCount
{
    return Collector::numReferencedObjects();
}

+ (void)garbageCollect
{
    while (Collector::collect()) { }
}

@end
