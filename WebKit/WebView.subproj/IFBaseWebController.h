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
- (void)setMainView: (IFWebView *)view andMainDataSource: (IFWebDataSource *)dataSource;

- (void)setMainView: (IFWebView *)view;
- (IFWebView *)mainView;

- (void)setMainDataSource: (IFWebDataSource *)dataSource;
- (IFWebDataSource *)mainDataSource;

- (IFWebFrame *)mainFrame;

@end
