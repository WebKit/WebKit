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
    id <IFWebController>controller;
}

- (void)setName: (NSString *)n;
- (NSString *)name;
- (void)setController: (id <IFWebController>)c;
- (id <IFWebController>)controller;
- (void)setView: v;
- view;
- (void)setDataSource: (IFWebDataSource *)d;
- (IFWebDataSource *)dataSource;
- (void)setRenderFramePart: (void *)p;
- (void *)renderFramePart;

@end

@interface IFWebFrame (IFPrivate)
- (void)_setRenderFramePart: (void *)p;
- (void *)_renderFramePart;
- (void)_setDataSource: (IFWebDataSource *)d;
@end
