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
#import "WKInspectorResourceURLSchemeHandler.h"

#if PLATFORM(MAC)

#import "Logging.h"
#import "WKURLSchemeTask.h"
#import "WebInspectorUIProxy.h"
#import "WebURLSchemeHandlerCocoa.h"
#import <WebCore/MIMETypeRegistry.h>
#import <wtf/Assertions.h>
#import <wtf/darwin/DispatchExtras.h>

@implementation WKInspectorResourceURLSchemeHandler {
    RetainPtr<NSMapTable<id <WKURLSchemeTask>, NSOperation *>> _fileLoadOperations;
    RetainPtr<NSBundle> _cachedBundle;
    
    RetainPtr<NSSet<NSString *>> _allowedURLSchemesForCSP;
    RetainPtr<NSSet<NSURL *>> _mainResourceURLsForCSP;
}

- (NSSet<NSString *> *)allowedURLSchemesForCSP
{
    return _allowedURLSchemesForCSP.get();
}

- (void)setAllowedURLSchemesForCSP:(NSSet<NSString *> *)allowedURLSchemes
{
    _allowedURLSchemesForCSP = adoptNS([allowedURLSchemes copy]);
}

- (NSSet<NSURL *> *)mainResourceURLsForCSP
{
    if (!_mainResourceURLsForCSP)
        _mainResourceURLsForCSP = adoptNS([[NSSet alloc] initWithObjects:adoptNS([[NSURL alloc] initWithString:WebKit::WebInspectorUIProxy::inspectorPageURL().createNSString().get()]).get(), adoptNS([[NSURL alloc] initWithString:WebKit::WebInspectorUIProxy::inspectorTestPageURL().createNSString().get()]).get(), nil]);

    return _mainResourceURLsForCSP.get();
}

// MARK - WKURLSchemeHandler Protocol

- (void)webView:(WKWebView *)webView startURLSchemeTask:(id <WKURLSchemeTask>)urlSchemeTask
{
    dispatch_assert_queue(mainDispatchQueueSingleton());
    if (!_cachedBundle) {
        _cachedBundle = [NSBundle bundleWithIdentifier:@"com.apple.WebInspectorUI"];

        // It is an error if WebInspectorUI has not already been soft-linked by the time
        // we load resources from it. And if soft-linking fails, we shouldn't start loads.
        RELEASE_ASSERT(_cachedBundle);
    }

    if (!_fileLoadOperations)
        _fileLoadOperations = adoptNS([[NSMapTable alloc] initWithKeyOptions:NSPointerFunctionsStrongMemory valueOptions:NSPointerFunctionsStrongMemory capacity:5]);

    RetainPtr operation = [NSBlockOperation blockOperationWithBlock:^{
        [_fileLoadOperations removeObjectForKey:urlSchemeTask];

        NSURL *requestURL = urlSchemeTask.request.URL;
        NSURL *fileURLForRequest = [_cachedBundle URLForResource:requestURL.relativePath withExtension:@""];
        if (!fileURLForRequest) {
            LOG_ERROR("Unable to find Web Inspector resource: %@", requestURL.absoluteString);
            [urlSchemeTask didFailWithError:[NSError errorWithDomain:NSCocoaErrorDomain code:NSURLErrorFileDoesNotExist userInfo:nil]];
            return;
        }

        NSError *readError;
        NSData *fileData = [NSData dataWithContentsOfURL:fileURLForRequest options:0 error:&readError];
        if (!fileData) {
            LOG_ERROR("Unable to read data for Web Inspector resource: %@", requestURL.absoluteString);
            [urlSchemeTask didFailWithError:[NSError errorWithDomain:NSCocoaErrorDomain code:NSURLErrorResourceUnavailable userInfo:@{
                NSUnderlyingErrorKey: readError,
            }]];
            return;
        }

        RetainPtr mimeType = WebCore::MIMETypeRegistry::mimeTypeForExtension(String(fileURLForRequest.pathExtension)).createNSString();
        if (!mimeType)
            mimeType = @"application/octet-stream";

        RetainPtr<NSMutableDictionary> headerFields = adoptNS(@{
            @"Access-Control-Allow-Origin": @"*",
            @"Content-Length": adoptNS([[NSString alloc] initWithFormat:@"%zu", (size_t)fileData.length]).get(),
            @"Content-Type": mimeType.get(),
        }.mutableCopy);

        // Allow fetches for resources that use a registered custom URL scheme.
        if (_allowedURLSchemesForCSP && [self.mainResourceURLsForCSP containsObject:requestURL]) {
            RetainPtr listOfCustomProtocols = adoptNS([[NSString alloc] initWithFormat:@"%@:", [_allowedURLSchemesForCSP.get().allObjects componentsJoinedByString:@": "]]);
            RetainPtr stringForCSPPolicy = adoptNS([[NSString alloc] initWithFormat:@"connect-src * %@; img-src * file: blob: resource: %@", listOfCustomProtocols.get(), listOfCustomProtocols.get()]);
            [headerFields setObject:stringForCSPPolicy.get() forKey:@"Content-Security-Policy"];
        }

        RetainPtr<NSHTTPURLResponse> urlResponse = adoptNS([[NSHTTPURLResponse alloc] initWithURL:urlSchemeTask.request.URL statusCode:200 HTTPVersion:nil headerFields:headerFields.get()]);
        [urlSchemeTask didReceiveResponse:urlResponse.get()];
        [urlSchemeTask didReceiveData:fileData];
        [urlSchemeTask didFinish];
    }];
    
    [_fileLoadOperations setObject:operation.get() forKey:urlSchemeTask];
    [[NSOperationQueue mainQueue] addOperation:operation.get()];
}

- (void)webView:(WKWebView *)webView stopURLSchemeTask:(id <WKURLSchemeTask>)urlSchemeTask
{
    dispatch_assert_queue(mainDispatchQueueSingleton());
    if (NSOperation *operation = [_fileLoadOperations objectForKey:urlSchemeTask]) {
        [operation cancel];
        [_fileLoadOperations removeObjectForKey:urlSchemeTask];
    }
}

@end

#endif // PLATFORM(MAC)
