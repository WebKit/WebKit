/*	
    IFCarbonWindowView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <AppKit/NSView.h>

@interface IFCarbonWindowView : NSView
{
@private
    void*  _qdPort;
    void*  _savePort;
    BOOL   _synchToView;
}

- (void*) qdPort;

@end