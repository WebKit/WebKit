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

#import <WebKit/WebKitDebug.h>

#include <KWQKHTMLPart.h>
#include <rendering/render_frames.h>

@interface _IFFrameHolder : NSObject
{
    id object;
}
- initWithObject: o;
- (void)_checkReadyToDealloc: userInfo;
@end
@implementation _IFFrameHolder
- initWithObject: o
{
    object = o;	// Non-retained
    return [super init];
}

- (void)_checkReadyToDealloc: userInfo
{
    if ([object dataSource] == nil || ![[object dataSource] isLoading])
        [object dealloc];
    else {
        [NSTimer scheduledTimerWithTimeInterval:1.0 target:self selector: @selector(_checkReadyToDealloc:) userInfo: nil repeats:FALSE];
    }
}
@end

@implementation IFWebFrame

- init
{
    return [self initWithName: nil view: nil provisionalDataSource: nil controller: nil];
}

- initWithName: (NSString *)n view: v provisionalDataSource: (IFWebDataSource *)d controller: (id<IFWebController>)c
{
    IFWebFramePrivate *data;

    [super init];

    _framePrivate = [[IFWebFramePrivate alloc] init];   

    [self _setState: IFWEBFRAMESTATE_UNINITIALIZED];    

    [self setController: c];

    // Allow controller to override?
    if (d && [self setProvisionalDataSource: d] == NO){
        [self autorelease];
        return nil;
    }
    
    data = (IFWebFramePrivate *)_framePrivate;
    
    [data setName: n];
    
    if (v)
        [self setView: v];
    
    return self; 
}

- (oneway void)release {
#ifdef THIS_MAY_BE_BAD
    if ([self retainCount] == 1){
        _IFFrameHolder *ch = [[[_IFFrameHolder alloc] initWithObject: self] autorelease];
        [self stopLoading];
        [NSTimer scheduledTimerWithTimeInterval:1.0 target:ch selector: @selector(_checkReadyToDealloc:) userInfo: nil repeats:FALSE];
        return;
    }
#endif
    [super release];
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


- (IFWebDataSource *)provisionalDataSource
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
    return [data provisionalDataSource];
}


- (IFWebDataSource *)dataSource
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
    return [data dataSource];
}


- (BOOL)setProvisionalDataSource: (IFWebDataSource *)newDataSource
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
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
    if (newDataSource != nil && locationChangeHandler != nil){
        if (![locationChangeHandler locationWillChangeTo: [newDataSource inputURL]])
            return NO;
    }
    [newDataSource _setLocationChangeHandler: locationChangeHandler];

    oldDataSource = [self dataSource];
    
    // Is this the top frame?  If so set the data source's parent to nil.
    if (self == [[self controller] mainFrame])
        [newDataSource _setParent: nil];
        
    // Otherwise set the new data source's parent to the old data source's parent.
    else if (oldDataSource && oldDataSource != newDataSource)
        [newDataSource _setParent: [oldDataSource parent]];
            
    [newDataSource _setController: [self controller]];
    
    [data setProvisionalDataSource: newDataSource];
    
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
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
    
    if (self == [[self controller] mainFrame])
        WEBKITDEBUGLEVEL (WEBKIT_LOG_DOCUMENTLOAD, "loading %s", [[[[self provisionalDataSource] inputURL] absoluteString] cString]);

    // Force refresh is irrelevant, as this will always be the first load.
    // The controller will transition the provisional data source to the
    // committed data source.
    [data->provisionalDataSource startLoading: NO];
}


- (void)stopLoading
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
    
    [data->provisionalDataSource stopLoading];
    [data->dataSource stopLoading];
}


- (void)reload: (BOOL)forceRefresh
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;

    [self _clearErrors];

    [data->dataSource startLoading: forceRefresh];
}


- (void)reset
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
    [data setDataSource: nil];
    [[data view] _resetWidget];
    [data setView: nil];
}

- (NSDictionary *)errors
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
    return data->errors;
}

- (IFError *)mainDocumentError
{
    IFWebFramePrivate *data = (IFWebFramePrivate *)_framePrivate;
    return data->mainDocumentError;
}


@end
