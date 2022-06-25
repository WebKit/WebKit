/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import <WebKit/WKFoundation.h>

@class WKDownload;

/* @enum WKDownloadRedirectPolicy
 @abstract The policy to pass back to the decision handler from the download:willPerformHTTPRedirection:newRequest:decisionHandler: method.
 @constant WKDownloadRedirectPolicyCancel   Cancel the redirect.
 @constant WKDownloadRedirectPolicyAllow    Allow the redirect.
 */
typedef NS_ENUM(NSInteger, WKDownloadRedirectPolicy) {
    WKDownloadRedirectPolicyCancel,
    WKDownloadRedirectPolicyAllow,
} NS_SWIFT_NAME(WKDownload.RedirectPolicy) WK_API_AVAILABLE(macos(11.3), ios(14.5));

NS_ASSUME_NONNULL_BEGIN

@protocol WKDownloadDelegate <NSObject>

@required

/* @abstract Invoked when the download needs a location to write the downloaded bytes.
 @param download The download for which we need a file to which to write.
 @param response The server response if this download was the result of an HTTP request,
 or a synthesized response for blob downloads.
 @param suggestedFilename The suggested filename.
 @param completionHandler The completion handler you must invoke with
 either a file URL to begin the download or nil to cancel the download.
 @discussion suggestedFilename will often be the same as response.suggestedFilename,
 but web content can specify the suggested download filename.  If the destination file
 URL is non-null, it must be a file that does not exist in a directory that does exist
 and can be written to.
 */
- (void)download:(WKDownload *)download decideDestinationUsingResponse:(NSURLResponse *)response suggestedFilename:(NSString *)suggestedFilename completionHandler:(void (^)(NSURL * _Nullable destination))completionHandler;

@optional

/* @abstract Invoked when the download has received an HTTP redirection response.
 @param download The download that received the redirect.
 @param response The redirection response.
 @param newRequest The new request that will be sent.
 @param completionHandler The completion handler you must invoke to indicate whether or not
 to proceed with the redirection.
 @discussion If you do not implement this method, all server suggested redirects will be taken.
 */
- (void)download:(WKDownload *)download willPerformHTTPRedirection:(NSHTTPURLResponse *)response newRequest:(NSURLRequest *)request decisionHandler:(void (^)(WKDownloadRedirectPolicy))decisionHandler WK_SWIFT_ASYNC_NAME(download(_:decidedPolicyForHTTPRedirection:newRequest:)) WK_SWIFT_ASYNC(4);

/* @abstract Invoked when the download needs to respond to an authentication challenge.
 @param download The download that received the authentication challenge.
 @param challenge The authentication challenge.
 @param completionHandler The completion handler you must invoke to respond to the challenge. The
 disposition argument is one of the constants of the enumerated type
 NSURLSessionAuthChallengeDisposition. When disposition is NSURLSessionAuthChallengeUseCredential,
 the credential argument is the credential to use, or nil to indicate continuing without a
 credential.
 @discussion If you do not implement this method, the web view will respond to the authentication challenge with the NSURLSessionAuthChallengeRejectProtectionSpace disposition.
 */
- (void)download:(WKDownload *)download didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge completionHandler:(void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential * _Nullable))completionHandler WK_SWIFT_ASYNC_NAME(download(_:respondTo:));

/* @abstract Invoked when the download has finished successfully.
 @param download The download that finished.
 */
- (void)downloadDidFinish:(WKDownload *)download;

/* @abstract Invoked when the download has failed.
 @param download The download that has failed.
 @param error The error indicating the failure reason.
 @param resumeData This data can be passed to WKWebView resumeDownloadFromResumeData: to attempt to resume this download.
 */
- (void)download:(WKDownload *)download didFailWithError:(NSError *)error resumeData:(nullable NSData *)resumeData;

@end

NS_ASSUME_NONNULL_END
