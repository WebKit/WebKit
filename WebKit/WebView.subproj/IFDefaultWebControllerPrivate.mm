/*	
    WKWebController.mm
	Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <WebKit/WKDefaultWebControllerPrivate.h>
#import <WebKit/WKWebDataSource.h>
#import <WebKit/WKWebView.h>


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
