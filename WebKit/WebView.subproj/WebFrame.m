/*	
        IFWebFrame.m
	    
	    Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/IFWebFrame.h>
#import <WebKit/IFWebFramePrivate.h>
#import <WebKit/IFWebViewPrivate.h>
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFBaseWebControllerPrivate.h>
#import <WebKit/IFWebController.h>
#import <WebKit/IFLocationChangeHandler.h>

#import <WebKit/WebKitDebug.h>

#include <khtml_part.h>
#include <rendering/render_frames.h>

@implementation IFWebFrame

- init
{
    return [self initWithName: nil view: nil provisionalDataSource: nil controller: nil];
}

- initWithName: (NSString *)n view: v provisionalDataSource: (IFWebDataSource *)d controller: (id<IFWebController>)c
{
    [super init];

    _private = [[IFWebFramePrivate alloc] init];   

    [self _setState: IFWEBFRAMESTATE_UNINITIALIZED];    

    [self setController: c];

    // Allow controller to override?
    if (d && [self setProvisionalDataSource: d] == NO){
        [self autorelease];
        return nil;
    }
    
    [_private setName: n];
    
    if (v)
        [self setView: v];
    
    return self;
}

- (void)dealloc
{
    [_private release];
    [super dealloc];
}

- (NSString *)name
{
    return [_private name];
}


- (void)setView: v
{
    [_private setView: v];
    [v _setController: [self controller]];
}

- view
{
    return [_private view];
}

- (id <IFWebController>)controller
{
    return [_private controller];
}


- (void)setController: (id <IFWebController>)controller
{
    [_private setController: controller];
}


- (IFWebDataSource *)provisionalDataSource
{
    return [_private provisionalDataSource];
}


- (IFWebDataSource *)dataSource
{
    return [_private dataSource];
}


// FIXME:  The name of this method is a little misleading, perhaps
// we could call it prepareProvisionalDataSource?.
- (BOOL)setProvisionalDataSource: (IFWebDataSource *)newDataSource
{
    IFWebDataSource *oldDataSource;
    id <IFLocationChangeHandler>locationChangeHandler;

    WEBKIT_ASSERT ([self controller] != nil);

    // Unfortunately the view must be non-nil, this is ultimately due
    // to KDE parser requiring a KHTMLView.  Once we settle on a final
    // KDE drop we should fix this dependency.
    WEBKIT_ASSERT ([self view] != nil);

    if ([self _state] != IFWEBFRAMESTATE_COMPLETE){
        [self stopLoading];
    }
    
    locationChangeHandler = [[self controller] provideLocationChangeHandlerForFrame: self];

    [newDataSource _setLocationChangeHandler: locationChangeHandler];

    oldDataSource = [self dataSource];
    
    // Is this the top frame?  If so set the data source's parent to nil.
    if (self == [[self controller] mainFrame])
        [newDataSource _setParent: nil];
        
    // Otherwise set the new data source's parent to the old data source's parent.
    else if (oldDataSource && oldDataSource != newDataSource)
        [newDataSource _setParent: [oldDataSource parent]];
            
    [newDataSource _setController: [self controller]];
    
    [_private setProvisionalDataSource: newDataSource];
    
    [[self view] provisionalDataSourceChanged: newDataSource];

#ifdef OLD_WAY
    // This introduces a nasty dependency on the view.
    khtml::RenderPart *renderPartFrame = [self _renderFramePart];
    id view = [self view];
    if (renderPartFrame && [view isKindOfClass: NSClassFromString(@"IFWebView")])
        renderPartFrame->setWidget ([view _provisionalWidget]);
#endif

    [self _setState: IFWEBFRAMESTATE_PROVISIONAL];
    
    return YES;
}


- (void)startLoading
{
    if (self == [[self controller] mainFrame])
        WEBKITDEBUGLEVEL (WEBKIT_LOG_DOCUMENTLOAD, "loading %s", [[[[self provisionalDataSource] inputURL] absoluteString] cString]);

    // Force refresh is irrelevant, as this will always be the first load.
    // The controller will transition the provisional data source to the
    // committed data source.
    [_private->provisionalDataSource startLoading: NO];
}


- (void)stopLoading
{
    [_private->provisionalDataSource stopLoading];
    [_private->dataSource stopLoading];
}


- (void)reload: (BOOL)forceRefresh
{
    [_private->dataSource _clearErrors];

    [_private->dataSource startLoading: forceRefresh];
}


- (void)reset
{
    [_private setDataSource: nil];
    [[_private view] _resetWidget];
    [_private setView: nil];
}

@end
