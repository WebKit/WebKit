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

#import <WebKit/WKFoundation.h>

#import <Foundation/Foundation.h>
#import <WebKit/WKBrowsingContextController.h>

@class WKBackForwardListItem;

WK_CLASS_DEPRECATED_WITH_REPLACEMENT("WKNavigationDelegate", macos(10.10, 10.14.4), ios(8.0, 12.2))
@protocol WKBrowsingContextLoadDelegate <NSObject>
@optional

/* Sent when the provisional load begins. */
- (void)browsingContextControllerDidStartProvisionalLoad:(WKBrowsingContextController *)sender;

/* Sent if a server-side redirect was recieved. */
- (void)browsingContextControllerDidReceiveServerRedirectForProvisionalLoad:(WKBrowsingContextController *)sender;

/* Sent if the provisional load fails. */
- (void)browsingContextController:(WKBrowsingContextController *)sender didFailProvisionalLoadWithError:(NSError *)error;

/* Sent when the load gets committed. */
- (void)browsingContextControllerDidCommitLoad:(WKBrowsingContextController *)sender;

/* Sent when the load completes. */
- (void)browsingContextControllerDidFinishLoad:(WKBrowsingContextController *)sender;

/* Sent if the commited load fails. */
- (void)browsingContextController:(WKBrowsingContextController *)sender didFailLoadWithError:(NSError *)error;

- (void)browsingContextControllerDidStartProgress:(WKBrowsingContextController *)sender WK_API_DEPRECATED_WITH_REPLACEMENT("WKWebView.estimatedProgress", macos(10.10, 10.14.4), ios(8.0, 12.2));
- (void)browsingContextController:(WKBrowsingContextController *)sender estimatedProgressChangedTo:(double)estimatedProgress WK_API_DEPRECATED_WITH_REPLACEMENT("WKWebView.estimatedProgress", macos(10.10, 10.14.4), ios(8.0, 12.2));
- (void)browsingContextControllerDidFinishProgress:(WKBrowsingContextController *)sender WK_API_DEPRECATED_WITH_REPLACEMENT("WKWebView.estimatedProgress", macos(10.10, 10.14.4), ios(8.0, 12.2));

- (void)browsingContextControllerDidChangeBackForwardList:(WKBrowsingContextController *)sender addedItem:(WKBackForwardListItem *)addedItem removedItems:(NSArray *)removedItems WK_API_DEPRECATED_WITH_REPLACEMENT("WKWebView.backForwardList", macos(10.10, 10.14.4), ios(8.0, 12.2));

@end
