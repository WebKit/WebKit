/*	
        IFWebFrame.m
	    
	    Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/IFWebFrame.h>
#import <WebKit/IFWebFramePrivate.h>
#import <WebKit/IFWebViewPrivate.h>
#import <WebKit/IFWebDataSource.h>
#import <WebKit/IFBaseWebControllerPrivate.h>

#import <WebKit/WebKitDebug.h>

@implementation IFWebFrame

- init
{
    return [self initWithName: nil view: nil dataSource: nil controller: nil];
}

- initWithName: (NSString *)n view: v dataSource: (IFWebDataSource *)d controller: (id<IFWebController>)c
{
    IFWebFramePrivate *data;

    [super init];
    
    _framePrivate = [[IFWebFramePrivate alloc] init];   
    
    data = (IFWebFramePrivate *)_framePrivate;
    
    [data setName: n];
    
    [self setController: c];
    [self setView: v];
    [self setDataSource:  d];
    
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
    [v _setController: [self controller]];
}

- view
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
    return [data view];
}


- (id <IFWebController>)controller
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
    return [data controller];
}

- (void)setController: (id <IFWebController>)controller
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
    [data setController: controller];
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

    WEBKIT_ASSERT ([self controller] != nil);
        
    // FIXME!  _changeFrame:dataSource: is implemented in IFBaseWebController, not a IFWebController
    // method!
    if (ds != nil){
        [[self controller] _changeFrame: self dataSource: ds];
    }
}

- (void)reset
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
    [data setDataSource: nil];
    [[data view] _resetWidget];
    [data setView: nil];
}

@end
