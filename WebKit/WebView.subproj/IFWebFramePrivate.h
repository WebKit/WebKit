/*	
        IFWebFramePrivate.h
	    
	    Copyright 2001, Apple, Inc. All rights reserved.

        Private header file.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/IFWebFrame.h>
#import <WebKit/IFWebDataSource.h>

@interface IFWebFramePrivate : NSObject
{
    NSString *name;
    id view;
    IFWebDataSource *dataSource;
    IFWebDataSource *provisionalDataSource;
    void *renderFramePart;
}

- (void)setName: (NSString *)n;
- (NSString *)name;
- (void)setView: v;
- view;
- (void)setDataSource: (IFWebDataSource *)d;
- (IFWebDataSource *)dataSource;
- (void)setRenderFramePart: (void *)p;
- (void *)renderFramePart;

@end
