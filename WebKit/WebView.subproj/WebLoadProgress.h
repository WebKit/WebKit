/*
        WebLoadProgress.h
	Copyright 2001, 2002, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Foundation/Foundation.h>

@class WebResourceHandle;

@interface WebLoadProgress : NSObject
{
    int bytesSoFar;	// 0 if this is the start of load
    int totalToLoad;	// -1 if this is not known.
                        // bytesSoFar == totalLoaded when complete
}

- (id)init; // both -1
- (id)initWithBytesSoFar:(int)bytes totalToLoad:(int)total;
- (id)initWithResourceHandle:(WebResourceHandle *)handle;

+ (WebLoadProgress *)progress; // both -1
+ (WebLoadProgress *)progressWithBytesSoFar:(int)bytes totalToLoad:(int)total;
+ (WebLoadProgress *)progressWithResourceHandle:(WebResourceHandle *)handle;

- (int)bytesSoFar;
- (int)totalToLoad;

@end
