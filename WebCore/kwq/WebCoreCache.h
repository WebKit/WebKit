//
//  WebCoreCache.h
//  WebCore
//
//  Created by Darin Adler on Sun Jul 14 2002.
//  Copyright (c) 2002 Apple Computer, Inc.. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface WebCoreCache : NSObject
{
}

+ (NSArray *)statistics;
+ (void)empty;
+ (void)setDisabled:(BOOL)disabled;

@end
