/*	
    WKWebController.mm
	Copyright 2001, Apple, Inc. All rights reserved.
*/
#import <WebKit/WKWebController.h>
#import <WebKit/WKWebDataSource.h>
#import <WebKit/WKWebView.h>


@interface WKDefaultWebControllerPrivate : NSObject
{
    WKWebView *mainView;
    WKWebDataSource *mainDataSource;
    NSMutableDictionary *viewMap;
    NSMutableDictionary *dataSourceMap;
}
@end
