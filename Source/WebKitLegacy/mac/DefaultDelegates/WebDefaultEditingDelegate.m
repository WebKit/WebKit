/*
 * Copyright (C) 2005 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKitLegacy/WebDefaultEditingDelegate.h>

#import <WebKitLegacy/DOM.h>
#import <WebKitLegacy/WebEditingDelegate.h>
#import <WebKitLegacy/WebEditingDelegatePrivate.h>
#import <WebKitLegacy/WebView.h>

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

- (BOOL)webView:(WebView *)webView shouldMoveRangeAfterDelete:(DOMRange *)range replacingRange:(DOMRange *)rangeToBeReplaced
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

#if !PLATFORM(IOS_FAMILY)
- (void)webView:(WebView *)webView didWriteSelectionToPasteboard:(NSPasteboard *)pasteboard
{
}
#else
- (NSArray *)supportedPasteboardTypesForCurrentSelection
{
    return nil;
}

- (DOMDocumentFragment *)documentFragmentForPasteboardItemAtIndex:(NSInteger)index
{
    return nil;
}
#endif

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
