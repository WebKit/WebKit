/*	
    WebDefaultEditingDelegate.m
    Copyright 2004, Apple Computer, Inc.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/WebDefaultEditingDelegate.h>

#import <WebKit/DOM.h>
#import <WebKit/WebEditingDelegate.h>
#import <WebKit/WebView.h>

@implementation WebDefaultEditingDelegate

static WebDefaultEditingDelegate *sharedDelegate = nil;

+ (WebDefaultEditingDelegate *)sharedEditingDelegate
{
    if (!sharedDelegate) {
        sharedDelegate = [[WebDefaultEditingDelegate alloc] init];
    }
    return sharedDelegate;
}

- (BOOL)webView:(WebView *)webView shouldBeginEditingInDOMRange:(DOMRange *)range
{
    return YES;
}

- (BOOL)webView:(WebView *)webView shouldEndEditingInDOMRange:(DOMRange *)range
{
    return YES;
}

- (BOOL)webView:(WebView *)webView shouldInsertNode:(DOMNode *)node replacingDOMRange:(DOMRange *)range givenAction:(WebViewInsertAction)action
{
    return YES;
}

- (BOOL)webView:(WebView *)webView shouldInsertText:(NSString *)text replacingDOMRange:(DOMRange *)range givenAction:(WebViewInsertAction)action
{
    return YES;
}

- (BOOL)webView:(WebView *)webView shouldDeleteDOMRange:(DOMRange *)range
{
    return YES;
}

- (BOOL)webView:(WebView *)webView shouldChangeSelectedDOMRange:(DOMRange *)currentRange toDOMRange:(DOMRange *)proposedRange affinity:(NSSelectionAffinity)selectionAffinity stillSelecting:(BOOL)flag
{
    return YES;
}

- (BOOL)webView:(WebView *)webView shouldApplyStyle:(DOMCSSStyleDeclaration *)style toElementsInDOMRange:(DOMRange *)range
{
    return YES;
}

- (BOOL)webView:(WebView *)webView shouldChangeTypingStyle:(DOMCSSStyleDeclaration *)currentStyle toStyle:(DOMCSSStyleDeclaration *)proposedStyle
{
    return YES;
}

- (BOOL)webView:(WebView *)webView doCommandBySelector:(SEL)selector
{
    return NO;
}

- (void)webViewDidBeginEditing:(NSNotification *)notification
{
}

- (void)webViewDidChange:(NSNotification *)notification
{
}

- (void)webViewDidEndEditing:(NSNotification *)notification
{
}

- (void)webViewDidChangeTypingStyle:(NSNotification *)notification
{
}

- (void)webViewDidChangeSelection:(NSNotification *)notification
{
}

- (NSUndoManager *)undoManagerForWebView:(WebView *)webView
{
    return nil;
}

@end
