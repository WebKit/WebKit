/*
        IFLoadProgress.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Foundation/Foundation.h>

typedef enum {
    IF_LOAD_TYPE_CSS    = 1,
    IF_LOAD_TYPE_IMAGE  = 2,
    IF_LOAD_TYPE_SCRIPT = 3,
    IF_LOAD_TYPE_HTML = 4,
    IF_LOAD_TYPE_PLUGIN = 5
} IF_LOAD_TYPE;


@interface IFLoadProgress : NSObject
{
    int bytesSoFar;	// 0 if this is the start of load
    int totalToLoad;	// -1 if this is not known.
                        // bytesSoFar == totalLoaded when complete
    IF_LOAD_TYPE type;
}

- (id)initWithBytesSoFar:(int)bytes totalToLoad:(int)total type:(IF_LOAD_TYPE)loadType;

- (int)bytesSoFar;
- (int)totalToLoad;
- (IF_LOAD_TYPE)type;

@end


