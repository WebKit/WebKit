/*
        IFLoadProgress.h
	Copyright 2001, 2002, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Foundation/Foundation.h>

@class IFURLHandle;

@interface IFLoadProgress : NSObject
{
    int bytesSoFar;	// 0 if this is the start of load
    int totalToLoad;	// -1 if this is not known.
                        // bytesSoFar == totalLoaded when complete
}

- (id)init; // both -1
- (id)initWithBytesSoFar:(int)bytes totalToLoad:(int)total;
- (id)initWithURLHandle:(IFURLHandle *)handle;

+ (IFLoadProgress *)progress; // both -1
+ (IFLoadProgress *)progressWithBytesSoFar:(int)bytes totalToLoad:(int)total;
+ (IFLoadProgress *)progressWithURLHandle:(IFURLHandle *)handle;

- (int)bytesSoFar;
- (int)totalToLoad;

@end
