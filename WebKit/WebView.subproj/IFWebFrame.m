/*	
        IFWebFrame.h
	    
	    Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/IFWebFrame.h>
#import <WebKit/IFWebDataSource.h>

@implementation IFWebFrame

- initWithName: (NSString *)n view: v dataSource: (IFWebDataSource *)d
{
    [super init];
    name = [n retain];
    view = [v retain];
    dataSource = [d retain];
    return self;
}

- (void)dealloc
{
    [name autorelease];
    [view autorelease];
    [dataSource autorelease];
}

- (NSString *)name
{
    return name;
}


- (void)setView: v
{
    if (view)
        [view autorelease];
    view = [v retain];
}

- view
{
    return view;
}


- (IFWebDataSource *)dataSource
{
    return dataSource;
}

- (void)setDataSource: (IFWebDataSource *)ds
{
    if (dataSource == ds)
        return;
        
    [dataSource autorelease];
    dataSource = [ds retain];
    [dataSource setFrame: self];
}

// Required to break retain cycle between frame and data source.
- (void)reset
{
    [dataSource autorelease];
    dataSource = nil;
    [view autorelease];
    view = nil;
}

// renderFramePart is a pointer to a RenderPart
- (void)_setRenderFramePart: (void *)p
{
    renderFramePart = p;
}

- (void *)_renderFramePart
{
    return renderFramePart;
}

@end
