/*	
    WebEditingDelegate.h
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.
    
    Public header file.
*/

#import <Cocoa/Cocoa.h>

@class DOMCSSStyleDeclaration;
@class DOMRange;
@class WebView;

typedef enum {
    WebViewInsertActionTyped,	
    WebViewInsertActionPasted,	
    WebViewInsertActionDropped,	
} WebViewInsertAction;

@interface NSObject (WebViewEditingDelegate)
- (BOOL)webView:(WebView *)webView shouldBeginEditingInDOMRange:(DOMRange *)range;
- (BOOL)webView:(WebView *)webView shouldEndEditingInDOMRange:(DOMRange *)range;
- (BOOL)webView:(WebView *)webView shouldInsertNode:(DOMNode *)node replacingDOMRange:(DOMRange *)range givenAction:(WebViewInsertAction)action;
- (BOOL)webView:(WebView *)webView shouldInsertText:(NSString *)text replacingDOMRange:(DOMRange *)range givenAction:(WebViewInsertAction)action;
- (BOOL)webView:(WebView *)webView shouldDeleteDOMRange:(DOMRange *)range;
- (BOOL)webView:(WebView *)webView shouldChangeSelectedDOMRange:(DOMRange *)currentRange toDOMRange:(DOMRange *)proposedRange affinity:(NSSelectionAffinity)selectionAffinity stillSelecting:(BOOL)flag;
- (BOOL)webView:(WebView *)webView shouldApplyStyle:(DOMCSSStyleDeclaration *)style toElementsInDOMRange:(DOMRange *)range;
- (BOOL)webView:(WebView *)webView shouldChangeTypingStyle:(DOMCSSStyleDeclaration *)currentStyle toStyle:(DOMCSSStyleDeclaration *)proposedStyle;
- (BOOL)webView:(WebView *)webView doCommandBySelector:(SEL)selector;
- (void)webViewDidBeginEditing:(NSNotification *)notification;
- (void)webViewDidChange:(NSNotification *)notification;
- (void)webViewDidEndEditing:(NSNotification *)notification;
- (void)webViewDidChangeTypingStyle:(NSNotification *)notification;
- (void)webViewDidChangeSelection:(NSNotification *)notification;
- (NSUndoManager *)undoManagerForWebView:(WebView *)webView;
@end

