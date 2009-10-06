/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#if !defined(ENABLE_DASHBOARD_SUPPORT)
#define ENABLE_DASHBOARD_SUPPORT 1
#endif

// Mail on Tiger expects the old value for WebMenuItemTagSearchInGoogle
#define WebMenuItemTagSearchInGoogle OldWebMenuItemTagSearchWeb

#define WEBMENUITEMTAG_WEBKIT_3_0_SPI_START 2000
enum { 
    // The next three values were used in WebKit 2.0 for SPI. In WebKit 3.0 these are API, with different values.
    OldWebMenuItemTagSearchInSpotlight = 1000,
    OldWebMenuItemTagSearchWeb,
    OldWebMenuItemTagLookUpInDictionary,
    // FIXME: These should move to WebUIDelegate.h as part of the WebMenuItemTag enum there, when we're not in API freeze 
    // Note that these values must be kept aligned with values in WebCore/ContextMenuItem.h
    WebMenuItemTagOpenLink = WEBMENUITEMTAG_WEBKIT_3_0_SPI_START,
    WebMenuItemTagIgnoreGrammar,
    WebMenuItemTagSpellingMenu,
    WebMenuItemTagShowSpellingPanel,
    WebMenuItemTagCheckSpelling,
    WebMenuItemTagCheckSpellingWhileTyping,
    WebMenuItemTagCheckGrammarWithSpelling,
    WebMenuItemTagFontMenu,
    WebMenuItemTagShowFonts,
    WebMenuItemTagBold,
    WebMenuItemTagItalic,
    WebMenuItemTagUnderline,
    WebMenuItemTagOutline,
    WebMenuItemTagStyles,
    WebMenuItemTagShowColors,
    WebMenuItemTagSpeechMenu,
    WebMenuItemTagStartSpeaking,
    WebMenuItemTagStopSpeaking,
    WebMenuItemTagWritingDirectionMenu,
    WebMenuItemTagDefaultDirection,
    WebMenuItemTagLeftToRight,
    WebMenuItemTagRightToLeft,
    WebMenuItemPDFSinglePageScrolling,
    WebMenuItemPDFFacingPagesScrolling,
    WebMenuItemTagInspectElement,
    WebMenuItemTagTextDirectionMenu,
    WebMenuItemTagTextDirectionDefault,
    WebMenuItemTagTextDirectionLeftToRight,
    WebMenuItemTagTextDirectionRightToLeft,
    WebMenuItemTagCorrectSpellingAutomatically,
    WebMenuItemTagSubstitutionsMenu,
    WebMenuItemTagShowSubstitutions,
    WebMenuItemTagSmartCopyPaste,
    WebMenuItemTagSmartQuotes,
    WebMenuItemTagSmartDashes,
    WebMenuItemTagSmartLinks,
    WebMenuItemTagTextReplacement,
    WebMenuItemTagTransformationsMenu,
    WebMenuItemTagMakeUpperCase,
    WebMenuItemTagMakeLowerCase,
    WebMenuItemTagCapitalize,
    WebMenuItemTagChangeBack,
    WebMenuItemTagBaseApplication = 10000
};
@class WebGeolocation;
@class WebSecurityOrigin;

@interface NSObject (WebUIDelegatePrivate)

- (void)webView:(WebView *)webView addMessageToConsole:(NSDictionary *)message;

- (NSView *)webView:(WebView *)webView plugInViewWithArguments:(NSDictionary *)arguments;

#if ENABLE_DASHBOARD_SUPPORT
// regions is an dictionary whose keys are regions label and values are arrays of WebDashboardRegions.
- (void)webView:(WebView *)webView dashboardRegionsChanged:(NSDictionary *)regions;
#endif

- (void)webView:(WebView *)sender dragImage:(NSImage *)anImage at:(NSPoint)viewLocation offset:(NSSize)initialOffset event:(NSEvent *)event pasteboard:(NSPasteboard *)pboard source:(id)sourceObj slideBack:(BOOL)slideFlag forView:(NSView *)view;
- (void)webView:(WebView *)sender didDrawRect:(NSRect)rect;
- (void)webView:(WebView *)sender didScrollDocumentInFrameView:(WebFrameView *)frameView;
// FIXME: If we ever make this method public, it should include a WebFrame parameter.
- (BOOL)webViewShouldInterruptJavaScript:(WebView *)sender;
- (void)webView:(WebView *)sender willPopupMenu:(NSMenu *)menu;
- (void)webView:(WebView *)sender contextMenuItemSelected:(NSMenuItem *)item forElement:(NSDictionary *)element;
- (void)webView:(WebView *)sender saveFrameView:(WebFrameView *)frameView showingPanel:(BOOL)showingPanel;
- (BOOL)webView:(WebView *)sender shouldHaltPlugin:(DOMNode *)pluginNode;
/*!
    @method webView:frame:exceededDatabaseQuotaForSecurityOrigin:database:
    @param sender The WebView sending the delegate method.
    @param frame The WebFrame whose JavaScript initiated this call.
    @param origin The security origin that needs a larger quota.
    @param databaseIdentifier The identifier of the database involved.
*/
- (void)webView:(WebView *)sender frame:(WebFrame *)frame exceededDatabaseQuotaForSecurityOrigin:(WebSecurityOrigin *)origin database:(NSString *)databaseIdentifier;

- (WebView *)webView:(WebView *)sender createWebViewWithRequest:(NSURLRequest *)request windowFeatures:(NSDictionary *)features;

- (BOOL)webView:(WebView *)sender shouldReplaceUploadFile:(NSString *)path usingGeneratedFilename:(NSString **)filename;
- (NSString *)webView:(WebView *)sender generateReplacementFile:(NSString *)path;

- (BOOL)webView:(WebView *)sender frame:(WebFrame *)frame requestGeolocationPermission:(WebGeolocation *)geolocation securityOrigin:(WebSecurityOrigin *)origin;

- (void)webView:(WebView *)sender formStateDidChangeForNode:(DOMNode *)node;
- (void)webView:(WebView *)sender formDidFocusNode:(DOMNode *)node;
- (void)webView:(WebView *)sender formDidBlurNode:(DOMNode *)node;

/*!
    @method webView:printFrame:
    @abstract Informs that a WebFrame needs to be printed
    @param webView The WebView sending the delegate method
    @param frameView The WebFrame needing to be printed
    @discussion This method is called when a script or user requests the page to be printed.
*/
- (void)webView:(WebView *)sender printFrame:(WebFrame *)frame;

@end
