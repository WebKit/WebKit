/*	
    IFWebController.mm
	Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <WebKit/IFBaseWebControllerPrivate.h>
#import <WebKit/IFWebDataSourcePrivate.h>
#import <WebKit/IFWebViewPrivate.h>
#import <WebKit/IFWebFramePrivate.h>

#include <KWQKHTMLPart.h>
#include <rendering/render_frames.h>


@implementation IFBaseWebControllerPrivate

- init 
{
    mainFrame = nil;
    return self;
}

- (void)dealloc
{
    [mainFrame reset];
    [mainFrame autorelease];
    [super dealloc];
}

@end


@implementation IFBaseWebController (IFPrivate)

- (BOOL)_changeLocationTo: (NSURL *)url forFrame: (IFWebFrame *)frame parent: (IFWebDataSource *)parent
{
    if ([self locationWillChangeTo: url forFrame: frame]){
        IFWebDataSource *dataSource = [[[IFWebDataSource alloc] initWithURL: url] autorelease];
        
        [dataSource _setParent: parent];
        [self _changeFrame: frame dataSource: dataSource];
        return YES;
    }
    return NO;
}


- (void)_changeFrame: (IFWebFrame *)frame dataSource: (IFWebDataSource *)newDataSource
{
    IFBaseWebControllerPrivate *data = ((IFBaseWebControllerPrivate *)_controllerPrivate);
    IFWebDataSource *oldDataSource;

    oldDataSource = [frame dataSource];
    if (frame == data->mainFrame)
        [newDataSource _setParent: nil];
    else if (oldDataSource && oldDataSource != newDataSource)
        [newDataSource _setParent: [oldDataSource parent]];
            
    [newDataSource _setController: self];
    [frame _setDataSource: newDataSource];
    
    // dataSourceChanged: will reset the view and begin trying to
    // display the new new datasource.
    [[frame view] dataSourceChanged: newDataSource];

    // This introduces a nasty dependency on the view.
    khtml::RenderPart *renderPartFrame = [frame _renderFramePart];
    id view = [frame view];
    if (renderPartFrame && [view isKindOfClass: NSClassFromString(@"IFWebView")])
        renderPartFrame->setWidget ([[frame view] _widget]);
}

- (void)_checkLoadCompleteForDataSource: (IFWebDataSource *)dataSource
{
    // Check that all handle clients have been removed,
    // and that all descendent data sources are done
    // loading.  Then call locationChangeDone:forFrame:
}

@end
