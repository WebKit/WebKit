/*	
    IFWebController.mm
	Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <WebKit/IFWebController.h>
#import <WebKit/IFWebDataSource.h>
#import <WebKit/IFWebView.h>


@interface IFBaseWebControllerPrivate : NSObject
{
    IFWebFrame *mainFrame;
}
@end
