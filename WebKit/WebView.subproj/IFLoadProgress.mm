/*	IFLoadProgress.mm

        Copyright 2001, Apple, Inc. All rights reserved.
*/

#import "IFLoadProgress.h"

#include <WCLoadProgress.h>

@implementation IFLoadProgress

static id IFLoadProgressMake(void)
{
    return [[[IFLoadProgress alloc] init] autorelease];
}

+(void) load
{
    WCSetIFLoadProgressMakeFunc(IFLoadProgressMake);
}

- (id)initWithBytesSoFar:(int)bytes totalToLoad:(int)total type:(IFLoadType)loadType
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

- (IFLoadType)type
{
    return type;
}

@end