/*	
    IFWebController.mm
	Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <WebKit/IFWebController.h>
#import <WebKit/IFWebDataSource.h>
#import <WebKit/IFWebView.h>


@interface WKDefaultWebControllerPrivate : NSObject
{
    WKWebView *mainView;
    WKWebDataSource *mainDataSource;
    NSMutableDictionary *viewMap;
    NSMutableDictionary *dataSourceMap;
}
@end
