/*	
        IFWebFramePrivate.mm
	    
	    Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <WebKit/IFWebDataSource.h>
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFWebFramePrivate.h>

#import <WebKit/WebKitDebug.h>

@implementation IFWebFramePrivate

- (void)dealloc
{
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

    // Set the committed data source on the frame.
    [self _setDataSource: data->provisionalDataSource];
    
    // dataSourceChanged: will reset the view and begin trying to
    // display the new new datasource.
    [[self view] dataSourceChanged: data->provisionalDataSource];

    
    // Now that the provisional data source is committed, release it.
    [data setProvisionalDataSource: nil];

    [[self controller] locationChangeCommittedForFrame: self];
}


@end

