/*	
        IFWebFrame.m
	    
	    Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/IFWebFrame.h>
#import <WebKit/IFWebFramePrivate.h>
#import <WebKit/IFWebDataSource.h>

@implementation IFWebFrame

- init
{
    return [self initWithName: nil view: nil dataSource: nil];
}

- initWithName: (NSString *)n view: v dataSource: (IFWebDataSource *)d
{
    IFWebFramePrivate *data;

    [super init];
    
    _framePrivate = [[IFWebFramePrivate alloc] init];
    
    data = (IFWebFramePrivate *)_framePrivate;
    
    [data setName: n];
    [data setView: v];
    [data setDataSource:  d];
    
    return self; 
}

- (void)dealloc
{
    [_framePrivate release];
    [super dealloc];
}

- (NSString *)name
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
    return [data name];
}


- (void)setView: v
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
    [data setView: v];
}

- view
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
    return [data view];
}


- (IFWebDataSource *)dataSource
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
    return [data dataSource];
}

- (void)setDataSource: (IFWebDataSource *)ds
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
    if ([data dataSource] == ds)
        return;
        
    [data setDataSource: ds];
    [[data dataSource] setFrame: self];
}

// Required to break retain cycle between frame and data source.
- (void)reset
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
    [data setDataSource: nil];
    [data setView: nil];
}

// renderFramePart is a pointer to a RenderPart
- (void)_setRenderFramePart: (void *)p
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
    [data setRenderFramePart: p];
}

- (void *)_renderFramePart
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
    return [data renderFramePart];
}

@end
