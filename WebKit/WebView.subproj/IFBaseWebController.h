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


/*
    Calls designated initializer with nil arguments.
*/
- init;

/*
    Designated initializer.
*/
- initWithView: (IFWebView *)view provisionalDataSource: (IFWebDataSource *)dataSource;


- (void)setDirectsAllLinksToSystemBrowser: (BOOL)flag;


- (BOOL)directsAllLinksToSystemBrowser;

@end
