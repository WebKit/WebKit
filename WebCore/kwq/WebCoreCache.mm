//
//  WebCoreCache.m
//  WebCore
//
//  Created by Darin Adler on Sun Jul 14 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebCoreCache.h"

#import "loader.h"

using khtml::Cache;

@implementation WebCoreCache

+ (NSArray *)statistics
{
    Cache::Statistics s = Cache::getStatistics();

    return [NSArray arrayWithObjects:
        [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithInt:s.images.count], @"images",
            [NSNumber numberWithInt:s.movies.count], @"movies",
            [NSNumber numberWithInt:s.styleSheets.count], @"style sheets",
            [NSNumber numberWithInt:s.scripts.count], @"scripts",
            [NSNumber numberWithInt:s.other.count], @"other",
            nil],
        [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithInt:s.images.size], @"images",
            [NSNumber numberWithInt:s.movies.size], @"movies",
            [NSNumber numberWithInt:s.styleSheets.size] ,@"style sheets",
            [NSNumber numberWithInt:s.scripts.size], @"scripts",
            [NSNumber numberWithInt:s.other.size], @"other",
            nil],
        nil];
}

+ (void)empty
{
    Cache::flushAll();
}

+ (void)setDisabled:(BOOL)disabled
{
    Cache::setCacheDisabled(disabled);
}

@end
