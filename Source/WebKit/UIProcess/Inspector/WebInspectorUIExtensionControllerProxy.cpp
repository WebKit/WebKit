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

#include "WebInspectorUIExtensionControllerMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"

namespace WebKit {

WebInspectorUIExtensionControllerProxy::WebInspectorUIExtensionControllerProxy(WebPageProxy& inspectorPage)
    : m_inspectorPage(makeWeakPtr(inspectorPage))
{
}

WebInspectorUIExtensionControllerProxy::~WebInspectorUIExtensionControllerProxy()
{
    if (!m_inspectorPage)
        return;

    m_inspectorPage = nullptr;

    auto callbacks = std::exchange(m_frontendLoadedCallbackQueue, { });
    for (auto& callback : callbacks)
        callback();
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

// API

void WebInspectorUIExtensionControllerProxy::registerExtension(const InspectorExtensionID& extensionID, const String& displayName, WTF::CompletionHandler<void(Expected<bool, InspectorExtensionError>)>&& completionHandler)
{
    whenFrontendHasLoaded([weakThis = makeWeakPtr(this), extensionID, displayName, completionHandler = WTFMove(completionHandler)] () mutable {
        if (!weakThis || !weakThis->m_inspectorPage) {
            completionHandler(makeUnexpected(InspectorExtensionError::InvalidRequest));
            return;
        }

        weakThis->m_inspectorPage->sendWithAsyncReply(Messages::WebInspectorUIExtensionController::RegisterExtension { extensionID, displayName }, WTFMove(completionHandler));
    });
}

void WebInspectorUIExtensionControllerProxy::unregisterExtension(const InspectorExtensionID& extensionID, WTF::CompletionHandler<void(Expected<bool, InspectorExtensionError>)>&& completionHandler)
{
    whenFrontendHasLoaded([weakThis = makeWeakPtr(this), extensionID, completionHandler = WTFMove(completionHandler)] () mutable {
        if (!weakThis || !weakThis->m_inspectorPage) {
            completionHandler(makeUnexpected(InspectorExtensionError::InvalidRequest));
            return;
        }

        weakThis->m_inspectorPage->sendWithAsyncReply(Messages::WebInspectorUIExtensionController::UnregisterExtension { extensionID }, WTFMove(completionHandler));
    });
}

void WebInspectorUIExtensionControllerProxy::createTabForExtension(const InspectorExtensionID& extensionID, const String& tabName, const URL& tabIconURL, const URL& sourceURL, WTF::CompletionHandler<void(Expected<InspectorExtensionTabID, InspectorExtensionError>)>&& completionHandler)
{
    whenFrontendHasLoaded([weakThis = makeWeakPtr(this), extensionID, tabName, tabIconURL, sourceURL, completionHandler = WTFMove(completionHandler)] () mutable {
        if (!weakThis || !weakThis->m_inspectorPage) {
            completionHandler(makeUnexpected(InspectorExtensionError::InvalidRequest));
            return;
        }

        weakThis->m_inspectorPage->sendWithAsyncReply(Messages::WebInspectorUIExtensionController::CreateTabForExtension { extensionID, tabName, tabIconURL, sourceURL }, WTFMove(completionHandler));
    });
}

void WebInspectorUIExtensionControllerProxy::evaluateScriptForExtension(const InspectorExtensionID& extensionID, const String& scriptSource, const Optional<WTF::URL>& frameURL, const Optional<WTF::URL>& contextSecurityOrigin, const Optional<bool>& useContentScriptContext, WTF::CompletionHandler<void(InspectorExtensionEvaluationResult)>&& completionHandler)
{
    whenFrontendHasLoaded([weakThis = makeWeakPtr(this), extensionID, scriptSource, frameURL, contextSecurityOrigin, useContentScriptContext, completionHandler = WTFMove(completionHandler)] () mutable {
        if (!weakThis || !weakThis->m_inspectorPage) {
            completionHandler(makeUnexpected(InspectorExtensionError::ContextDestroyed));
            return;
        }

        weakThis->m_inspectorPage->sendWithAsyncReply(Messages::WebInspectorUIExtensionController::EvaluateScriptForExtension {extensionID, scriptSource, frameURL, contextSecurityOrigin, useContentScriptContext}, [completionHandler = WTFMove(completionHandler)](const IPC::DataReference& dataReference, const Optional<WebCore::ExceptionDetails>& details, const Optional<InspectorExtensionError> error) mutable {
            if (error) {
                completionHandler(makeUnexpected(error.value()));
                return;
            }

            if (details) {
                Expected<RefPtr<API::SerializedScriptValue>, WebCore::ExceptionDetails> returnedValue = makeUnexpected(details.value());
                return completionHandler({ returnedValue });
            }

            Vector<uint8_t> data;
            data.reserveInitialCapacity(dataReference.size());
            data.append(dataReference.data(), dataReference.size());
            completionHandler({ { API::SerializedScriptValue::adopt(WTFMove(data)).ptr() } });
        });
    });
}

void WebInspectorUIExtensionControllerProxy::reloadForExtension(const InspectorExtensionID& extensionID, const Optional<bool>& ignoreCache, const Optional<String>& userAgent, const Optional<String>& injectedScript, WTF::CompletionHandler<void(InspectorExtensionEvaluationResult)>&& completionHandler)
{
    whenFrontendHasLoaded([weakThis = makeWeakPtr(this), extensionID, ignoreCache, userAgent, injectedScript, completionHandler = WTFMove(completionHandler)] () mutable {
        if (!weakThis || !weakThis->m_inspectorPage) {
            completionHandler(makeUnexpected(InspectorExtensionError::ContextDestroyed));
            return;
        }

        weakThis->m_inspectorPage->sendWithAsyncReply(Messages::WebInspectorUIExtensionController::ReloadForExtension {extensionID, ignoreCache, userAgent, injectedScript}, [completionHandler = WTFMove(completionHandler)](const Optional<InspectorExtensionError> error) mutable {
            if (error) {
                completionHandler(makeUnexpected(error.value()));
                return;
            }

            completionHandler({ });
        });
    });
}

} // namespace WebKit

#endif // ENABLE(INSPECTOR_EXTENSIONS)
