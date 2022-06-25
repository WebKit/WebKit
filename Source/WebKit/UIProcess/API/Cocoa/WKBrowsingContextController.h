/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <Foundation/Foundation.h>
#import <WebKit/WKBrowsingContextGroup.h>
#import <WebKit/WKFoundation.h>
#import <WebKit/WKProcessGroup.h>

@class WKBackForwardList;
@class WKBackForwardListItem;
@protocol WKBrowsingContextHistoryDelegate;
@protocol WKBrowsingContextLoadDelegate;
@protocol WKBrowsingContextPolicyDelegate;

WK_CLASS_DEPRECATED_WITH_REPLACEMENT("WKWebView", macos(10.10, 10.14.4), ios(8.0, 12.2))
@interface WKBrowsingContextController : NSObject

#pragma mark Delegates

@property (weak) id <WKBrowsingContextLoadDelegate> loadDelegate;
@property (weak) id <WKBrowsingContextPolicyDelegate> policyDelegate;
@property (weak) id <WKBrowsingContextHistoryDelegate> historyDelegate;

#pragma mark Loading

+ (void)registerSchemeForCustomProtocol:(NSString *)scheme WK_API_DEPRECATED_WITH_REPLACEMENT("WKURLSchemeHandler", macos(10.10, 10.14.4), ios(8.0, 12.2));
+ (void)unregisterSchemeForCustomProtocol:(NSString *)scheme WK_API_DEPRECATED_WITH_REPLACEMENT("WKURLSchemeHandler", macos(10.10, 10.14.4), ios(8.0, 12.2));

/* Load a request. This is only valid for requests of non-file: URLs. Passing a
   file: URL will throw an exception. */
- (void)loadRequest:(NSURLRequest *)request;
- (void)loadRequest:(NSURLRequest *)request userData:(id)userData;

/* Load a file: URL. Opens the sandbox only for files within allowedDirectory.
    - Passing a non-file: URL to either parameter will yield an exception.
    - Passing nil as the allowedDirectory will open the entire file-system for
      reading.
*/
- (void)loadFileURL:(NSURL *)URL restrictToFilesWithin:(NSURL *)allowedDirectory;
- (void)loadFileURL:(NSURL *)URL restrictToFilesWithin:(NSURL *)allowedDirectory userData:(id)userData;

/* Load a webpage using the passed in string as its contents. */
- (void)loadHTMLString:(NSString *)HTMLString baseURL:(NSURL *)baseURL;
- (void)loadHTMLString:(NSString *)HTMLString baseURL:(NSURL *)baseURL userData:(id)userData;

- (void)loadAlternateHTMLString:(NSString *)string baseURL:(NSURL *)baseURL forUnreachableURL:(NSURL *)unreachableURL;

/* Load a webpage using the passed in data as its contents. */
- (void)loadData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)baseURL;
- (void)loadData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)baseURL userData:(id)userData;

/* Stops the load associated with the active URL. */
- (void)stopLoading;

/* Reload the currently active URL. */
- (void)reload;

/* Reload the currently active URL, bypassing caches. */
- (void)reloadFromOrigin;

@property (copy) NSString *applicationNameForUserAgent;
@property (copy) NSString *customUserAgent;

#pragma mark Back/Forward

/* Go to the next webpage in the back/forward list. */
- (void)goForward;

/* Whether there is a next webpage in the back/forward list. */
@property(readonly) BOOL canGoForward;

/* Go to the previous webpage in the back/forward list. */
- (void)goBack;

/* Whether there is a previous webpage in the back/forward list. */
@property(readonly) BOOL canGoBack;

- (void)goToBackForwardListItem:(WKBackForwardListItem *)item;

@property(readonly) WKBackForwardList *backForwardList;

#pragma mark Active Load Introspection

@property (readonly, getter=isLoading) BOOL loading;

/* URL for the active load. This is the URL that should be shown in user interface. */
@property(readonly) NSURL *activeURL;

/* URL for a request that has been sent, but no response has been received yet. */
@property(readonly) NSURL *provisionalURL;

/* URL for a request that has been received, and is now being used. */
@property(readonly) NSURL *committedURL;

@property(readonly) NSURL *unreachableURL;

@property(readonly) double estimatedProgress;

#pragma mark Active Document Introspection

/* Title of the document associated with the active load. */
@property(readonly) NSString *title;

@property (readonly) NSArray *certificateChain;

#pragma mark Zoom

/* Sets the text zoom for the active URL. */
@property CGFloat textZoom;

/* Sets the text zoom for the active URL. */
@property CGFloat pageZoom;

@end
