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
#import <WebKit/IFDownloadHandler.h>
#import <WebFoundation/WebFoundation.h>

#import <WebKit/WebKitDebug.h>

#import <khtml_part.h>
#import <rendering/render_frames.h>

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

    if (d == nil) {
	// set a dummy data source so that the main from for a
	// newly-created empty window has a KHTMLPart. JavaScript
	// always creates new windows initially empty, and then wants
	// to use the main frame's part to make the new window load
	// it's URL, so we need to make sure empty frames have a part.
	// However, we don't want to do the spinner, so we do this
	// weird thing:

	IFWebDataSource *dummyDataSource = [[IFWebDataSource alloc] initWithURL:nil];
        [dummyDataSource _setController: [self controller]];
        [_private setProvisionalDataSource: dummyDataSource];

    // Allow controller to override?
    } else if ([self setProvisionalDataSource: d] == NO){
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
    IFURLPolicy urlPolicy;
    
    WEBKIT_ASSERT ([self controller] != nil);

    // Unfortunately the view must be non-nil, this is ultimately due
    // to KDE parser requiring a KHTMLView.  Once we settle on a final
    // KDE drop we should fix this dependency.
    WEBKIT_ASSERT ([self view] != nil);

    urlPolicy = [[self controller] URLPolicyForURL:[newDataSource inputURL]];

    if(urlPolicy == IFURLPolicyUseContentPolicy){
            
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
    }
    
    else if(urlPolicy == IFURLPolicyOpenExternally){
        return [[NSWorkspace sharedWorkspace] openURL:[newDataSource inputURL]];
    }
    
    // Do nothing in the IFURLPolicyIgnore case.

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

- (IFWebFrame *)frameNamed:(NSString *)name
{
    if([name isEqualToString:@"_self"] || [name isEqualToString:@"_current"])
        return self;
    
    else if([name isEqualToString:@"_top"])
        return [[self controller] mainFrame];
        
    else if([name isEqualToString:@"_parent"]){
        if([[self dataSource] parent]){
            return [[[self dataSource] parent] webFrame];
        }else{
            return self;
        }
    }else{
        return [[self controller] frameNamed:name];
    }
}

@end
