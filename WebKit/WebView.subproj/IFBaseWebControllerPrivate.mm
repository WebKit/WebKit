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

- (void)_checkLoadCompleteForDataSource: (IFWebDataSource *)dataSource
{
    // Check that all handle clients have been removed,
    // and that all descendent data sources are done
    // loading.  Then call locationChangeDone:forFrame:
}

@end
