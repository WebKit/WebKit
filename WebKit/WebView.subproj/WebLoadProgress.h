/*
        WebLoadProgress.h
	Copyright 2001, 2002, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Foundation/Foundation.h>

@class WebResourceHandle;

/*!
    @class WebLoadProgress
*/
@interface WebLoadProgress : NSObject
{
    int bytesSoFar;	// 0 if this is the start of load
    int totalToLoad;	// -1 if this is not known.
                        // bytesSoFar == totalLoaded when complete
}

/*!
    @method initWithBytesSoFar:totalToLoad:
    @param bytes
    @param total
*/
- (id)initWithBytesSoFar:(int)bytes totalToLoad:(int)total;

/*!
    @method initWithResourceHandle:
    @param handle
*/
- (id)initWithResourceHandle:(WebResourceHandle *)handle;

/*!
    @method progress
*/
+ (WebLoadProgress *)progress; // both -1

/*!
    @method progressWithBytesSoFar:totalToLoad:
    @param bytes
    @param total
*/
+ (WebLoadProgress *)progressWithBytesSoFar:(int)bytes totalToLoad:(int)total;

/*!
    @method progressWithResourceHandle:
    @param handle
*/
+ (WebLoadProgress *)progressWithResourceHandle:(WebResourceHandle *)handle;

/*!
    @method bytesSoFar:
*/
- (int)bytesSoFar;

/*!
    @method totalToLoad:
*/
- (int)totalToLoad;

@end
