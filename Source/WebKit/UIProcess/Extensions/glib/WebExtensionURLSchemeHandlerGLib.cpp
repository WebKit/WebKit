/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebExtensionURLSchemeHandler.h"

#include <gio/gio.h>

#if ENABLE(WK_WEB_EXTENSIONS)

#include "APIFrameHandle.h"
#include "APIFrameInfo.h"
#include "WebExtensionContext.h"
#include "WebExtensionContextProxyMessages.h"
#include "WebExtensionController.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"
#include <WebCore/ResourceResponse.h>
#include <wtf/WorkerPool.h>
#include <wtf/glib/GRefPtr.h>

namespace WebKit {

class WebPageProxy;

constexpr int notFoundErrorCode = G_IO_ERROR_NOT_FOUND;
constexpr int resourceUnavailableErrorCode = G_IO_ERROR_FAILED;
constexpr int noPermissionErrorCode = G_IO_ERROR_PERMISSION_DENIED;

WebExtensionURLSchemeHandler::WebExtensionURLSchemeHandler(WebExtensionController& controller)
    : m_webExtensionController(controller)
    , m_workerPool(WorkerPool::create("WebExtension URL Scheme Handler Pool"_s))
{
}

void WebExtensionURLSchemeHandler::platformStartTask(WebPageProxy& page, WebURLSchemeTask& task)
{
    GRefPtr<GCancellable> cancellable = adoptGRef(g_cancellable_new());

    RunLoop::mainSingleton().dispatch([this, protectedThis = Ref { *this }, cancellable, page = Ref { page }, task = Ref { task }] {
        if (g_cancellable_is_cancelled(cancellable.get()))
            return;

        URL frameDocumentURL = task->frameInfo().request().url();
        URL requestURL = task->request().url();

        if (task->frameInfo().request().url().isEmpty() || task->frameInfo().request().url().isAboutBlank()) {
            frameDocumentURL = task->request().firstPartyForCookies();

            if (!task->frameInfo().isMainFrame()) {
                if (RefPtr parentFrameHandle = task->frameInfo().parentFrameHandle()) {
                    if (RefPtr parent = WebFrameProxy::webFrame(parentFrameHandle->frameID()))
                        frameDocumentURL = parent->url();
                }
            }
        }

        RefPtr webExtensionController = m_webExtensionController;
        if (!webExtensionController) {
            task->didComplete({ "GIOError"_s, noPermissionErrorCode, { }, nullString() });
            return;
        }

        RefPtr extensionContext = webExtensionController->extensionContext(requestURL);
        if (!extensionContext) {
            // We need to return the same error here, as we do below for URLs that don't match web_accessible_resources.
            // Otherwise, a page tracking extension injected content and watching extension UUIDs across page loads can fingerprint
            // the user and know the same set of extensions are installed and enabled for this user and that website.
            task->didComplete({ "GIOError"_s, noPermissionErrorCode, { }, nullString() });
            return;
        }

        Ref extension = extensionContext->extension();

#if ENABLE(INSPECTOR_EXTENSIONS)
        // Chrome does not require devtools extensions to explicitly list resources as web_accessible_resources.
        if (!frameDocumentURL.protocolIs("inspector-resource"_s) && !protocolHostAndPortAreEqual(frameDocumentURL, requestURL))
#else
        if (!protocolHostAndPortAreEqual(frameDocumentURL, requestURL))
#endif
        {
            if (!extension->isWebAccessibleResource(requestURL, frameDocumentURL)) {
                task->didComplete({ "GIOError"_s, noPermissionErrorCode, { }, nullString() });
                return;
            }
        }

        if (task->frameInfo().isMainFrame() && requestURL == frameDocumentURL) {
            if (!extensionContext->isURLForThisExtension(page->configuration().requiredWebExtensionBaseURL())) {
                task->didComplete({ "GIOError"_s, resourceUnavailableErrorCode, { }, nullString() });
                return;
            }
        }

        auto resourceDataResult = extension->resourceDataForPath(requestURL.path().toString());
        if (!resourceDataResult) {
            extensionContext->recordErrorIfNeeded(resourceDataResult.error());
            String errorMessage = resourceDataResult.error()->localizedDescription();
            task->didComplete({ "GIOError"_s, notFoundErrorCode, { }, WTF::move(errorMessage) });
            return;
        }

        Ref resourceData = resourceDataResult.value();

        auto mimeType = extension->resourceMIMETypeForPath(requestURL.path().toString());
        resourceData = extensionContext->localizedResourceData(WTF::move(resourceData), mimeType).releaseNonNull();

        auto urlResponseMIMEType = extension->resourceMIMETypeForPath(requestURL.path().toString());
        auto urlResponse = WebCore::ResourceResponse(WTF::move(requestURL), WTF::move(urlResponseMIMEType), resourceData->size(), "UTF-8"_s);
        urlResponse.setHTTPHeaderField("Access-Control-Allow-Origin"_s, "*"_s);
        urlResponse.setHTTPHeaderField("Content-Security-Policy"_s, extension->contentSecurityPolicy());
        urlResponse.setHTTPHeaderField("Content-Length"_s, String::number(resourceData->size()));
        urlResponse.setHTTPHeaderField("Content-Type"_s, mimeType);
        urlResponse.setHTTPStatusCode(200);

        task->didReceiveResponse(WTF::move(urlResponse));
        task->didReceiveData(WebCore::SharedBuffer::create(resourceData->span()));
        task->didComplete({ });
    });

    m_operations.set(task, cancellable);
}

void WebExtensionURLSchemeHandler::platformStopTask(WebPageProxy& page, WebURLSchemeTask& task)
{
    auto cancellable = m_operations.take(task);
    if (cancellable)
        g_cancellable_cancel(cancellable.get());
}

void WebExtensionURLSchemeHandler::platformTaskCompleted(WebURLSchemeTask& task)
{
    m_operations.remove(task);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
