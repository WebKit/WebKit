/*	
        IFWebFrame.h
	    
	    Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/
#import <Cocoa/Cocoa.h>

@class IFWebDataSource;

@interface IFWebFrame : NSObject
{
    // FIXME:  Create private class for ivars.
    NSString *name;
    id view;
    IFWebDataSource *dataSource;
    void *renderFramePart;
}

- initWithName: (NSString *)name view: view dataSource: (IFWebDataSource *)dataSource;
- (NSString *)name;
- (void)setView: view;
- view;
- (void)setDataSource: (IFWebDataSource *)ds;
- (IFWebDataSource *)dataSource;

// private
- (void)_setRenderFramePart: (void *)p;
- (void *)_renderFramePart;
@end
