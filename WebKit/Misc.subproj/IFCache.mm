//
//  IFCache.m
//  WebKit
//
//  Created by Darin Adler on Thu Mar 28 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "IFCache.h"

#include "misc/loader.h"

@implementation IFCache

+ (NSArray *)getStatistics
{
    khtml::Cache::Statistics s = khtml::Cache::getStatistics();

    NSMutableDictionary *counts =
        [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithInt:s.images.count],@"images",
            [NSNumber numberWithInt:s.movies.count],@"movies",
            [NSNumber numberWithInt:s.styleSheets.count],@"style sheets",
            [NSNumber numberWithInt:s.scripts.count],@"scripts",
            [NSNumber numberWithInt:s.other.count],@"other",
            nil];
    
    NSMutableDictionary *sizes =
        [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithInt:s.images.size],@"images",
            [NSNumber numberWithInt:s.movies.size],@"movies",
            [NSNumber numberWithInt:s.styleSheets.size],@"style sheets",
            [NSNumber numberWithInt:s.scripts.size],@"scripts",
            [NSNumber numberWithInt:s.other.size],@"other",
            nil];
        
    return [NSArray arrayWithObjects:counts, sizes, nil];
}

+ (void)empty
{
    khtml::Cache::flushAll();
}

+ (void)setDisabled:(BOOL)disabled
{
    khtml::Cache::setCacheDisabled(disabled);
}

@end
