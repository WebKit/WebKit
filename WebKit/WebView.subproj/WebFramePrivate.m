/*	
        IFWebFramePrivate.mm
	    
	    Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <WebKit/IFWebDataSource.h>
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFWebViewPrivate.h>
#import <WebKit/IFWebFramePrivate.h>
#import <WebKit/IFError.h>

#import <WebKit/WebKitDebug.h>

// includes from kde
#include <khtmlview.h>

@implementation IFWebFramePrivate

- (void)dealloc
{
    [lastError autorelease];
    [name autorelease];
    [view autorelease];
    [dataSource autorelease];
    [super dealloc];
}

- (NSString *)name { return name; }
- (void)setName: (NSString *)n 
{
    [name autorelease];
    name = [n retain];
}


- view { return view; }
- (void)setView: v 
{ 
    [view autorelease];
    view = [v retain];
}

- (IFWebDataSource *)dataSource { return dataSource; }
- (void)setDataSource: (IFWebDataSource *)d
{ 
    [dataSource autorelease];
    dataSource = [d retain];
}


- (id <IFWebController>)controller { return controller; }
- (void)setController: (id <IFWebController>)c
{ 
    // Warning:  non-retained reference
    controller = c;
}


- (IFWebDataSource *)provisionalDataSource { return provisionalDataSource; }
- (void)setProvisionalDataSource: (IFWebDataSource *)d
{ 
    [provisionalDataSource autorelease];
    provisionalDataSource = [d retain];
}


- (void *)renderFramePart { return renderFramePart; }
- (void)setRenderFramePart: (void *)p 
{
    renderFramePart = p;
}


@end


@implementation IFWebFrame (IFPrivate)


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

- (void)_setDataSource: (IFWebDataSource *)ds
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
    [data setDataSource: ds];
    [ds _setController: [self controller]];
}


- (void)_transitionProvisionalToCommitted
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;

    WEBKIT_ASSERT ([self controller] != nil);

    WEBKIT_ASSERT ([self _state] == IFWEBFRAMESTATE_PROVISIONAL);

    [[self view] _stopPlugins];
    
    [[self view] _removeSubviews];
    
    // Set the committed data source on the frame.
    [self _setDataSource: data->provisionalDataSource];
    
    // dataSourceChanged: will reset the view and begin trying to
    // display the new new datasource.
    [[self view] dataSourceChanged: data->provisionalDataSource];

    
    // Now that the provisional data source is committed, release it.
    [data setProvisionalDataSource: nil];

    [self _setState: IFWEBFRAMESTATE_COMMITTED];

    [[self controller] locationChangeCommittedForFrame: self];
}

- (IFWebFrameState)_state
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
    
    return data->state;
}

- (void)_setState: (IFWebFrameState)newState
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;

    data->state = newState;
}

- (BOOL)_checkLoadComplete: (IFError *)error
{
    int i, count;
    
    WEBKIT_ASSERT ([self controller] != nil);

    if ([self _state] == IFWEBFRAMESTATE_COMPLETE)
        return YES;

    if ([self _state] == IFWEBFRAMESTATE_PROVISIONAL && error == nil)
        return NO;

    // Check all children first.
    count = [[[self dataSource] children] count];
    for (i = 0; i < count; i++){
        IFWebFrame *childFrame;
        
        childFrame = [[[self dataSource] children] objectAtIndex: i];
        if ([childFrame _checkLoadComplete: nil] == NO)
            return NO;
    }

    if (![[self dataSource] isLoading]){
        if (error) {
            [self _setLastError: error];
        }
        
        [self _setState: IFWEBFRAMESTATE_COMPLETE];
        
        [[self dataSource] _part]->end();
        
        if ([[self controller] mainFrame] == self){
            [[self view] setNeedsLayout: YES];
            [[self view] layout];
            [[self view] display];
        }
        [[self controller] locationChangeDone: nil forFrame: self];
        return YES;
    }
    return NO;
}

- (void)_setLastError: (IFError *)error
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
    
    [data->lastError release];
    data->lastError = [error retain];
}

@end

