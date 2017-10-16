/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "Session.h"

#include "CommandResult.h"
#include "SessionHost.h"
#include "WebDriverAtoms.h"
#include <inspector/InspectorValues.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/HexNumber.h>
#include <wtf/UUID.h>

using namespace Inspector;

namespace WebDriver {

// The web element identifier is a constant defined by the spec in Section 11 Elements.
// https://www.w3.org/TR/webdriver/#elements
static const String webElementIdentifier = ASCIILiteral("element-6066-11e4-a52e-4f735466cecf");

Session::Session(std::unique_ptr<SessionHost>&& host)
    : m_host(WTFMove(host))
    , m_id(createCanonicalUUIDString())
{
}

Session::~Session()
{
}

const Capabilities& Session::capabilities() const
{
    return m_host->capabilities();
}

static std::optional<String> firstWindowHandleInResult(InspectorValue& result)
{
    RefPtr<InspectorArray> handles;
    if (result.asArray(handles) && handles->length()) {
        auto handleValue = handles->get(0);
        String handle;
        if (handleValue->asString(handle))
            return handle;
    }
    return std::nullopt;
}

void Session::closeAllToplevelBrowsingContexts(const String& toplevelBrowsingContext, Function<void (CommandResult&&)>&& completionHandler)
{
    closeTopLevelBrowsingContext(toplevelBrowsingContext, [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        if (auto handle = firstWindowHandleInResult(*result.result())) {
            closeAllToplevelBrowsingContexts(handle.value(), WTFMove(completionHandler));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

void Session::close(Function<void (CommandResult&&)>&& completionHandler)
{
    m_toplevelBrowsingContext = std::nullopt;
    getWindowHandles([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        if (auto handle = firstWindowHandleInResult(*result.result())) {
            closeAllToplevelBrowsingContexts(handle.value(), WTFMove(completionHandler));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

void Session::setTimeouts(const Timeouts& timeouts, Function<void (CommandResult&&)>&& completionHandler)
{
    if (timeouts.script)
        m_timeouts.script = timeouts.script;
    if (timeouts.pageLoad)
        m_timeouts.pageLoad = timeouts.pageLoad;
    if (timeouts.implicit)
        m_timeouts.implicit = timeouts.implicit;
    completionHandler(CommandResult::success());
}

void Session::switchToTopLevelBrowsingContext(std::optional<String> toplevelBrowsingContext)
{
    m_toplevelBrowsingContext = toplevelBrowsingContext;
    m_currentBrowsingContext = std::nullopt;
}

void Session::switchToBrowsingContext(std::optional<String> browsingContext)
{
    // Automation sends empty strings for main frame.
    if (!browsingContext || browsingContext.value().isEmpty())
        m_currentBrowsingContext = std::nullopt;
    else
        m_currentBrowsingContext = browsingContext;
}

std::optional<String> Session::pageLoadStrategyString() const
{
    if (!capabilities().pageLoadStrategy)
        return std::nullopt;

    switch (capabilities().pageLoadStrategy.value()) {
    case PageLoadStrategy::None:
        return String("None");
    case PageLoadStrategy::Normal:
        return String("Normal");
    case PageLoadStrategy::Eager:
        return String("Eager");
    }

    return std::nullopt;
}

void Session::createTopLevelBrowsingContext(Function<void (CommandResult&&)>&& completionHandler)
{
    ASSERT(!m_toplevelBrowsingContext.value());
    m_host->startAutomationSession(m_id, [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](std::optional<String> errorMessage) mutable {
        if (errorMessage) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, errorMessage.value()));
            return;
        }
        m_host->sendCommandToBackend(ASCIILiteral("createBrowsingContext"), nullptr, [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            String handle;
            if (!response.responseObject->getString(ASCIILiteral("handle"), handle)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            switchToTopLevelBrowsingContext(handle);
            completionHandler(CommandResult::success());
        });
    });
}

void Session::handleUserPrompts(Function<void (CommandResult&&)>&& completionHandler)
{
    RefPtr<InspectorObject> parameters = InspectorObject::create();
    parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
    m_host->sendCommandToBackend(ASCIILiteral("isShowingJavaScriptDialog"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
        if (response.isError || !response.responseObject) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        bool isShowingJavaScriptDialog;
        if (!response.responseObject->getBoolean("result", isShowingJavaScriptDialog)) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }

        if (!isShowingJavaScriptDialog) {
            completionHandler(CommandResult::success());
            return;
        }

        if (!capabilities().unhandledPromptBehavior) {
            reportUnexpectedAlertOpen([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
                dismissAlert([this, errorResult = WTFMove(result), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
                    if (result.isError()) {
                        completionHandler(WTFMove(result));
                        return;
                    }
                    completionHandler(WTFMove(errorResult));
                });
            });
            return;
        }

        switch (capabilities().unhandledPromptBehavior.value()) {
        case UnhandledPromptBehavior::Dismiss:
            dismissAlert(WTFMove(completionHandler));
            break;
        case UnhandledPromptBehavior::Accept:
            acceptAlert(WTFMove(completionHandler));
            break;
        case UnhandledPromptBehavior::Ignore:
            reportUnexpectedAlertOpen(WTFMove(completionHandler));
            break;
        }
    });
}

void Session::reportUnexpectedAlertOpen(Function<void (CommandResult&&)>&& completionHandler)
{
    getAlertText([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) {
        std::optional<String> alertText;
        if (!result.isError()) {
            String valueString;
            if (result.result()->asString(valueString))
                alertText = valueString;
        }
        auto errorResult = CommandResult::fail(CommandResult::ErrorCode::UnexpectedAlertOpen);
        if (alertText) {
            RefPtr<InspectorObject> additonalData = InspectorObject::create();
            additonalData->setString(ASCIILiteral("text"), alertText.value());
            errorResult.setAdditionalErrorData(WTFMove(additonalData));
        }
        completionHandler(WTFMove(errorResult));
    });
}

void Session::go(const String& url, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, url, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        RefPtr<InspectorObject> parameters = InspectorObject::create();
        parameters->setString(ASCIILiteral("handle"), m_toplevelBrowsingContext.value());
        parameters->setString(ASCIILiteral("url"), url);
        if (m_timeouts.pageLoad)
            parameters->setInteger(ASCIILiteral("pageLoadTimeout"), m_timeouts.pageLoad.value().millisecondsAs<int>());
        if (auto pageLoadStrategy = pageLoadStrategyString())
            parameters->setString(ASCIILiteral("pageLoadStrategy"), pageLoadStrategy.value());
        m_host->sendCommandToBackend(ASCIILiteral("navigateBrowsingContext"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            switchToBrowsingContext(std::nullopt);
            completionHandler(CommandResult::success());
        });
    });
}

void Session::getCurrentURL(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        RefPtr<InspectorObject> parameters = InspectorObject::create();
        parameters->setString(ASCIILiteral("handle"), m_toplevelBrowsingContext.value());
        m_host->sendCommandToBackend(ASCIILiteral("getBrowsingContext"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            RefPtr<InspectorObject> browsingContext;
            if (!response.responseObject->getObject("context", browsingContext)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            String url;
            if (!browsingContext->getString("url", url)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            completionHandler(CommandResult::success(InspectorValue::create(url)));
        });
    });
}

void Session::back(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        RefPtr<InspectorObject> parameters = InspectorObject::create();
        parameters->setString(ASCIILiteral("handle"), m_toplevelBrowsingContext.value());
        if (m_timeouts.pageLoad)
            parameters->setInteger(ASCIILiteral("pageLoadTimeout"), m_timeouts.pageLoad.value().millisecondsAs<int>());
        if (auto pageLoadStrategy = pageLoadStrategyString())
            parameters->setString(ASCIILiteral("pageLoadStrategy"), pageLoadStrategy.value());
        m_host->sendCommandToBackend(ASCIILiteral("goBackInBrowsingContext"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            switchToBrowsingContext(std::nullopt);
            completionHandler(CommandResult::success());
        });
    });
}

void Session::forward(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        RefPtr<InspectorObject> parameters = InspectorObject::create();
        parameters->setString(ASCIILiteral("handle"), m_toplevelBrowsingContext.value());
        if (m_timeouts.pageLoad)
            parameters->setInteger(ASCIILiteral("pageLoadTimeout"), m_timeouts.pageLoad.value().millisecondsAs<int>());
        if (auto pageLoadStrategy = pageLoadStrategyString())
            parameters->setString(ASCIILiteral("pageLoadStrategy"), pageLoadStrategy.value());
        m_host->sendCommandToBackend(ASCIILiteral("goForwardInBrowsingContext"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            switchToBrowsingContext(std::nullopt);
            completionHandler(CommandResult::success());
        });
    });
}

void Session::refresh(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        RefPtr<InspectorObject> parameters = InspectorObject::create();
        parameters->setString(ASCIILiteral("handle"), m_toplevelBrowsingContext.value());
        if (m_timeouts.pageLoad)
            parameters->setInteger(ASCIILiteral("pageLoadTimeout"), m_timeouts.pageLoad.value().millisecondsAs<int>());
        if (auto pageLoadStrategy = pageLoadStrategyString())
            parameters->setString(ASCIILiteral("pageLoadStrategy"), pageLoadStrategy.value());
        m_host->sendCommandToBackend(ASCIILiteral("reloadBrowsingContext"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            switchToBrowsingContext(std::nullopt);
            completionHandler(CommandResult::success());
        });
    });
}

void Session::getTitle(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        RefPtr<InspectorObject> parameters = InspectorObject::create();
        parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
        parameters->setString(ASCIILiteral("function"), ASCIILiteral("function() { return document.title; }"));
        parameters->setArray(ASCIILiteral("arguments"), InspectorArray::create());
        m_host->sendCommandToBackend(ASCIILiteral("evaluateJavaScriptFunction"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            String valueString;
            if (!response.responseObject->getString(ASCIILiteral("result"), valueString)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            RefPtr<InspectorValue> resultValue;
            if (!InspectorValue::parseJSON(valueString, resultValue)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

void Session::getWindowHandle(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    RefPtr<InspectorObject> parameters = InspectorObject::create();
    parameters->setString(ASCIILiteral("handle"), m_toplevelBrowsingContext.value());
    m_host->sendCommandToBackend(ASCIILiteral("getBrowsingContext"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError || !response.responseObject) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        RefPtr<InspectorObject> browsingContext;
        if (!response.responseObject->getObject(ASCIILiteral("context"), browsingContext)) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }
        String handle;
        if (!browsingContext->getString(ASCIILiteral("handle"), handle)) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }
        completionHandler(CommandResult::success(InspectorValue::create(handle)));
    });
}

void Session::closeTopLevelBrowsingContext(const String& toplevelBrowsingContext, Function<void (CommandResult&&)>&& completionHandler)
{
    RefPtr<InspectorObject> parameters = InspectorObject::create();
    parameters->setString(ASCIILiteral("handle"), toplevelBrowsingContext);
    m_host->sendCommandToBackend(ASCIILiteral("closeBrowsingContext"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
        if (!m_host->isConnected()) {
            // Closing the browsing context made the browser quit.
            completionHandler(CommandResult::success(InspectorArray::create()));
            return;
        }
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }

        getWindowHandles([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) {
            if (!m_host->isConnected()) {
                // Closing the browsing context made the browser quit.
                completionHandler(CommandResult::success(InspectorArray::create()));
                return;
            }
            completionHandler(WTFMove(result));
        });
    });
}

void Session::closeWindow(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto toplevelBrowsingContext = std::exchange(m_toplevelBrowsingContext, std::nullopt);
        closeTopLevelBrowsingContext(toplevelBrowsingContext.value(), WTFMove(completionHandler));
    });
}

void Session::switchToWindow(const String& windowHandle, Function<void (CommandResult&&)>&& completionHandler)
{
    RefPtr<InspectorObject> parameters = InspectorObject::create();
    parameters->setString(ASCIILiteral("browsingContextHandle"), windowHandle);
    m_host->sendCommandToBackend(ASCIILiteral("switchToBrowsingContext"), WTFMove(parameters), [this, protectedThis = makeRef(*this), windowHandle, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        switchToTopLevelBrowsingContext(windowHandle);
        completionHandler(CommandResult::success());
    });
}

void Session::getWindowHandles(Function<void (CommandResult&&)>&& completionHandler)
{
    m_host->sendCommandToBackend(ASCIILiteral("getBrowsingContexts"), InspectorObject::create(), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError || !response.responseObject) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        RefPtr<InspectorArray> browsingContextArray;
        if (!response.responseObject->getArray(ASCIILiteral("contexts"), browsingContextArray)) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }
        RefPtr<InspectorArray> windowHandles = InspectorArray::create();
        for (unsigned i = 0; i < browsingContextArray->length(); ++i) {
            RefPtr<InspectorValue> browsingContextValue = browsingContextArray->get(i);
            RefPtr<InspectorObject> browsingContext;
            if (!browsingContextValue->asObject(browsingContext)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            String handle;
            if (!browsingContext->getString(ASCIILiteral("handle"), handle)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            windowHandles->pushString(handle);
        }
        completionHandler(CommandResult::success(WTFMove(windowHandles)));
    });
}

void Session::switchToFrame(RefPtr<InspectorValue>&& frameID, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    if (frameID->isNull()) {
        switchToBrowsingContext(std::nullopt);
        completionHandler(CommandResult::success());
        return;
    }

    handleUserPrompts([this, frameID = WTFMove(frameID), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        RefPtr<InspectorObject> parameters = InspectorObject::create();
        parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString(ASCIILiteral("frameHandle"), m_currentBrowsingContext.value());

        int frameIndex;
        if (frameID->asInteger(frameIndex)) {
            if (frameIndex < 0 || frameIndex > USHRT_MAX) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchFrame));
                return;
            }
            parameters->setInteger(ASCIILiteral("ordinal"), frameIndex);
        } else {
            String frameElementID = extractElementID(*frameID);
            if (!frameElementID.isEmpty())
                parameters->setString(ASCIILiteral("nodeHandle"), frameElementID);
            else {
                String frameName;
                if (!frameID->asString(frameName)) {
                    completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchFrame));
                    return;
                }
                parameters->setString(ASCIILiteral("name"), frameName);
            }
        }

        m_host->sendCommandToBackend(ASCIILiteral("resolveChildFrameHandle"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            String frameHandle;
            if (!response.responseObject->getString(ASCIILiteral("result"), frameHandle)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            switchToBrowsingContext(frameHandle);
            completionHandler(CommandResult::success());
        });
    });
}

void Session::switchToParentFrame(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::success());
        return;
    }

    handleUserPrompts([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        RefPtr<InspectorObject> parameters = InspectorObject::create();
        parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
        parameters->setString(ASCIILiteral("frameHandle"), m_currentBrowsingContext.value());
        m_host->sendCommandToBackend(ASCIILiteral("resolveParentFrameHandle"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            String frameHandle;
            if (!response.responseObject->getString(ASCIILiteral("result"), frameHandle)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            switchToBrowsingContext(frameHandle);
            completionHandler(CommandResult::success());
        });
    });
}

void Session::getToplevelBrowsingContextRect(Function<void (CommandResult&&)>&& completionHandler)
{
    RefPtr<InspectorObject> parameters = InspectorObject::create();
    parameters->setString(ASCIILiteral("handle"), m_toplevelBrowsingContext.value());
    m_host->sendCommandToBackend(ASCIILiteral("getBrowsingContext"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError || !response.responseObject) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        RefPtr<InspectorObject> browsingContext;
        if (!response.responseObject->getObject(ASCIILiteral("context"), browsingContext)) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }
        RefPtr<InspectorObject> windowOrigin;
        double x, y;
        if (!browsingContext->getObject(ASCIILiteral("windowOrigin"), windowOrigin)
            || !windowOrigin->getDouble(ASCIILiteral("x"), x)
            || !windowOrigin->getDouble(ASCIILiteral("y"), y)) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }
        RefPtr<InspectorObject> windowSize;
        double width, height;
        if (!browsingContext->getObject(ASCIILiteral("windowSize"), windowSize)
            || !windowSize->getDouble(ASCIILiteral("width"), width)
            || !windowSize->getDouble(ASCIILiteral("height"), width)) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }
        auto windowRect = InspectorObject::create();
        windowRect->setDouble(ASCIILiteral("x"), x);
        windowRect->setDouble(ASCIILiteral("y"), y);
        windowRect->setDouble(ASCIILiteral("width"), width);
        windowRect->setDouble(ASCIILiteral("height"), height);
        completionHandler(CommandResult::success(WTFMove(windowRect)));
    });
}

void Session::moveToplevelBrowsingContextWindow(double x, double y, Function<void (CommandResult&&)>&& completionHandler)
{
    RefPtr<InspectorObject> parameters = InspectorObject::create();
    parameters->setString(ASCIILiteral("handle"), m_toplevelBrowsingContext.value());
    RefPtr<InspectorObject> windowOrigin = InspectorObject::create();
    windowOrigin->setDouble("x", x);
    windowOrigin->setDouble("y", y);
    parameters->setObject(ASCIILiteral("origin"), WTFMove(windowOrigin));
    m_host->sendCommandToBackend(ASCIILiteral("moveWindowOfBrowsingContext"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

void Session::resizeToplevelBrowsingContextWindow(double width, double height, Function<void (CommandResult&&)>&& completionHandler)
{
    RefPtr<InspectorObject> parameters = InspectorObject::create();
    parameters->setString(ASCIILiteral("handle"), m_toplevelBrowsingContext.value());
    RefPtr<InspectorObject> windowSize = InspectorObject::create();
    windowSize->setDouble("width", width);
    windowSize->setDouble("height", height);
    parameters->setObject(ASCIILiteral("size"), WTFMove(windowSize));
    m_host->sendCommandToBackend(ASCIILiteral("resizeWindowOfBrowsingContext"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

void Session::getWindowRect(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        getToplevelBrowsingContextRect(WTFMove(completionHandler));
    });
}

void Session::setWindowRect(std::optional<double> x, std::optional<double> y, std::optional<double> width, std::optional<double> height, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, x, y, width, height, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        if (width && height)  {
            resizeToplevelBrowsingContextWindow(width.value(), height.value(), [this, x, y, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
                if (result.isError()) {
                    completionHandler(WTFMove(result));
                    return;
                }
                if (!x || !y) {
                    getToplevelBrowsingContextRect(WTFMove(completionHandler));
                    return;
                }

                moveToplevelBrowsingContextWindow(x.value(), y.value(), [this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
                    if (result.isError()) {
                        completionHandler(WTFMove(result));
                        return;
                    }
                    getToplevelBrowsingContextRect(WTFMove(completionHandler));
                });
            });
            return;
        }

        if (x && y) {
            moveToplevelBrowsingContextWindow(x.value(), y.value(), [this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
                if (result.isError()) {
                    completionHandler(WTFMove(result));
                    return;
                }
                getToplevelBrowsingContextRect(WTFMove(completionHandler));
            });
            return;
        }

        getToplevelBrowsingContextRect(WTFMove(completionHandler));
    });
}

RefPtr<InspectorObject> Session::createElement(RefPtr<InspectorValue>&& value)
{
    RefPtr<InspectorObject> valueObject;
    if (!value->asObject(valueObject))
        return nullptr;

    String elementID;
    if (!valueObject->getString("session-node-" + m_id, elementID))
        return nullptr;

    RefPtr<InspectorObject> elementObject = InspectorObject::create();
    elementObject->setString(webElementIdentifier, elementID);
    return elementObject;
}

RefPtr<InspectorObject> Session::createElement(const String& elementID)
{
    RefPtr<InspectorObject> elementObject = InspectorObject::create();
    elementObject->setString("session-node-" + m_id, elementID);
    return elementObject;
}

RefPtr<InspectorObject> Session::extractElement(InspectorValue& value)
{
    String elementID = extractElementID(value);
    return !elementID.isEmpty() ? createElement(elementID) : nullptr;
}

String Session::extractElementID(InspectorValue& value)
{
    RefPtr<InspectorObject> valueObject;
    if (!value.asObject(valueObject))
        return emptyString();

    String elementID;
    if (!valueObject->getString(webElementIdentifier, elementID))
        return emptyString();

    return elementID;
}

void Session::computeElementLayout(const String& elementID, OptionSet<ElementLayoutOption> options, Function<void (std::optional<Rect>&&, std::optional<Point>&&, bool, RefPtr<InspectorObject>&&)>&& completionHandler)
{
    ASSERT(m_toplevelBrowsingContext.value());

    RefPtr<InspectorObject> parameters = InspectorObject::create();
    parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
    parameters->setString(ASCIILiteral("frameHandle"), m_currentBrowsingContext.value());
    parameters->setString(ASCIILiteral("nodeHandle"), elementID);
    parameters->setBoolean(ASCIILiteral("scrollIntoViewIfNeeded"), options.contains(ElementLayoutOption::ScrollIntoViewIfNeeded));
    parameters->setBoolean(ASCIILiteral("useViewportCoordinates"), options.contains(ElementLayoutOption::UseViewportCoordinates));
    m_host->sendCommandToBackend(ASCIILiteral("computeElementLayout"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
        if (response.isError || !response.responseObject) {
            completionHandler(std::nullopt, std::nullopt, false, WTFMove(response.responseObject));
            return;
        }
        RefPtr<InspectorObject> rectObject;
        if (!response.responseObject->getObject(ASCIILiteral("rect"), rectObject)) {
            completionHandler(std::nullopt, std::nullopt, false, nullptr);
            return;
        }
        std::optional<int> elementX;
        std::optional<int> elementY;
        RefPtr<InspectorObject> elementPosition;
        if (rectObject->getObject(ASCIILiteral("origin"), elementPosition)) {
            int x, y;
            if (elementPosition->getInteger(ASCIILiteral("x"), x) && elementPosition->getInteger(ASCIILiteral("y"), y)) {
                elementX = x;
                elementY = y;
            }
        }
        if (!elementX || !elementY) {
            completionHandler(std::nullopt, std::nullopt, false, nullptr);
            return;
        }
        std::optional<int> elementWidth;
        std::optional<int> elementHeight;
        RefPtr<InspectorObject> elementSize;
        if (rectObject->getObject(ASCIILiteral("size"), elementSize)) {
            int width, height;
            if (elementSize->getInteger(ASCIILiteral("width"), width) && elementSize->getInteger(ASCIILiteral("height"), height)) {
                elementWidth = width;
                elementHeight = height;
            }
        }
        if (!elementWidth || !elementHeight) {
            completionHandler(std::nullopt, std::nullopt, false, nullptr);
            return;
        }
        Rect rect = { { elementX.value(), elementY.value() }, { elementWidth.value(), elementHeight.value() } };

        bool isObscured;
        if (!response.responseObject->getBoolean(ASCIILiteral("isObscured"), isObscured)) {
            completionHandler(std::nullopt, std::nullopt, false, nullptr);
            return;
        }
        RefPtr<InspectorObject> inViewCenterPointObject;
        if (!response.responseObject->getObject(ASCIILiteral("inViewCenterPoint"), inViewCenterPointObject)) {
            completionHandler(rect, std::nullopt, isObscured, nullptr);
            return;
        }
        int inViewCenterPointX, inViewCenterPointY;
        if (!inViewCenterPointObject->getInteger(ASCIILiteral("x"), inViewCenterPointX)
            || !inViewCenterPointObject->getInteger(ASCIILiteral("y"), inViewCenterPointY)) {
            completionHandler(std::nullopt, std::nullopt, isObscured, nullptr);
            return;
        }
        Point inViewCenterPoint = { inViewCenterPointX, inViewCenterPointY };
        completionHandler(rect, inViewCenterPoint, isObscured, nullptr);
    });
}

void Session::findElements(const String& strategy, const String& selector, FindElementsMode mode, const String& rootElementID, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    auto implicitWait = m_timeouts.implicit.value_or(0_s);
    RefPtr<InspectorArray> arguments = InspectorArray::create();
    arguments->pushString(InspectorValue::create(strategy)->toJSONString());
    if (rootElementID.isEmpty())
        arguments->pushString(InspectorValue::null()->toJSONString());
    else
        arguments->pushString(createElement(rootElementID)->toJSONString());
    arguments->pushString(InspectorValue::create(selector)->toJSONString());
    arguments->pushString(InspectorValue::create(mode == FindElementsMode::Single)->toJSONString());
    arguments->pushString(InspectorValue::create(implicitWait.milliseconds())->toJSONString());

    RefPtr<InspectorObject> parameters = InspectorObject::create();
    parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
    if (m_currentBrowsingContext)
        parameters->setString(ASCIILiteral("frameHandle"), m_currentBrowsingContext.value());
    parameters->setString(ASCIILiteral("function"), FindNodesJavaScript);
    parameters->setArray(ASCIILiteral("arguments"), WTFMove(arguments));
    parameters->setBoolean(ASCIILiteral("expectsImplicitCallbackArgument"), true);
    // If there's an implicit wait, use one second more as callback timeout.
    if (implicitWait)
        parameters->setInteger(ASCIILiteral("callbackTimeout"), Seconds(implicitWait + 1_s).millisecondsAs<int>());

    m_host->sendCommandToBackend(ASCIILiteral("evaluateJavaScriptFunction"), WTFMove(parameters), [this, protectedThis = makeRef(*this), mode, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError || !response.responseObject) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        String valueString;
        if (!response.responseObject->getString(ASCIILiteral("result"), valueString)) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }
        RefPtr<InspectorValue> resultValue;
        if (!InspectorValue::parseJSON(valueString, resultValue)) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }

        switch (mode) {
        case FindElementsMode::Single: {
            RefPtr<InspectorObject> elementObject = createElement(WTFMove(resultValue));
            if (!elementObject) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchElement));
                return;
            }
            completionHandler(CommandResult::success(WTFMove(elementObject)));
            break;
        }
        case FindElementsMode::Multiple: {
            RefPtr<InspectorArray> elementsArray;
            if (!resultValue->asArray(elementsArray)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchElement));
                return;
            }
            RefPtr<InspectorArray> elementObjectsArray = InspectorArray::create();
            unsigned elementsArrayLength = elementsArray->length();
            for (unsigned i = 0; i < elementsArrayLength; ++i) {
                if (auto elementObject = createElement(elementsArray->get(i)))
                    elementObjectsArray->pushObject(WTFMove(elementObject));
            }
            completionHandler(CommandResult::success(WTFMove(elementObjectsArray)));
            break;
        }
        }
    });
}

void Session::isElementSelected(const String& elementID, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        RefPtr<InspectorArray> arguments = InspectorArray::create();
        arguments->pushString(createElement(elementID)->toJSONString());
        arguments->pushString(InspectorValue::create("selected")->toJSONString());

        RefPtr<InspectorObject> parameters = InspectorObject::create();
        parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString(ASCIILiteral("frameHandle"), m_currentBrowsingContext.value());
        parameters->setString(ASCIILiteral("function"), ElementAttributeJavaScript);
        parameters->setArray(ASCIILiteral("arguments"), WTFMove(arguments));
        m_host->sendCommandToBackend(ASCIILiteral("evaluateJavaScriptFunction"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            String valueString;
            if (!response.responseObject->getString(ASCIILiteral("result"), valueString)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            RefPtr<InspectorValue> resultValue;
            if (!InspectorValue::parseJSON(valueString, resultValue)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            if (resultValue->isNull()) {
                completionHandler(CommandResult::success(InspectorValue::create(false)));
                return;
            }
            String booleanResult;
            if (!resultValue->asString(booleanResult) || booleanResult != "true") {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            completionHandler(CommandResult::success(InspectorValue::create(true)));
        });
    });
}

void Session::getElementText(const String& elementID, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        RefPtr<InspectorArray> arguments = InspectorArray::create();
        arguments->pushString(createElement(elementID)->toJSONString());

        RefPtr<InspectorObject> parameters = InspectorObject::create();
        parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString(ASCIILiteral("frameHandle"), m_currentBrowsingContext.value());
        // FIXME: Add an atom to properly implement this instead of just using innerText.
        parameters->setString(ASCIILiteral("function"), ASCIILiteral("function(element) { return element.innerText.replace(/^[^\\S\\xa0]+|[^\\S\\xa0]+$/g, '') }"));
        parameters->setArray(ASCIILiteral("arguments"), WTFMove(arguments));
        m_host->sendCommandToBackend(ASCIILiteral("evaluateJavaScriptFunction"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            String valueString;
            if (!response.responseObject->getString(ASCIILiteral("result"), valueString)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            RefPtr<InspectorValue> resultValue;
            if (!InspectorValue::parseJSON(valueString, resultValue)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

void Session::getElementTagName(const String& elementID, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        RefPtr<InspectorArray> arguments = InspectorArray::create();
        arguments->pushString(createElement(elementID)->toJSONString());

        RefPtr<InspectorObject> parameters = InspectorObject::create();
        parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString(ASCIILiteral("frameHandle"), m_currentBrowsingContext.value());
        parameters->setString(ASCIILiteral("function"), ASCIILiteral("function(element) { return element.tagName.toLowerCase() }"));
        parameters->setArray(ASCIILiteral("arguments"), WTFMove(arguments));
        m_host->sendCommandToBackend(ASCIILiteral("evaluateJavaScriptFunction"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            String valueString;
            if (!response.responseObject->getString(ASCIILiteral("result"), valueString)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            RefPtr<InspectorValue> resultValue;
            if (!InspectorValue::parseJSON(valueString, resultValue)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

void Session::getElementRect(const String& elementID, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        computeElementLayout(elementID, { }, [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](std::optional<Rect>&& rect, std::optional<Point>&&, bool, RefPtr<InspectorObject>&& error) {
            if (!rect || error) {
                completionHandler(CommandResult::fail(WTFMove(error)));
                return;
            }
            RefPtr<InspectorObject> rectObject = InspectorObject::create();
            rectObject->setInteger(ASCIILiteral("x"), rect.value().origin.x);
            rectObject->setInteger(ASCIILiteral("y"), rect.value().origin.y);
            rectObject->setInteger(ASCIILiteral("width"), rect.value().size.width);
            rectObject->setInteger(ASCIILiteral("height"), rect.value().size.height);
            completionHandler(CommandResult::success(WTFMove(rectObject)));
        });
    });
}

void Session::isElementEnabled(const String& elementID, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        RefPtr<InspectorArray> arguments = InspectorArray::create();
        arguments->pushString(createElement(elementID)->toJSONString());

        RefPtr<InspectorObject> parameters = InspectorObject::create();
        parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString(ASCIILiteral("frameHandle"), m_currentBrowsingContext.value());
        parameters->setString(ASCIILiteral("function"), ASCIILiteral("function(element) { return element.disabled === undefined ? true : !element.disabled }"));
        parameters->setArray(ASCIILiteral("arguments"), WTFMove(arguments));
        m_host->sendCommandToBackend(ASCIILiteral("evaluateJavaScriptFunction"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            String valueString;
            if (!response.responseObject->getString(ASCIILiteral("result"), valueString)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            RefPtr<InspectorValue> resultValue;
            if (!InspectorValue::parseJSON(valueString, resultValue)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

void Session::isElementDisplayed(const String& elementID, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        RefPtr<InspectorArray> arguments = InspectorArray::create();
        arguments->pushString(createElement(elementID)->toJSONString());

        RefPtr<InspectorObject> parameters = InspectorObject::create();
        parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString(ASCIILiteral("frameHandle"), m_currentBrowsingContext.value());
        parameters->setString(ASCIILiteral("function"), ElementDisplayedJavaScript);
        parameters->setArray(ASCIILiteral("arguments"), WTFMove(arguments));
        m_host->sendCommandToBackend(ASCIILiteral("evaluateJavaScriptFunction"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            String valueString;
            if (!response.responseObject->getString(ASCIILiteral("result"), valueString)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            RefPtr<InspectorValue> resultValue;
            if (!InspectorValue::parseJSON(valueString, resultValue)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

void Session::getElementAttribute(const String& elementID, const String& attribute, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, elementID, attribute, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        RefPtr<InspectorArray> arguments = InspectorArray::create();
        arguments->pushString(createElement(elementID)->toJSONString());
        arguments->pushString(InspectorValue::create(attribute)->toJSONString());

        RefPtr<InspectorObject> parameters = InspectorObject::create();
        parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString(ASCIILiteral("frameHandle"), m_currentBrowsingContext.value());
        parameters->setString(ASCIILiteral("function"), ElementAttributeJavaScript);
        parameters->setArray(ASCIILiteral("arguments"), WTFMove(arguments));
        m_host->sendCommandToBackend(ASCIILiteral("evaluateJavaScriptFunction"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            String valueString;
            if (!response.responseObject->getString(ASCIILiteral("result"), valueString)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            RefPtr<InspectorValue> resultValue;
            if (!InspectorValue::parseJSON(valueString, resultValue)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

void Session::waitForNavigationToComplete(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    RefPtr<InspectorObject> parameters = InspectorObject::create();
    parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
    if (m_currentBrowsingContext)
        parameters->setString(ASCIILiteral("frameHandle"), m_currentBrowsingContext.value());
    if (m_timeouts.pageLoad)
        parameters->setInteger(ASCIILiteral("pageLoadTimeout"), m_timeouts.pageLoad.value().millisecondsAs<int>());
    if (auto pageLoadStrategy = pageLoadStrategyString())
        parameters->setString(ASCIILiteral("pageLoadStrategy"), pageLoadStrategy.value());
    m_host->sendCommandToBackend(ASCIILiteral("waitForNavigationToComplete"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            auto result = CommandResult::fail(WTFMove(response.responseObject));
            if (result.errorCode() == CommandResult::ErrorCode::NoSuchFrame) {
                // Navigation destroyed the current frame, switch to top level browsing context and ignore the error.
                switchToBrowsingContext(std::nullopt);
            } else {
                completionHandler(WTFMove(result));
                return;
            }
        }
        completionHandler(CommandResult::success());
    });
}

void Session::selectOptionElement(const String& elementID, Function<void (CommandResult&&)>&& completionHandler)
{
    RefPtr<InspectorObject> parameters = InspectorObject::create();
    parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
    parameters->setString(ASCIILiteral("frameHandle"), m_currentBrowsingContext.value());
    parameters->setString(ASCIILiteral("nodeHandle"), elementID);
    m_host->sendCommandToBackend(ASCIILiteral("selectOptionElement"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

void Session::elementClick(const String& elementID, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    OptionSet<ElementLayoutOption> options = ElementLayoutOption::ScrollIntoViewIfNeeded;
    options |= ElementLayoutOption::UseViewportCoordinates;
    computeElementLayout(elementID, options, [this, protectedThis = makeRef(*this), elementID, completionHandler = WTFMove(completionHandler)](std::optional<Rect>&& rect, std::optional<Point>&& inViewCenter, bool isObscured, RefPtr<InspectorObject>&& error) mutable {
        if (!rect || error) {
            completionHandler(CommandResult::fail(WTFMove(error)));
            return;
        }
        if (isObscured) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::ElementClickIntercepted));
            return;
        }
        if (!inViewCenter) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::ElementNotInteractable));
            return;
        }

        getElementTagName(elementID, [this, elementID, inViewCenter = WTFMove(inViewCenter), isObscured, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
            bool isOptionElement = false;
            if (!result.isError()) {
                String tagName;
                if (result.result()->asString(tagName))
                    isOptionElement = tagName == "option";
            }

            Function<void (CommandResult&&)> continueAfterClickFunction = [this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
                if (result.isError()) {
                    completionHandler(WTFMove(result));
                    return;
                }

                waitForNavigationToComplete(WTFMove(completionHandler));
            };
            if (isOptionElement)
                selectOptionElement(elementID, WTFMove(continueAfterClickFunction));
            else
                performMouseInteraction(inViewCenter.value().x, inViewCenter.value().y, MouseButton::Left, MouseInteraction::SingleClick, WTFMove(continueAfterClickFunction));
        });
    });
}

void Session::elementClear(const String& elementID, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    RefPtr<InspectorArray> arguments = InspectorArray::create();
    arguments->pushString(createElement(elementID)->toJSONString());

    RefPtr<InspectorObject> parameters = InspectorObject::create();
    parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
    if (m_currentBrowsingContext)
        parameters->setString(ASCIILiteral("frameHandle"), m_currentBrowsingContext.value());
    parameters->setString(ASCIILiteral("function"), FormElementClearJavaScript);
    parameters->setArray(ASCIILiteral("arguments"), WTFMove(arguments));
    m_host->sendCommandToBackend(ASCIILiteral("evaluateJavaScriptFunction"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

String Session::virtualKeyForKeySequence(const String& keySequence, KeyModifier& modifier)
{
    // §17.4.2 Keyboard Actions.
    // https://www.w3.org/TR/webdriver/#keyboard-actions
    modifier = KeyModifier::None;
    switch (keySequence[0]) {
    case 0xE001U:
        return ASCIILiteral("Cancel");
    case 0xE002U:
        return ASCIILiteral("Help");
    case 0xE003U:
        return ASCIILiteral("Backspace");
    case 0xE004U:
        return ASCIILiteral("Tab");
    case 0xE005U:
        return ASCIILiteral("Clear");
    case 0xE006U:
        return ASCIILiteral("Return");
    case 0xE007U:
        return ASCIILiteral("Enter");
    case 0xE008U:
        modifier = KeyModifier::Shift;
        return ASCIILiteral("Shift");
    case 0xE009U:
        modifier = KeyModifier::Control;
        return ASCIILiteral("Control");
    case 0xE00AU:
        modifier = KeyModifier::Alternate;
        return ASCIILiteral("Alternate");
    case 0xE00BU:
        return ASCIILiteral("Pause");
    case 0xE00CU:
        return ASCIILiteral("Escape");
    case 0xE00DU:
        return ASCIILiteral("Space");
    case 0xE00EU:
        return ASCIILiteral("PageUp");
    case 0xE00FU:
        return ASCIILiteral("PageDown");
    case 0xE010U:
        return ASCIILiteral("End");
    case 0xE011U:
        return ASCIILiteral("Home");
    case 0xE012U:
        return ASCIILiteral("LeftArrow");
    case 0xE013U:
        return ASCIILiteral("UpArrow");
    case 0xE014U:
        return ASCIILiteral("RightArrow");
    case 0xE015U:
        return ASCIILiteral("DownArrow");
    case 0xE016U:
        return ASCIILiteral("Insert");
    case 0xE017U:
        return ASCIILiteral("Delete");
    case 0xE018U:
        return ASCIILiteral("Semicolon");
    case 0xE019U:
        return ASCIILiteral("Equals");
    case 0xE01AU:
        return ASCIILiteral("NumberPad0");
    case 0xE01BU:
        return ASCIILiteral("NumberPad1");
    case 0xE01CU:
        return ASCIILiteral("NumberPad2");
    case 0xE01DU:
        return ASCIILiteral("NumberPad3");
    case 0xE01EU:
        return ASCIILiteral("NumberPad4");
    case 0xE01FU:
        return ASCIILiteral("NumberPad5");
    case 0xE020U:
        return ASCIILiteral("NumberPad6");
    case 0xE021U:
        return ASCIILiteral("NumberPad7");
    case 0xE022U:
        return ASCIILiteral("NumberPad8");
    case 0xE023U:
        return ASCIILiteral("NumberPad9");
    case 0xE024U:
        return ASCIILiteral("NumberPadMultiply");
    case 0xE025U:
        return ASCIILiteral("NumberPadAdd");
    case 0xE026U:
        return ASCIILiteral("NumberPadSeparator");
    case 0xE027U:
        return ASCIILiteral("NumberPadSubtract");
    case 0xE028U:
        return ASCIILiteral("NumberPadDecimal");
    case 0xE029U:
        return ASCIILiteral("NumberPadDivide");
    case 0xE031U:
        return ASCIILiteral("Function1");
    case 0xE032U:
        return ASCIILiteral("Function2");
    case 0xE033U:
        return ASCIILiteral("Function3");
    case 0xE034U:
        return ASCIILiteral("Function4");
    case 0xE035U:
        return ASCIILiteral("Function5");
    case 0xE036U:
        return ASCIILiteral("Function6");
    case 0xE037U:
        return ASCIILiteral("Function7");
    case 0xE038U:
        return ASCIILiteral("Function8");
    case 0xE039U:
        return ASCIILiteral("Function9");
    case 0xE03AU:
        return ASCIILiteral("Function10");
    case 0xE03BU:
        return ASCIILiteral("Function11");
    case 0xE03CU:
        return ASCIILiteral("Function12");
    case 0xE03DU:
        modifier = KeyModifier::Meta;
        return ASCIILiteral("Meta");
    default:
        break;
    }
    return String();
}

void Session::elementSendKeys(const String& elementID, Vector<String>&& keys, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, elementID, keys = WTFMove(keys), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        // FIXME: move this to an atom.
        static const char focusScript[] =
            "function focus(element) {"
            "    var doc = element.ownerDocument || element;"
            "    var prevActiveElement = doc.activeElement;"
            "    if (element != prevActiveElement && prevActiveElement)"
            "        prevActiveElement.blur();"
            "    element.focus();"
            "    if (element != prevActiveElement && element.value && element.value.length && element.setSelectionRange)"
            "        element.setSelectionRange(element.value.length, element.value.length);"
            "    if (element != doc.activeElement)"
            "        throw new Error('cannot focus element');"
            "}";

        RefPtr<InspectorArray> arguments = InspectorArray::create();
        arguments->pushString(createElement(elementID)->toJSONString());
        RefPtr<InspectorObject> parameters = InspectorObject::create();
        parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString(ASCIILiteral("frameHandle"), m_currentBrowsingContext.value());
        parameters->setString(ASCIILiteral("function"), focusScript);
        parameters->setArray(ASCIILiteral("arguments"), WTFMove(arguments));
        m_host->sendCommandToBackend(ASCIILiteral("evaluateJavaScriptFunction"), WTFMove(parameters), [this, protectedThis = makeRef(*this), keys = WTFMove(keys), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            unsigned stickyModifiers = 0;
            Vector<KeyboardInteraction> interactions;
            interactions.reserveInitialCapacity(keys.size());
            for (const auto& key : keys) {
                KeyboardInteraction interaction;
                KeyModifier modifier;
                auto virtualKey = virtualKeyForKeySequence(key, modifier);
                if (!virtualKey.isNull()) {
                    interaction.key = virtualKey;
                    if (modifier != KeyModifier::None) {
                        stickyModifiers ^= modifier;
                        if (stickyModifiers & modifier)
                            interaction.type = KeyboardInteractionType::KeyPress;
                        else
                            interaction.type = KeyboardInteractionType::KeyRelease;
                    }
                } else
                    interaction.text = key;
                interactions.uncheckedAppend(WTFMove(interaction));
            }

            // Reset sticky modifiers if needed.
            if (stickyModifiers) {
                if (stickyModifiers & KeyModifier::Shift)
                    interactions.append({ KeyboardInteractionType::KeyRelease, std::nullopt, std::optional<String>(ASCIILiteral("Shift")) });
                if (stickyModifiers & KeyModifier::Control)
                    interactions.append({ KeyboardInteractionType::KeyRelease, std::nullopt, std::optional<String>(ASCIILiteral("Control")) });
                if (stickyModifiers & KeyModifier::Alternate)
                    interactions.append({ KeyboardInteractionType::KeyRelease, std::nullopt, std::optional<String>(ASCIILiteral("Alternate")) });
                if (stickyModifiers & KeyModifier::Meta)
                    interactions.append({ KeyboardInteractionType::KeyRelease, std::nullopt, std::optional<String>(ASCIILiteral("Meta")) });
            }

            performKeyboardInteractions(WTFMove(interactions), WTFMove(completionHandler));
        });
    });
}

void Session::elementSubmit(const String& elementID, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        RefPtr<InspectorArray> arguments = InspectorArray::create();
        arguments->pushString(createElement(elementID)->toJSONString());

        RefPtr<InspectorObject> parameters = InspectorObject::create();
        parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString(ASCIILiteral("frameHandle"), m_currentBrowsingContext.value());
        parameters->setString(ASCIILiteral("function"), FormSubmitJavaScript);
        parameters->setArray(ASCIILiteral("arguments"), WTFMove(arguments));
        m_host->sendCommandToBackend(ASCIILiteral("evaluateJavaScriptFunction"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            completionHandler(CommandResult::success());
        });
    });
}

RefPtr<InspectorValue> Session::handleScriptResult(RefPtr<InspectorValue>&& resultValue)
{
    RefPtr<InspectorArray> resultArray;
    if (resultValue->asArray(resultArray)) {
        RefPtr<InspectorArray> returnValueArray = InspectorArray::create();
        unsigned resultArrayLength = resultArray->length();
        for (unsigned i = 0; i < resultArrayLength; ++i)
            returnValueArray->pushValue(handleScriptResult(resultArray->get(i)));
        return returnValueArray;
    }

    if (auto element = createElement(RefPtr<InspectorValue>(resultValue)))
        return element;

    RefPtr<InspectorObject> resultObject;
    if (resultValue->asObject(resultObject)) {
        RefPtr<InspectorObject> returnValueObject = InspectorObject::create();
        auto end = resultObject->end();
        for (auto it = resultObject->begin(); it != end; ++it)
            returnValueObject->setValue(it->key, handleScriptResult(WTFMove(it->value)));
        return returnValueObject;
    }

    return resultValue;
}

void Session::executeScript(const String& script, RefPtr<InspectorArray>&& argumentsArray, ExecuteScriptMode mode, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, script, argumentsArray = WTFMove(argumentsArray), mode, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        RefPtr<InspectorArray> arguments = InspectorArray::create();
        unsigned argumentsLength = argumentsArray->length();
        for (unsigned i = 0; i < argumentsLength; ++i) {
            if (auto argument = argumentsArray->get(i)) {
                if (auto element = extractElement(*argument))
                    arguments->pushString(element->toJSONString());
                else
                    arguments->pushString(argument->toJSONString());
            }
        }

        RefPtr<InspectorObject> parameters = InspectorObject::create();
        parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString(ASCIILiteral("frameHandle"), m_currentBrowsingContext.value());
        parameters->setString(ASCIILiteral("function"), "function(){" + script + '}');
        parameters->setArray(ASCIILiteral("arguments"), WTFMove(arguments));
        if (mode == ExecuteScriptMode::Async) {
            parameters->setBoolean(ASCIILiteral("expectsImplicitCallbackArgument"), true);
            if (m_timeouts.script)
                parameters->setInteger(ASCIILiteral("callbackTimeout"), m_timeouts.script.value().millisecondsAs<int>());
        }
        m_host->sendCommandToBackend(ASCIILiteral("evaluateJavaScriptFunction"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            String valueString;
            if (!response.responseObject->getString(ASCIILiteral("result"), valueString)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            if (valueString.isEmpty()) {
                completionHandler(CommandResult::success());
                return;
            }
            RefPtr<InspectorValue> resultValue;
            if (!InspectorValue::parseJSON(valueString, resultValue)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            completionHandler(CommandResult::success(handleScriptResult(WTFMove(resultValue))));
        });
    });
}

void Session::performMouseInteraction(int x, int y, MouseButton button, MouseInteraction interaction, Function<void (CommandResult&&)>&& completionHandler)
{
    RefPtr<InspectorObject> parameters = InspectorObject::create();
    parameters->setString(ASCIILiteral("handle"), m_toplevelBrowsingContext.value());
    RefPtr<InspectorObject> position = InspectorObject::create();
    position->setInteger(ASCIILiteral("x"), x);
    position->setInteger(ASCIILiteral("y"), y);
    parameters->setObject(ASCIILiteral("position"), WTFMove(position));
    switch (button) {
    case MouseButton::None:
        parameters->setString(ASCIILiteral("button"), ASCIILiteral("None"));
        break;
    case MouseButton::Left:
        parameters->setString(ASCIILiteral("button"), ASCIILiteral("Left"));
        break;
    case MouseButton::Middle:
        parameters->setString(ASCIILiteral("button"), ASCIILiteral("Middle"));
        break;
    case MouseButton::Right:
        parameters->setString(ASCIILiteral("button"), ASCIILiteral("Right"));
        break;
    }
    switch (interaction) {
    case MouseInteraction::Move:
        parameters->setString(ASCIILiteral("interaction"), ASCIILiteral("Move"));
        break;
    case MouseInteraction::Down:
        parameters->setString(ASCIILiteral("interaction"), ASCIILiteral("Down"));
        break;
    case MouseInteraction::Up:
        parameters->setString(ASCIILiteral("interaction"), ASCIILiteral("Up"));
        break;
    case MouseInteraction::SingleClick:
        parameters->setString(ASCIILiteral("interaction"), ASCIILiteral("SingleClick"));
        break;
    case MouseInteraction::DoubleClick:
        parameters->setString(ASCIILiteral("interaction"), ASCIILiteral("DoubleClick"));
        break;
    }
    parameters->setArray(ASCIILiteral("modifiers"), InspectorArray::create());
    m_host->sendCommandToBackend(ASCIILiteral("performMouseInteraction"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

void Session::performKeyboardInteractions(Vector<KeyboardInteraction>&& interactions, Function<void (CommandResult&&)>&& completionHandler)
{
    RefPtr<InspectorObject> parameters = InspectorObject::create();
    parameters->setString(ASCIILiteral("handle"), m_toplevelBrowsingContext.value());
    RefPtr<InspectorArray> interactionsArray = InspectorArray::create();
    for (const auto& interaction : interactions) {
        RefPtr<InspectorObject> interactionObject = InspectorObject::create();
        switch (interaction.type) {
        case KeyboardInteractionType::KeyPress:
            interactionObject->setString(ASCIILiteral("type"), ASCIILiteral("KeyPress"));
            break;
        case KeyboardInteractionType::KeyRelease:
            interactionObject->setString(ASCIILiteral("type"), ASCIILiteral("KeyRelease"));
            break;
        case KeyboardInteractionType::InsertByKey:
            interactionObject->setString(ASCIILiteral("type"), ASCIILiteral("InsertByKey"));
            break;
        }
        if (interaction.key)
            interactionObject->setString(ASCIILiteral("key"), interaction.key.value());
        if (interaction.text)
            interactionObject->setString(ASCIILiteral("text"), interaction.text.value());
        interactionsArray->pushObject(WTFMove(interactionObject));
    }
    parameters->setArray(ASCIILiteral("interactions"), WTFMove(interactionsArray));
    m_host->sendCommandToBackend(ASCIILiteral("performKeyboardInteractions"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

static std::optional<Session::Cookie> parseAutomationCookie(const InspectorObject& cookieObject)
{
    Session::Cookie cookie;
    if (!cookieObject.getString(ASCIILiteral("name"), cookie.name))
        return std::nullopt;
    if (!cookieObject.getString(ASCIILiteral("value"), cookie.value))
        return std::nullopt;

    String path;
    if (cookieObject.getString(ASCIILiteral("path"), path))
        cookie.path = path;
    String domain;
    if (cookieObject.getString(ASCIILiteral("domain"), domain))
        cookie.domain = domain;
    bool secure;
    if (cookieObject.getBoolean(ASCIILiteral("secure"), secure))
        cookie.secure = secure;
    bool httpOnly;
    if (cookieObject.getBoolean(ASCIILiteral("httpOnly"), httpOnly))
        cookie.httpOnly = httpOnly;
    bool session = false;
    cookieObject.getBoolean(ASCIILiteral("session"), session);
    if (!session) {
        double expiry;
        if (cookieObject.getDouble(ASCIILiteral("expires"), expiry))
            cookie.expiry = expiry;
    }

    return cookie;
}

static RefPtr<InspectorObject> builtAutomationCookie(const Session::Cookie& cookie)
{
    RefPtr<InspectorObject> cookieObject = InspectorObject::create();
    cookieObject->setString(ASCIILiteral("name"), cookie.name);
    cookieObject->setString(ASCIILiteral("value"), cookie.value);
    cookieObject->setString(ASCIILiteral("path"), cookie.path.value_or("/"));
    cookieObject->setString(ASCIILiteral("domain"), cookie.domain.value_or(emptyString()));
    cookieObject->setBoolean(ASCIILiteral("secure"), cookie.secure.value_or(false));
    cookieObject->setBoolean(ASCIILiteral("httpOnly"), cookie.httpOnly.value_or(false));
    cookieObject->setBoolean(ASCIILiteral("session"), !cookie.expiry);
    cookieObject->setDouble(ASCIILiteral("expires"), cookie.expiry.value_or(0));
    return cookieObject;
}

static RefPtr<InspectorObject> serializeCookie(const Session::Cookie& cookie)
{
    RefPtr<InspectorObject> cookieObject = InspectorObject::create();
    cookieObject->setString(ASCIILiteral("name"), cookie.name);
    cookieObject->setString(ASCIILiteral("value"), cookie.value);
    if (cookie.path)
        cookieObject->setString(ASCIILiteral("path"), cookie.path.value());
    if (cookie.domain)
        cookieObject->setString(ASCIILiteral("domain"), cookie.domain.value());
    if (cookie.secure)
        cookieObject->setBoolean(ASCIILiteral("secure"), cookie.secure.value());
    if (cookie.httpOnly)
        cookieObject->setBoolean(ASCIILiteral("httpOnly"), cookie.httpOnly.value());
    if (cookie.expiry)
        cookieObject->setInteger(ASCIILiteral("expiry"), cookie.expiry.value());
    return cookieObject;
}

void Session::getAllCookies(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        RefPtr<InspectorObject> parameters = InspectorObject::create();
        parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
        m_host->sendCommandToBackend(ASCIILiteral("getAllCookies"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            RefPtr<InspectorArray> cookiesArray;
            if (!response.responseObject->getArray(ASCIILiteral("cookies"), cookiesArray)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            RefPtr<InspectorArray> cookies = InspectorArray::create();
            for (unsigned i = 0; i < cookiesArray->length(); ++i) {
                RefPtr<InspectorValue> cookieValue = cookiesArray->get(i);
                RefPtr<InspectorObject> cookieObject;
                if (!cookieValue->asObject(cookieObject)) {
                    completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                    return;
                }

                auto cookie = parseAutomationCookie(*cookieObject);
                if (!cookie) {
                    completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                    return;
                }
                cookies->pushObject(serializeCookie(cookie.value()));
            }
            completionHandler(CommandResult::success(WTFMove(cookies)));
        });
    });
}

void Session::getNamedCookie(const String& name, Function<void (CommandResult&&)>&& completionHandler)
{
    getAllCookies([this, name, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        RefPtr<InspectorArray> cookiesArray;
        result.result()->asArray(cookiesArray);
        for (unsigned i = 0; i < cookiesArray->length(); ++i) {
            RefPtr<InspectorValue> cookieValue = cookiesArray->get(i);
            RefPtr<InspectorObject> cookieObject;
            cookieValue->asObject(cookieObject);
            String cookieName;
            cookieObject->getString(ASCIILiteral("name"), cookieName);
            if (cookieName == name) {
                completionHandler(CommandResult::success(WTFMove(cookieObject)));
                return;
            }
        }
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchCookie));
    });
}

void Session::addCookie(const Cookie& cookie, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, cookie = builtAutomationCookie(cookie), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        RefPtr<InspectorObject> parameters = InspectorObject::create();
        parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
        parameters->setObject(ASCIILiteral("cookie"), WTFMove(cookie));
        m_host->sendCommandToBackend(ASCIILiteral("addSingleCookie"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            completionHandler(CommandResult::success());
        });
    });
}

void Session::deleteCookie(const String& name, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, name, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        RefPtr<InspectorObject> parameters = InspectorObject::create();
        parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
        parameters->setString(ASCIILiteral("cookieName"), name);
        m_host->sendCommandToBackend(ASCIILiteral("deleteSingleCookie"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            completionHandler(CommandResult::success());
        });
    });
}

void Session::deleteAllCookies(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        RefPtr<InspectorObject> parameters = InspectorObject::create();
        parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
        m_host->sendCommandToBackend(ASCIILiteral("deleteAllCookies"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            completionHandler(CommandResult::success());
        });
    });
}

void Session::dismissAlert(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    RefPtr<InspectorObject> parameters = InspectorObject::create();
    parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
    m_host->sendCommandToBackend(ASCIILiteral("dismissCurrentJavaScriptDialog"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

void Session::acceptAlert(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    RefPtr<InspectorObject> parameters = InspectorObject::create();
    parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
    m_host->sendCommandToBackend(ASCIILiteral("acceptCurrentJavaScriptDialog"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

void Session::getAlertText(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    RefPtr<InspectorObject> parameters = InspectorObject::create();
    parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
    m_host->sendCommandToBackend(ASCIILiteral("messageOfCurrentJavaScriptDialog"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError || !response.responseObject) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        String valueString;
        if (!response.responseObject->getString(ASCIILiteral("message"), valueString)) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }
        completionHandler(CommandResult::success(InspectorValue::create(valueString)));
    });
}

void Session::sendAlertText(const String& text, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    RefPtr<InspectorObject> parameters = InspectorObject::create();
    parameters->setString(ASCIILiteral("browsingContextHandle"), m_toplevelBrowsingContext.value());
    parameters->setString(ASCIILiteral("userInput"), text);
    m_host->sendCommandToBackend(ASCIILiteral("setUserInputForCurrentJavaScriptPrompt"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

void Session::takeScreenshot(std::optional<String> elementID, std::optional<bool> scrollIntoView, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, elementID, scrollIntoView, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        RefPtr<InspectorObject> parameters = InspectorObject::create();
        parameters->setString(ASCIILiteral("handle"), m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString(ASCIILiteral("frameHandle"), m_currentBrowsingContext.value());
        if (elementID)
            parameters->setString(ASCIILiteral("nodeHandle"), elementID.value());
        if (scrollIntoView.value_or(false))
            parameters->setBoolean(ASCIILiteral("scrollIntoViewIfNeeded"), true);
        m_host->sendCommandToBackend(ASCIILiteral("takeScreenshot"), WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            String data;
            if (!response.responseObject->getString(ASCIILiteral("data"), data)) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }
            completionHandler(CommandResult::success(InspectorValue::create(data)));
        });
    });
}

} // namespace WebDriver
