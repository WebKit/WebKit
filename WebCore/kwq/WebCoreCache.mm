/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#import "WebCoreCache.h"

#include "Cache.h"

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
