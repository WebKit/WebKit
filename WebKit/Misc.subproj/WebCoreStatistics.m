//
//  WebCoreStatistics.m
//  WebKit
//
//  Created by Darin Adler on Thu Mar 28 2002.
//  Copyright (c) 2002, 2003 Apple Computer, Inc. All rights reserved.
//

#import "WebCoreStatistics.h"

#import <WebCore/WebCoreCache.h>
#import <WebCore/WebCoreJavaScript.h>

#import <WebKit/WebBridge.h>
#import <WebKit/WebFramePrivate.h>

@implementation WebCoreStatistics

+ (NSArray *)statistics
{
    return [WebCoreCache statistics];
}

+ (void)emptyCache
{
    [WebCoreCache empty];
}

+ (void)setCacheDisabled:(BOOL)disabled
{
    [WebCoreCache setDisabled:disabled];
}

+ (int)javaScriptObjectsCount
{
    return [WebCoreJavaScript objectCount];
}

+ (int)javaScriptInterpretersCount
{
    return [WebCoreJavaScript interpreterCount];
}

+ (int)javaScriptNoGCAllowedObjectsCount
{
    return [WebCoreJavaScript noGCAllowedObjectCount];
}

+ (int)javaScriptReferencedObjectsCount
{
    return [WebCoreJavaScript referencedObjectCount];
}

+ (NSSet *)javaScriptRootObjectClasses
{
    return [WebCoreJavaScript rootObjectClasses];
}

+ (void)garbageCollectJavaScriptObjects
{
    [WebCoreJavaScript garbageCollect];
}

+ (BOOL)shouldPrintExceptions
{
    return [WebCoreJavaScript shouldPrintExceptions];
}

+ (void)setShouldPrintExceptions:(BOOL)print
{
    [WebCoreJavaScript setShouldPrintExceptions:print];
}

@end

@implementation WebFrame (WebKitDebug)

- (NSString *)renderTreeAsExternalRepresentation
{
    return [[self _bridge] renderTreeAsExternalRepresentation];
}

@end
