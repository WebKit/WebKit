//
//  WebCoreJavaScript.h
//  WebCore
//
//  Created by Darin Adler on Sun Jul 14 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface WebCoreJavaScript : NSObject
{
}

+ (int)interpreterCount;

+ (int)objectCount;
+ (int)noGCAllowedObjectCount;
+ (int)referencedObjectCount;

+ (void)garbageCollect;

@end
