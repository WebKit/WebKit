/*	
    IFBaseWebController.mm
	Copyright 2001, 2002 Apple, Inc. All rights reserved.
*/
#import <WebKit/IFWebController.h>
#import <WebKit/IFWebFrame.h>


/*
*/
@interface IFBaseWebController : NSObject <IFWebController>
{
@private
    id _controllerPrivate;
}


- init;

- initWithView: (IFWebView *)view dataSource: (IFWebDataSource *)dataSource;

- (void)setDirectsAllLinksToSystemBrowser: (BOOL)flag;
- (BOOL)directsAllLinksToSystemBrowser;

// Sets the mainView and the mainDataSource.
- (void)setView: (IFWebView *)view andDataSource: (IFWebDataSource *)dataSource;

// Find the view for the specified data source.
- (IFWebView *)viewForDataSource: (IFWebDataSource *)dataSource;

// Find the data source for the specified view.
- (IFWebDataSource *)dataSourceForView: (IFWebView *)view;

- (void)setMainView: (IFWebView *)view;
- (IFWebView *)mainView;

- (void)setMainDataSource: (IFWebDataSource *)dataSource;
- (IFWebDataSource *)mainDataSource;

@end
