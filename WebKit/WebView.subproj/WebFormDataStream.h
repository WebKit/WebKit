/* Copyright 2004, Apple Computer, Inc. */

#import <Foundation/Foundation.h>

@interface WebFormDataStream : NSInputStream
{
    NSArray *_formDataArray;
}

- (id)initWithFormDataArray:(NSArray *)array;
- (NSArray *)formDataArray;

@end
