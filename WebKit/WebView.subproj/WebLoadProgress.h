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
} IFLoadType;


@interface IFLoadProgress : NSObject
{
    int bytesSoFar;	// 0 if this is the start of load
    int totalToLoad;	// -1 if this is not known.
                        // bytesSoFar == totalLoaded when complete
    IFLoadType type;
}

- (id)initWithBytesSoFar:(int)bytes totalToLoad:(int)total type:(IFLoadType)loadType;

- (int)bytesSoFar;
- (int)totalToLoad;
- (IFLoadType)type;

@end


