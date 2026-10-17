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

typedef struct {
    WebKit::WebExtensionURLSchemeHandler* handler;
    WebPageProxy* page;
    WebURLSchemeTask* task;
} URLSchemeHandlerAsyncData;

WEBKIT_DEFINE_ASYNC_DATA_STRUCT(URLSchemeHandlerAsyncData);

WebExtensionURLSchemeHandler::WebExtensionURLSchemeHandler(WebExtensionController& controller)
    : m_webExtensionController(controller)
    , m_workerPool(WorkerPool::create("WebExtension URL Scheme Handler Pool"_s))
{
}

void WebExtensionURLSchemeHandler::platformStartTask(WebPageProxy& page, WebURLSchemeTask& task)
{
    URLSchemeHandlerAsyncData* asyncData = createURLSchemeHandlerAsyncData();
    asyncData->handler = this;
    asyncData->page = &page;
    asyncData->task = &task;

    GRefPtr<GCancellable> cancellable = adoptGRef(g_cancellable_new());
    GRefPtr<GTask> operation = adoptGRef(g_task_new(nullptr, cancellable.get(), nullptr, nullptr));

    GTaskThreadFunc taskHandler = [](GTask* task, gpointer, gpointer taskData, GCancellable* cancellable) {
        if (!g_task_return_error_if_cancelled(task))
            return;

        URLSchemeHandlerAsyncData* data = static_cast<URLSchemeHandlerAsyncData*>(taskData);

        URL frameDocumentURL = data->task->frameInfo().request().url();
        URL requestURL = data->task->request().url();

        if (data->task->frameInfo().request().url().isEmpty() || data->task->frameInfo().request().url().isAboutBlank()) {
            frameDocumentURL = data->task->request().firstPartyForCookies();

            if (!data->task->frameInfo().isMainFrame()) {
                if (RefPtr parentFrameHandle = data->task->frameInfo().parentFrameHandle()) {
                    if (RefPtr parent = WebFrameProxy::webFrame(parentFrameHandle->frameID()))
                        frameDocumentURL = parent->url();
                }
            }
        }

        RefPtr webExtensionController = data->handler->m_webExtensionController.get();
        if (!webExtensionController) {
            data->task->didComplete({ "GIOError"_s, noPermissionErrorCode, { }, nullString() });
            g_task_return_new_error(task, g_io_error_quark(), noPermissionErrorCode, nullptr);
            return;
        }

        RefPtr extensionContext = webExtensionController->extensionContext(requestURL);
        if (!extensionContext) {
            // We need to return the same error here, as we do below for URLs that don't match web_accessible_resources.
            // Otherwise, a page tracking extension injected content and watching extension UUIDs across page loads can fingerprint
            // the user and know the same set of extensions are installed and enabled for this user and that website.
            data->task->didComplete({ "GIOError"_s, noPermissionErrorCode, { }, nullString() });
            g_task_return_new_error(task, g_io_error_quark(), noPermissionErrorCode, nullptr);
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
                data->task->didComplete({ "GIOError"_s, noPermissionErrorCode, { }, nullString() });
                g_task_return_new_error(task, g_io_error_quark(), noPermissionErrorCode, nullptr);
                return;
            }
        }

        if (data->task->frameInfo().isMainFrame() && requestURL == frameDocumentURL) {
            if (!extensionContext->isURLForThisExtension(data->page->configuration().requiredWebExtensionBaseURL())) {
                data->task->didComplete({ "GIOError"_s, resourceUnavailableErrorCode, { }, nullString() });
                g_task_return_new_error(task, g_io_error_quark(), resourceUnavailableErrorCode, nullptr);
                return;
            }
        }

        auto resourceDataResult = extension->resourceDataForPath(requestURL.path().toString());
        if (!resourceDataResult) {
            extensionContext->recordErrorIfNeeded(resourceDataResult.error());
            String errorMessage = resourceDataResult.error()->localizedDescription();
            data->task->didComplete({ "GIOError"_s, notFoundErrorCode, { }, WTF::move(errorMessage) });
            g_task_return_new_error(task, g_io_error_quark(), notFoundErrorCode, nullptr);
            return;
        }

        Ref resourceData = resourceDataResult.value();

        auto mimeType = extension->resourceMIMETypeForPath(requestURL.path().toString());
        resourceData = extensionContext->localizedResourceData(WTF::move(resourceData), mimeType).releaseNonNull();

        auto urlResponseMIMEType = extension->resourceMIMETypeForPath(requestURL.path().toString());
        auto urlResponse = WebCore::ResourceResponse(WTF::move(requestURL), WTF::move(urlResponseMIMEType), resourceData->size(), "UTF-8"_s);
        GUniquePtr<SoupMessageHeaders> soupHeaders(soup_message_headers_new(SOUP_MESSAGE_HEADERS_RESPONSE));
        soup_message_headers_replace(soupHeaders.get(), "Access-Control-Allow-Origin", "*");
        soup_message_headers_replace(soupHeaders.get(), "Content-Security-Policy", extension->contentSecurityPolicy().utf8().legacyCStringPointer());
        soup_message_headers_replace(soupHeaders.get(), "Content-Length", String::number(resourceData->size()).utf8().legacyCStringPointer());
        soup_message_headers_replace(soupHeaders.get(), "Content-Type", mimeType.utf8().legacyCStringPointer());
        urlResponse.updateSoupMessageHeaders(soupHeaders.get());

        data->task->didReceiveResponse(WTF::move(urlResponse));
        data->task->didReceiveData(WebCore::SharedBuffer::create(resourceData->span()));
        data->task->didComplete({ });
    };
    g_task_set_task_data(operation.get(), asyncData, reinterpret_cast<GDestroyNotify>(destroyURLSchemeHandlerAsyncData));

    m_operations.set(task, operation);

    g_task_run_in_thread(operation.get(), taskHandler);
}

void WebExtensionURLSchemeHandler::platformStopTask(WebPageProxy& page, WebURLSchemeTask& task)
{
    auto operation = m_operations.take(task);
    if (operation)
        g_cancellable_cancel(g_task_get_cancellable(operation.get()));
}

void WebExtensionURLSchemeHandler::platformTaskCompleted(WebURLSchemeTask& task)
{
    m_operations.remove(task);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
