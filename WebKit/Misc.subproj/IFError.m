/*	
        IFError.m
	Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <IFError.h>

#include <WCError.h>

@implementation IFError

static id IFErrorMake(int code) 
{
    return [[[IFError alloc] initWithErrorCode: (int)code] autorelease];
}

+(void) load
{
    WCSetIFErrorMakeFunc(IFErrorMake);
}

- initWithErrorCode: (int)c
{
    [super init];
    errorCode = c;
    return self;
}

- (int)errorCode
{
    return errorCode;
}

- (NSString *)errorDescription
{
    // FIXME!
    return nil;
}

@end
