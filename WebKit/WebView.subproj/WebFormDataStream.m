/* Copyright 2004, Apple Computer, Inc. */

#import "WebFormDataStream.h"

@implementation WebFormDataStream

- (id)initWithFormDataArray:(NSArray *)array
{
    [super init];
    _formDataArray = [array copy];
    return self;
}

- (void)dealloc
{
    [_formDataArray release];
    [super dealloc];
}

- (NSArray *)formDataArray
{
    return _formDataArray;
}

@end
