/*	
    IFWebController.mm
	Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <WebKit/IFBaseWebControllerPrivate.h>
#import <WebKit/IFWebDataSource.h>
#import <WebKit/IFWebView.h>


@implementation WKDefaultWebControllerPrivate

- init 
{
    mainView = nil;
    mainDataSource = nil;
    viewMap = nil;
    dataSourceMap = nil;
    return self;
}

- (void)dealloc
{
    [mainView release];
    [mainDataSource release];
    
    [viewMap release];
    [dataSourceMap release];
}

@end
