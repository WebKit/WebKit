/*	
        WebControllerSets.h
	Copyright 2002, Apple Computer, Inc.

*/

#import <Foundation/Foundation.h>

@class WebView;

@interface WebViewSets : NSObject
+ (void)addWebView:(WebView *)webView toSetNamed:(NSString *)name;
+ (void)removeWebView:(WebView *)webView fromSetNamed:(NSString *)name;
+ (NSEnumerator *)webViewsInSetNamed:(NSString *)name;
+ (void)makeWebViewsPerformSelector:(SEL)selector;
@end



