/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#import <WebKit/WebUIDelegate.h>

@interface NSObject (WebUIDelegatePrivate)

// webViewPrint: is obsolete; delegates should respond to webView:printFrameView: instead
- (void)webViewPrint:(WebView *)sender;
- (void)webView:(WebView *)sender printFrameView:(WebFrameView *)frameView;

- (float)webViewHeaderHeight:(WebView *)sender;
- (float)webViewFooterHeight:(WebView *)sender;
- (void)webView:(WebView *)sender drawHeaderInRect:(NSRect)rect;
- (void)webView:(WebView *)sender drawFooterInRect:(NSRect)rect;
- (void)webView:(WebView *)webView addMessageToConsole:(NSDictionary *)message;

// regions is an dictionary whose keys are regions label and values are arrays of WebDashboardRegions.
- (void)webView:(WebView *)webView dashboardRegionsChanged:(NSDictionary *)regions;

- (WebView *)webView:(WebView *)sender createWebViewModalDialogWithRequest:(NSURLRequest *)request;
- (void)webViewRunModal:(WebView *)sender;

// These three methods are replacements for the existing ones that lack the frame parameter. The old
// methods should be deprecated.

/*!
     @method webView:runJavaScriptAlertPanelWithMessage:initiatedByFrame:
     @abstract Display a JavaScript alert panel.
     @param sender The WebView sending the delegate method.
     @param message The message to display.
     @param frame The WebFrame whose JavaScript initiated this call.
     @discussion Clients should visually indicate that this panel comes
     from JavaScript initiated by the specified frame. The panel should have 
     a single OK button.
*/
- (void)webView:(WebView *)sender runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame;

/*!
     @method webView:runJavaScriptConfirmPanelWithMessage:initiatedByFrame:
     @abstract Display a JavaScript confirm panel.
     @param sender The WebView sending the delegate method.
     @param message The message to display.
     @param frame The WebFrame whose JavaScript initiated this call.
     @result YES if the user hit OK, NO if the user chose Cancel.
     @discussion Clients should visually indicate that this panel comes
     from JavaScript initiated by the specified frame. The panel should have 
     two buttons, e.g. "OK" and "Cancel".
*/
- (BOOL)webView:(WebView *)sender runJavaScriptConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame;

/*!
     @method webView:runJavaScriptTextInputPanelWithPrompt:defaultText:initiatedByFrame:
     @abstract Display a JavaScript text input panel.
     @param sender The WebView sending the delegate method.
     @param message The message to display.
     @param defaultText The initial text for the text entry area.
     @param frame The WebFrame whose JavaScript initiated this call.
     @result The typed text if the user hit OK, otherwise nil.
     @discussion Clients should visually indicate that this panel comes
     from JavaScript initiated by the specified frame. The panel should have 
     two buttons, e.g. "OK" and "Cancel", and an area to type text.
*/
- (NSString *)webView:(WebView *)sender runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText initiatedByFrame:(WebFrame *)frame;

/*!
     @method webView:runJavaScriptConfirmPanelWithMessage:initiatedByFrame:
     @abstract Display a confirm panel by an "before unload" event handler.
     @param sender The WebView sending the delegate method.
     @param message The message to display.
     @param frame The WebFrame whose JavaScript initiated this call.
     @result YES if the user hit OK, NO if the user chose Cancel.
     @discussion Clients should include a message in addition to the one
     supplied by the web page that indicates. The panel should have 
     two buttons, e.g. "OK" and "Cancel".
*/
// FIXME: Should we indicate a distinction between navigation and close?
- (BOOL)webView:(WebView *)sender runBeforeUnloadConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame;

@end
