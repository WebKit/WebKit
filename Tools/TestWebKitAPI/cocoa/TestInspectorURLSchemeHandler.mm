/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import "config.h"
#import "TestInspectorURLSchemeHandler.h"

#import <WebCore/MIMETypeRegistry.h>
#import <WebKit/WKURLSchemeTask.h>
#import <wtf/Assertions.h>

// Note: this class is a simplified version of WKResourceURLSchemeHandler for testing purposes.

@implementation TestInspectorURLSchemeHandler {
    RetainPtr<NSMapTable<id <WKURLSchemeTask>, NSOperation *>> _fileLoadOperations;
    RetainPtr<NSBundle> _cachedBundle;
    RetainPtr<NSOperationQueue> _operationQueue;
}

// MARK - WKURLSchemeHandler Protocol

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)urlSchemeTask
{
    if (!_cachedBundle)
        _cachedBundle = [NSBundle mainBundle];

    if (!_fileLoadOperations)
        _fileLoadOperations = adoptNS([[NSMapTable alloc] initWithKeyOptions:NSPointerFunctionsStrongMemory valueOptions:NSPointerFunctionsStrongMemory capacity:5]);

    if (!_operationQueue) {
        _operationQueue = adoptNS([[NSOperationQueue alloc] init]);
        _operationQueue.get().underlyingQueue = dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0);
        _operationQueue.get().qualityOfService = NSOperationQualityOfServiceUserInteractive;

        // The default value (NSOperationQueueDefaultMaxConcurrentOperationCount) results in a large number of threads
        // that can exceed the soft limit if two Web Inspector instances are being loaded simultaneously.
        _operationQueue.get().maxConcurrentOperationCount = 4;
    }

    NSBlockOperation *operation = [NSBlockOperation blockOperationWithBlock:^{
        dispatch_async(dispatch_get_main_queue(), ^{
            [_fileLoadOperations removeObjectForKey:urlSchemeTask];
        });

        NSURL *requestURL = urlSchemeTask.request.URL;
        NSURL *fileURLForRequest = [_cachedBundle URLForResource:requestURL.relativePath withExtension:@"" subdirectory:@"TestWebKitAPI.resources"];
        if (!fileURLForRequest) {
            [urlSchemeTask didFailWithError:[NSError errorWithDomain:NSCocoaErrorDomain code:NSURLErrorFileDoesNotExist userInfo:nil]];
            return;
        }

        NSError *readError;
        NSData *fileData = [NSData dataWithContentsOfURL:fileURLForRequest options:0 error:&readError];
        if (!fileData) {
            [urlSchemeTask didFailWithError:[NSError errorWithDomain:NSCocoaErrorDomain code:NSURLErrorResourceUnavailable userInfo:@{
                NSUnderlyingErrorKey: readError,
            }]];
            return;
        }

        NSString *mimeType = WebCore::MIMETypeRegistry::mimeTypeForExtension(String(fileURLForRequest.pathExtension));
        if (!mimeType)
            mimeType = @"application/octet-stream";

        RetainPtr<NSMutableDictionary> headerFields = adoptNS(@{
            @"Access-Control-Allow-Origin": @"*",
            @"Content-Length": [NSString stringWithFormat:@"%zu", (size_t)fileData.length],
            @"Content-Type": mimeType,
        }.mutableCopy);

        RetainPtr<NSHTTPURLResponse> urlResponse = adoptNS([[NSHTTPURLResponse alloc] initWithURL:urlSchemeTask.request.URL statusCode:200 HTTPVersion:nil headerFields:headerFields.get()]);
        [urlSchemeTask didReceiveResponse:urlResponse.get()];
        [urlSchemeTask didReceiveData:fileData];
        [urlSchemeTask didFinish];
    }];
    
    [_fileLoadOperations setObject:operation forKey:urlSchemeTask];
    [_operationQueue addOperation:operation];
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)urlSchemeTask
{
    // Ensure that all blocks with pending removals are dispatched before doing a map lookup.
    dispatch_async(dispatch_get_main_queue(), ^{
        if (NSOperation *operation = [_fileLoadOperations objectForKey:urlSchemeTask]) {
            [operation cancel];
            [_fileLoadOperations removeObjectForKey:urlSchemeTask];
        }
    });
}

@end
