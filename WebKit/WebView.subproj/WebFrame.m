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

    WEBKIT_ASSERT ([self controller] != nil);

    // Unfortunately the view must be non-nil, this is ultimately due
    // to KDE parser requiring a KHTMLView.  Once we settle on a final
    // KDE drop we should fix this dependency.
    WEBKIT_ASSERT ([self view] != nil);

    if (newDataSource != nil){
        if (![[self controller] locationWillChangeTo: [newDataSource inputURL] forFrame: self])
            return NO;
    }

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
