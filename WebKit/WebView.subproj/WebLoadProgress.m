/*	IFLoadProgress.mm

        Copyright 2001, 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/IFLoadProgress.h>

#import <WebFoundation/IFURLHandle.h>

@implementation IFLoadProgress

- (id)init
{
    return [self initWithBytesSoFar:-1 totalToLoad:-1];
}

- (id)initWithBytesSoFar:(int)bytes totalToLoad:(int)total
{
    if (![super init]) {
        return nil;
    }

    bytesSoFar = bytes;
    totalToLoad = total;

    return self;
}

- (id)initWithURLHandle:(IFURLHandle *)handle
{
    int b = [handle contentLengthReceived];
    int t = [handle statusCode] == IFURLHandleStatusLoadComplete ? b : [handle contentLength];
    return [self initWithBytesSoFar:b totalToLoad:t];
}

+ (IFLoadProgress *)progress
{
    return [[[IFLoadProgress alloc] init] autorelease];
}

+ (IFLoadProgress *)progressWithBytesSoFar:(int)bytes totalToLoad:(int)total
{
    return [[[IFLoadProgress alloc] initWithBytesSoFar:bytes totalToLoad:total] autorelease];
}

+ (IFLoadProgress *)progressWithURLHandle:(IFURLHandle *)handle
{
    return [[[IFLoadProgress alloc] initWithURLHandle:handle] autorelease];
}

- (int)bytesSoFar
{
    return bytesSoFar;
}

- (int)totalToLoad
{
    return totalToLoad;
}

@end
