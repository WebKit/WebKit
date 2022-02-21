/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebInspectorUIExtensionControllerProxy.h"

#if ENABLE(INSPECTOR_EXTENSIONS)

#include "APIInspectorExtension.h"
#include "APIURL.h"
#include "WebInspectorUIExtensionControllerMessages.h"
#include "WebInspectorUIExtensionControllerProxyMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"

namespace WebKit {

WebInspectorUIExtensionControllerProxy::WebInspectorUIExtensionControllerProxy(WebPageProxy& inspectorPage)
    : m_inspectorPage(inspectorPage)
{
    m_inspectorPage->process().addMessageReceiver(Messages::WebInspectorUIExtensionControllerProxy::messageReceiverName(), m_inspectorPage->webPageID(), *this);
}

WebInspectorUIExtensionControllerProxy::~WebInspectorUIExtensionControllerProxy()
{
    auto callbacks = std::exchange(m_frontendLoadedCallbackQueue, { });
    for (auto& callback : callbacks)
        callback();

    // At this point, we should have already been notified that the frontend has closed.
    ASSERT(!m_inspectorPage);
}

Ref<WebInspectorUIExtensionControllerProxy> WebInspectorUIExtensionControllerProxy::create(WebPageProxy& inspectorPage)
{
    return adoptRef(*new WebInspectorUIExtensionControllerProxy(inspectorPage));
}

void WebInspectorUIExtensionControllerProxy::whenFrontendHasLoaded(Function<void()>&& callback)
{
    if (m_frontendLoaded && m_inspectorPage) {
        callback();
        return;
    }

    m_frontendLoadedCallbackQueue.append(WTFMove(callback));
}

void WebInspectorUIExtensionControllerProxy::inspectorFrontendLoaded()
{
    ASSERT(m_inspectorPage);

    m_frontendLoaded = true;

    auto callbacks = std::exchange(m_frontendLoadedCallbackQueue, { });
    for (auto& callback : callbacks)
        callback();
}

void WebInspectorUIExtensionControllerProxy::inspectorFrontendWillClose()
{
    if (!m_inspectorPage)
        return;

    m_inspectorPage->process().removeMessageReceiver(Messages::WebInspectorUIExtensionControllerProxy::messageReceiverName(), m_inspectorPage->webPageID());
    m_inspectorPage = nullptr;

    m_extensionAPIObjectMap.clear();
}

// API

void WebInspectorUIExtensionControllerProxy::registerExtension(const Inspector::ExtensionID& extensionID, const String& extensionBundleIdentifier, const String& displayName, WTF::CompletionHandler<void(Expected<RefPtr<API::InspectorExtension>, Inspector::ExtensionError>)>&& completionHandler)
{
    whenFrontendHasLoaded([weakThis = WeakPtr { *this }, extensionID, extensionBundleIdentifier, displayName, completionHandler = WTFMove(completionHandler)] () mutable {
        if (!weakThis || !weakThis->m_inspectorPage) {
            completionHandler(makeUnexpected(Inspector::ExtensionError::InvalidRequest));
            return;
        }

        weakThis->m_inspectorPage->sendWithAsyncReply(Messages::WebInspectorUIExtensionController::RegisterExtension { extensionID, extensionBundleIdentifier, displayName }, [strongThis = Ref { *weakThis.get() }, extensionID, completionHandler = WTFMove(completionHandler)](Expected<void, Inspector::ExtensionError> result) mutable {
            if (!result) {
                completionHandler(makeUnexpected(Inspector::ExtensionError::RegistrationFailed));
                return;
            }

            if (!strongThis->m_inspectorPage) {
                completionHandler(makeUnexpected(Inspector::ExtensionError::ContextDestroyed));
                return;
            }

            RefPtr<API::InspectorExtension> extensionAPIObject = API::InspectorExtension::create(extensionID, strongThis.get());
            strongThis->m_extensionAPIObjectMap.set(extensionID, extensionAPIObject.copyRef());

            completionHandler(WTFMove(extensionAPIObject));
        });
    });
}

void WebInspectorUIExtensionControllerProxy::unregisterExtension(const Inspector::ExtensionID& extensionID, WTF::CompletionHandler<void(Expected<void, Inspector::ExtensionError>)>&& completionHandler)
{
    whenFrontendHasLoaded([weakThis = WeakPtr { *this }, extensionID, completionHandler = WTFMove(completionHandler)] () mutable {
        if (!weakThis || !weakThis->m_inspectorPage) {
            completionHandler(makeUnexpected(Inspector::ExtensionError::InvalidRequest));
            return;
        }

        weakThis->m_inspectorPage->sendWithAsyncReply(Messages::WebInspectorUIExtensionController::UnregisterExtension { extensionID }, [strongThis = Ref { *weakThis.get() }, extensionID, completionHandler = WTFMove(completionHandler)](Expected<void, Inspector::ExtensionError> result) mutable {
            if (!result) {
                completionHandler(makeUnexpected(Inspector::ExtensionError::InvalidRequest));
                return;
            }

            if (!strongThis->m_inspectorPage) {
                completionHandler(makeUnexpected(Inspector::ExtensionError::ContextDestroyed));
                return;
            }

            strongThis->m_extensionAPIObjectMap.take(extensionID);

            completionHandler(WTFMove(result));
        });
    });
}

void WebInspectorUIExtensionControllerProxy::createTabForExtension(const Inspector::ExtensionID& extensionID, const String& tabName, const URL& tabIconURL, const URL& sourceURL, WTF::CompletionHandler<void(Expected<Inspector::ExtensionTabID, Inspector::ExtensionError>)>&& completionHandler)
{
    whenFrontendHasLoaded([weakThis = WeakPtr { *this }, extensionID, tabName, tabIconURL, sourceURL, completionHandler = WTFMove(completionHandler)] () mutable {
        if (!weakThis || !weakThis->m_inspectorPage) {
            completionHandler(makeUnexpected(Inspector::ExtensionError::InvalidRequest));
            return;
        }

        weakThis->m_inspectorPage->sendWithAsyncReply(Messages::WebInspectorUIExtensionController::CreateTabForExtension { extensionID, tabName, tabIconURL, sourceURL }, WTFMove(completionHandler));
    });
}

void WebInspectorUIExtensionControllerProxy::evaluateScriptForExtension(const Inspector::ExtensionID& extensionID, const String& scriptSource, const std::optional<WTF::URL>& frameURL, const std::optional<WTF::URL>& contextSecurityOrigin, const std::optional<bool>& useContentScriptContext, WTF::CompletionHandler<void(Inspector::ExtensionEvaluationResult)>&& completionHandler)
{
    whenFrontendHasLoaded([weakThis = WeakPtr { *this }, extensionID, scriptSource, frameURL, contextSecurityOrigin, useContentScriptContext, completionHandler = WTFMove(completionHandler)] () mutable {
        if (!weakThis || !weakThis->m_inspectorPage) {
            completionHandler(makeUnexpected(Inspector::ExtensionError::ContextDestroyed));
            return;
        }

        weakThis->m_inspectorPage->sendWithAsyncReply(Messages::WebInspectorUIExtensionController::EvaluateScriptForExtension {extensionID, scriptSource, frameURL, contextSecurityOrigin, useContentScriptContext}, [completionHandler = WTFMove(completionHandler)](const IPC::DataReference& dataReference, const std::optional<WebCore::ExceptionDetails>& details, const std::optional<Inspector::ExtensionError> error) mutable {
            if (error) {
                completionHandler(makeUnexpected(error.value()));
                return;
            }

            if (details) {
                Expected<RefPtr<API::SerializedScriptValue>, WebCore::ExceptionDetails> returnedValue = makeUnexpected(details.value());
                return completionHandler({ returnedValue });
            }

            completionHandler({ { API::SerializedScriptValue::adopt(Vector { dataReference.data(), dataReference.size() }).ptr() } });
        });
    });
}

void WebInspectorUIExtensionControllerProxy::reloadForExtension(const Inspector::ExtensionID& extensionID, const std::optional<bool>& ignoreCache, const std::optional<String>& userAgent, const std::optional<String>& injectedScript, WTF::CompletionHandler<void(Inspector::ExtensionEvaluationResult)>&& completionHandler)
{
    whenFrontendHasLoaded([weakThis = WeakPtr { *this }, extensionID, ignoreCache, userAgent, injectedScript, completionHandler = WTFMove(completionHandler)] () mutable {
        if (!weakThis || !weakThis->m_inspectorPage) {
            completionHandler(makeUnexpected(Inspector::ExtensionError::ContextDestroyed));
            return;
        }

        weakThis->m_inspectorPage->sendWithAsyncReply(Messages::WebInspectorUIExtensionController::ReloadForExtension {extensionID, ignoreCache, userAgent, injectedScript}, [completionHandler = WTFMove(completionHandler)](const std::optional<Inspector::ExtensionError> error) mutable {
            if (error) {
                completionHandler(makeUnexpected(error.value()));
                return;
            }

            completionHandler({ });
        });
    });
}

void WebInspectorUIExtensionControllerProxy::showExtensionTab(const Inspector::ExtensionTabID& extensionTabIdentifier, CompletionHandler<void(Expected<void, Inspector::ExtensionError>)>&& completionHandler)
{
    whenFrontendHasLoaded([weakThis = WeakPtr { *this }, extensionTabIdentifier, completionHandler = WTFMove(completionHandler)] () mutable {
        if (!weakThis || !weakThis->m_inspectorPage) {
            completionHandler(makeUnexpected(Inspector::ExtensionError::ContextDestroyed));
            return;
        }

        weakThis->m_inspectorPage->sendWithAsyncReply(Messages::WebInspectorUIExtensionController::ShowExtensionTab { extensionTabIdentifier }, WTFMove(completionHandler));
    });
}

// API for testing.

void WebInspectorUIExtensionControllerProxy::evaluateScriptInExtensionTab(const Inspector::ExtensionTabID& extensionTabID, const String& scriptSource, WTF::CompletionHandler<void(Inspector::ExtensionEvaluationResult)>&& completionHandler)
{
    whenFrontendHasLoaded([weakThis = WeakPtr { *this }, extensionTabID, scriptSource, completionHandler = WTFMove(completionHandler)] () mutable {
        if (!weakThis || !weakThis->m_inspectorPage) {
            completionHandler(makeUnexpected(Inspector::ExtensionError::ContextDestroyed));
            return;
        }

        weakThis->m_inspectorPage->sendWithAsyncReply(Messages::WebInspectorUIExtensionController::EvaluateScriptInExtensionTab {extensionTabID, scriptSource}, [completionHandler = WTFMove(completionHandler)](const IPC::DataReference& dataReference, const std::optional<WebCore::ExceptionDetails>& details, const std::optional<Inspector::ExtensionError>& error) mutable {
            if (error) {
                completionHandler(makeUnexpected(error.value()));
                return;
            }

            if (details) {
                Expected<RefPtr<API::SerializedScriptValue>, WebCore::ExceptionDetails> returnedValue = makeUnexpected(details.value());
                return completionHandler({ returnedValue });
            }

            completionHandler({ { API::SerializedScriptValue::adopt({ dataReference.data(), dataReference.size() }).ptr() } });
        });
    });
}

// WebInspectorUIExtensionControllerProxy IPC messages.

void WebInspectorUIExtensionControllerProxy::didShowExtensionTab(const Inspector::ExtensionID& extensionID, const Inspector::ExtensionTabID& extensionTabID, WebCore::FrameIdentifier frameID)
{
    auto it = m_extensionAPIObjectMap.find(extensionID);
    if (it == m_extensionAPIObjectMap.end())
        return;

    RefPtr<API::InspectorExtension> extension = it->value;
    auto extensionClient = extension->client();
    if (!extensionClient)
        return;

    extensionClient->didShowExtensionTab(extensionTabID, frameID);
}

void WebInspectorUIExtensionControllerProxy::didHideExtensionTab(const Inspector::ExtensionID& extensionID, const Inspector::ExtensionTabID& extensionTabID)
{
    auto it = m_extensionAPIObjectMap.find(extensionID);
    if (it == m_extensionAPIObjectMap.end())
        return;

    RefPtr<API::InspectorExtension> extension = it->value;
    auto extensionClient = extension->client();
    if (!extensionClient)
        return;

    extensionClient->didHideExtensionTab(extensionTabID);
}

void WebInspectorUIExtensionControllerProxy::didNavigateExtensionTab(const Inspector::ExtensionID& extensionID, const Inspector::ExtensionTabID& extensionTabID, const WTF::URL& newURL)
{
    auto it = m_extensionAPIObjectMap.find(extensionID);
    if (it == m_extensionAPIObjectMap.end())
        return;

    RefPtr<API::InspectorExtension> extension = it->value;
    auto extensionClient = extension->client();
    if (!extensionClient)
        return;

    extensionClient->didNavigateExtensionTab(extensionTabID, newURL);
}

void WebInspectorUIExtensionControllerProxy::inspectedPageDidNavigate(const URL& newURL)
{
    for (auto& extension : copyToVector(m_extensionAPIObjectMap.values())) {
        auto extensionClient = extension->client();
        if (!extensionClient)
            continue;

        extensionClient->inspectedPageDidNavigate(newURL);
    }
}

} // namespace WebKit

#endif // ENABLE(INSPECTOR_EXTENSIONS)
