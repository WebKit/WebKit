/*
    WebUIDelegatePrivate.h
    Copyright (C) 2003 Apple Computer, Inc. All rights reserved.    
*/

#import <WebKit/WebUIDelegate.h>

@interface NSObject (WebUIDelegatePrivate)

- (void)webViewPrint:(WebView *)sender;

- (float)webViewHeaderHeight:(WebView *)sender;
- (float)webViewFooterHeight:(WebView *)sender;
- (void)webView:(WebView *)sender drawHeaderInRect:(NSRect)rect;
- (void)webView:(WebView *)sender drawFooterInRect:(NSRect)rect;
- (void)webView:(WebView *)webView addMessageToConsole:(NSDictionary *)message;
- (BOOL)webView:(WebView *)webView shouldBeginDragForElement:(NSDictionary *)element pasteboard:(NSPasteboard *)pasteboard mouseDownEvent:(NSEvent *)mouseDownEvent mouseDraggedEvent:(NSEvent *)mouseDraggedEvent;
- (BOOL)webView:(WebView *)webView shouldDetermineDragOperationForDraggingInfo:(id <NSDraggingInfo>)draggingInfo dragOperation:(NSDragOperation *)dragOperation;
- (BOOL)webView:(WebView *)webView shouldProcessDragWithDraggingInfo:(id <NSDraggingInfo>)draggingInfo;

@end
