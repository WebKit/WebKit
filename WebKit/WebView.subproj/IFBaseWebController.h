/*	
    IFDefaultWebController.mm
	Copyright 2001, 2002 Apple, Inc. All rights reserved.
*/
#import <WebKit/IFWebController.h>
#import <WebKit/IFWebFrame.h>


/*
*/
@interface WKDefaultWebController : NSObject <WKWebController>
{
@private
    id _controllerPrivate;
}


- init;

- initWithView: (WKWebView *)view dataSource: (WKWebDataSource *)dataSource;

- (void)setDirectsAllLinksToSystemBrowser: (BOOL)flag;
- (BOOL)directsAllLinksToSystemBrowser;

// Sets the mainView and the mainDataSource.
- (void)setView: (WKWebView *)view andDataSource: (WKWebDataSource *)dataSource;

// Find the view for the specified data source.
- (WKWebView *)viewForDataSource: (WKWebDataSource *)dataSource;

// Find the data source for the specified view.
- (WKWebDataSource *)dataSourceForView: (WKWebView *)view;

- (void)setMainView: (WKWebView *)view;
- (WKWebView *)mainView;

- (void)setMainDataSource: (WKWebDataSource *)dataSource;
- (WKWebDataSource *)mainDataSource;

@end
