//
//  WebCoreStatistics.h
//  WebKit
//
//  Created by Darin Adler on Thu Mar 28 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface WebCoreStatistics : NSObject
{
}

+ (NSArray *)statistics;
+ (void)emptyCache;
+ (void)setCacheDisabled:(BOOL)disabled;

+ (int)javaScriptObjectsCount;
+ (int)javaScriptInterpretersCount;
+ (int)javaScriptNoGCAllowedObjectsCount;
+ (int)javaScriptReferencedObjectsCount;
+ (NSSet *)javaScriptLiveObjectClasses;
+ (void)garbageCollectJavaScriptObjects;

@end
