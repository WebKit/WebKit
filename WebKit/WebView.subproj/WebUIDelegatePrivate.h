/*
    WebUIDelegatePrivate.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
*/

#import <WebKit/WebUIDelegate.h>

@interface NSObject (WebUIDelegatePrivate)

- (void)webViewPrint:(WebView *)sender;

- (float)webViewHeaderHeight:(WebView *)sender;
- (float)webViewFooterHeight:(WebView *)sender;
- (void)webView:(WebView *)sender drawHeaderInRect:(NSRect)rect forPage:(unsigned)pageIndex of:(unsigned)pageCount;
- (void)webView:(WebView *)sender drawFooterInRect:(NSRect)rect forPage:(unsigned)pageIndex of:(unsigned)pageCount;

@end
