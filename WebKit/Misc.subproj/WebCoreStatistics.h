//
//  IFCache.h
//  WebKit
//
//  Created by Darin Adler on Thu Mar 28 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface IFCache : NSObject
{
}

+ (NSArray *)getStatistics;
+ (void)empty;
+ (void)setDisabled:(BOOL)disabled;

+ (int)javaScriptObjectsCount;
+ (int)javaScriptInterpretersCount;
+ (int)javaScriptNoGCAllowedObjectsCount;
+ (int)javaScriptReferencedObjectsCount;
+ (void)garbageCollectJavaScriptObjects;

@end
