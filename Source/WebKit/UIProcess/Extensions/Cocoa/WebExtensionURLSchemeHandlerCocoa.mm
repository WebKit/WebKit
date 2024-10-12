/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionURLSchemeHandler.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "APIData.h"
#import "APIError.h"
#import "APIFrameInfo.h"
#import "WKNSData.h"
#import "WKNSError.h"
#import "WKURLSchemeTaskInternal.h"
#import "WKWebViewConfigurationPrivate.h"
#import "WebExtension.h"
#import "WebExtensionContext.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionController.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import "WebURLSchemeTask.h"
#import "_WKWebExtensionLocalization.h"
#import <UniformTypeIdentifiers/UTType.h>
#import <wtf/BlockPtr.h>

namespace WebKit {

class WebPageProxy;

constexpr NSInteger noPermissionErrorCode = NSURLErrorNoPermissionsToReadFile;

WebExtensionURLSchemeHandler::WebExtensionURLSchemeHandler(WebExtensionController& controller)
    : m_webExtensionController(controller)
{
}

void WebExtensionURLSchemeHandler::platformStartTask(WebPageProxy& page, WebURLSchemeTask& task)
{
    auto *operation = [NSBlockOperation blockOperationWithBlock:makeBlockPtr([this, protectedThis = Ref { *this }, &task, protectedTask = Ref { task }, &page, protectedPage = Ref { page }]() {
        // If a frame is loading, the frame request URL will be an empty string, since the request is actually the frame URL being loaded.
        // In this case, consider the firstPartyForCookies() to be the document including the frame. This fails for nested frames, since
        // it is always the main frame URL, not the immediate parent frame.
        // FIXME: <rdar://problem/59193765> Remove this workaround when there is a way to know the proper parent frame.
        URL frameDocumentURL = task.frameInfo().request().url().isEmpty() ? task.request().firstPartyForCookies() : task.frameInfo().request().url();
        URL requestURL = task.request().url();

        if (!m_webExtensionController) {
            task.didComplete([NSError errorWithDomain:NSURLErrorDomain code:noPermissionErrorCode userInfo:nil]);
            return;
        }

        RefPtr extensionContext = m_webExtensionController->extensionContext(requestURL);
        if (!extensionContext) {
            // We need to return the same error here, as we do below for URLs that don't match web_accessible_resources.
            // Otherwise, a page tracking extension injected content and watching extension UUIDs across page loads can fingerprint
            // the user and know the same set of extensions are installed and enabled for this user and that website.
            task.didComplete([NSError errorWithDomain:NSURLErrorDomain code:noPermissionErrorCode userInfo:nil]);
            return;
        }

#if ENABLE(INSPECTOR_EXTENSIONS)
        // Chrome does not require devtools extensions to explicitly list resources as web_accessible_resources.
        if (!frameDocumentURL.protocolIs("inspector-resource"_s) && !protocolHostAndPortAreEqual(frameDocumentURL, requestURL))
#else
        if (!protocolHostAndPortAreEqual(frameDocumentURL, requestURL))
#endif
        {
            if (!extensionContext->extension().isWebAccessibleResource(requestURL, frameDocumentURL)) {
                task.didComplete([NSError errorWithDomain:NSURLErrorDomain code:noPermissionErrorCode userInfo:nil]);
                return;
            }
        }

        bool loadingExtensionMainFrame = false;
        if (task.frameInfo().isMainFrame() && requestURL == frameDocumentURL) {
            if (!extensionContext->isURLForThisExtension(page.configuration().requiredWebExtensionBaseURL())) {
                task.didComplete([NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorResourceUnavailable userInfo:nil]);
                return;
            }

            loadingExtensionMainFrame = true;
        }

        RefPtr<API::Error> error;
        RefPtr resourceData = extensionContext->extension().resourceDataForPath(requestURL.path().toString(), error);
        if (!resourceData || error) {
            extensionContext->recordErrorIfNeeded(wrapper(error));
            task.didComplete([NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorFileDoesNotExist userInfo:nil]);
            return;
        }

        auto *fileData = static_cast<NSData *>(resourceData->wrapper());

        if (loadingExtensionMainFrame) {
            if (auto tab = extensionContext->getTab(page.identifier()))
                extensionContext->addExtensionTabPage(page, *tab);
        }

        auto *mimeType = [UTType typeWithFilenameExtension:((NSURL *)requestURL).pathExtension].preferredMIMEType;
        if (!mimeType)
            mimeType = @"application/octet-stream";

        if ([mimeType isEqualToString:@"text/css"]) {
            // FIXME: <https://webkit.org/b/252628> Only attempt to localize CSS files if we notice a localization wildcard in the file's NSData.
            auto *localization = extensionContext->localization();
            auto *stylesheetContents = [[NSString alloc] initWithData:fileData encoding:NSUTF8StringEncoding];
            stylesheetContents = [localization localizedStringForString:stylesheetContents];
            fileData = [stylesheetContents dataUsingEncoding:NSUTF8StringEncoding];
        }

        auto *urlResponse = [[NSHTTPURLResponse alloc] initWithURL:requestURL statusCode:200 HTTPVersion:nil headerFields:@{
            @"Access-Control-Allow-Origin": @"*",
            @"Content-Security-Policy": extensionContext->extension().contentSecurityPolicy(),
            @"Content-Length": [NSString stringWithFormat:@"%zu", (size_t)fileData.length],
            @"Content-Type": mimeType
        }];

        task.didReceiveResponse(urlResponse);
        task.didReceiveData(WebCore::SharedBuffer::create(fileData));
        task.didComplete({ });
    }).get()];

    m_operations.set(task, operation);

    [NSOperationQueue.mainQueue addOperation:operation];
}

void WebExtensionURLSchemeHandler::platformStopTask(WebPageProxy& page, WebURLSchemeTask& task)
{
    auto operation = m_operations.take(task);
    [operation cancel];
}

void WebExtensionURLSchemeHandler::platformTaskCompleted(WebURLSchemeTask& task)
{
    m_operations.remove(task);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
