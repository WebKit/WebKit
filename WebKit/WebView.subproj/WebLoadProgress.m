/*	IFLoadProgress.mm

        Copyright 2001, Apple, Inc. All rights reserved.
*/

#import "IFLoadProgress.h"

#include <WCLoadProgress.h>

@implementation IFLoadProgress

static id IFLoadProgressMake()
{
    return [[[IFLoadProgress alloc] init] autorelease];
}

+(void) load
{
    WCSetIFLoadProgressMakeFunc(IFLoadProgressMake);
}

- (id)initWithBytesSoFar:(int)bytes totalToLoad:(int)total type:(IF_LOAD_TYPE)loadType
{
    if (![super init]) {
        return nil;
    }

    bytesSoFar = bytes;
    totalToLoad = total;
    type = loadType;

    return self;
}

- (int)bytesSoFar
{
    return bytesSoFar;
}

- (int)totalToLoad
{
    return totalToLoad;
}

- (IF_LOAD_TYPE)type
{
    return type;
}

@end