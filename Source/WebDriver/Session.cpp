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
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/FileSystem.h>
#include <wtf/HashSet.h>
#include <wtf/HexNumber.h>
#include <wtf/NeverDestroyed.h>

namespace WebDriver {

// https://w3c.github.io/webdriver/webdriver-spec.html#dfn-session-script-timeout
static const double defaultScriptTimeout = 30000;
// https://w3c.github.io/webdriver/webdriver-spec.html#dfn-session-page-load-timeout
static const double defaultPageLoadTimeout = 300000;
// https://w3c.github.io/webdriver/webdriver-spec.html#dfn-session-implicit-wait-timeout
static const double defaultImplicitWaitTimeout = 0;

const String& Session::webElementIdentifier()
{
    // The web element identifier is a constant defined by the spec in Section 11 Elements.
    // https://www.w3.org/TR/webdriver/#elements
    static NeverDestroyed<String> webElementID { "element-6066-11e4-a52e-4f735466cecf"_s };
    return webElementID;
}

Session::Session(std::unique_ptr<SessionHost>&& host)
    : m_host(WTFMove(host))
    , m_scriptTimeout(defaultScriptTimeout)
    , m_pageLoadTimeout(defaultPageLoadTimeout)
    , m_implicitWaitTimeout(defaultImplicitWaitTimeout)
{
    if (capabilities().timeouts)
        setTimeouts(capabilities().timeouts.value(), [](CommandResult&&) { });
}

Session::~Session()
{
}

const String& Session::id() const
{
    return m_host->sessionID();
}

const Capabilities& Session::capabilities() const
{
    return m_host->capabilities();
}

bool Session::isConnected() const
{
    return m_host->isConnected();
}

static Optional<String> firstWindowHandleInResult(JSON::Value& result)
{
    auto handles = result.asArray();
    if (handles && handles->length()) {
        auto handle = handles->get(0)->asString();
        if (!!handle)
            return handle;
    }
    return WTF::nullopt;
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
    m_toplevelBrowsingContext = WTF::nullopt;
    m_currentBrowsingContext = WTF::nullopt;
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

void Session::getTimeouts(Function<void (CommandResult&&)>&& completionHandler)
{
    auto parameters = JSON::Object::create();
    if (m_scriptTimeout == std::numeric_limits<double>::infinity())
        parameters->setValue("script"_s, JSON::Value::null());
    else
        parameters->setDouble("script"_s, m_scriptTimeout);
    parameters->setDouble("pageLoad"_s, m_pageLoadTimeout);
    parameters->setDouble("implicit"_s, m_implicitWaitTimeout);
    completionHandler(CommandResult::success(WTFMove(parameters)));
}

void Session::setTimeouts(const Timeouts& timeouts, Function<void (CommandResult&&)>&& completionHandler)
{
    if (timeouts.script)
        m_scriptTimeout = timeouts.script.value();
    if (timeouts.pageLoad)
        m_pageLoadTimeout = timeouts.pageLoad.value();
    if (timeouts.implicit)
        m_implicitWaitTimeout = timeouts.implicit.value();
    completionHandler(CommandResult::success());
}

void Session::switchToTopLevelBrowsingContext(const String& toplevelBrowsingContext)
{
    m_toplevelBrowsingContext = toplevelBrowsingContext;
    m_currentBrowsingContext = String();
}

void Session::switchToBrowsingContext(const String& browsingContext)
{
    m_currentBrowsingContext = browsingContext;
}

Optional<String> Session::pageLoadStrategyString() const
{
    if (!capabilities().pageLoadStrategy)
        return WTF::nullopt;

    switch (capabilities().pageLoadStrategy.value()) {
    case PageLoadStrategy::None:
        return String("None");
    case PageLoadStrategy::Normal:
        return String("Normal");
    case PageLoadStrategy::Eager:
        return String("Eager");
    }

    return WTF::nullopt;
}

void Session::createTopLevelBrowsingContext(Function<void (CommandResult&&)>&& completionHandler)
{
    ASSERT(!m_toplevelBrowsingContext);
    m_host->sendCommandToBackend("createBrowsingContext"_s, nullptr, [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
        if (response.isError || !response.responseObject) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }

        auto handle = response.responseObject->getString("handle"_s);
        if (!handle) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }

        switchToTopLevelBrowsingContext(handle);
        completionHandler(CommandResult::success());
    });
}

void Session::handleUserPrompts(Function<void (CommandResult&&)>&& completionHandler)
{
    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
    m_host->sendCommandToBackend("isShowingJavaScriptDialog"_s, WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
        if (response.isError || !response.responseObject) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }

        auto isShowingJavaScriptDialog = response.responseObject->getBoolean("result");
        if (!isShowingJavaScriptDialog) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }

        if (!isShowingJavaScriptDialog.value()) {
            completionHandler(CommandResult::success());
            return;
        }

        handleUnexpectedAlertOpen(WTFMove(completionHandler));
    });
}

void Session::handleUnexpectedAlertOpen(Function<void (CommandResult&&)>&& completionHandler)
{
    switch (capabilities().unhandledPromptBehavior.valueOr(UnhandledPromptBehavior::DismissAndNotify)) {
    case UnhandledPromptBehavior::Dismiss:
        dismissAlert(WTFMove(completionHandler));
        break;
    case UnhandledPromptBehavior::Accept:
        acceptAlert(WTFMove(completionHandler));
        break;
    case UnhandledPromptBehavior::DismissAndNotify:
        dismissAndNotifyAlert(WTFMove(completionHandler));
        break;
    case UnhandledPromptBehavior::AcceptAndNotify:
        acceptAndNotifyAlert(WTFMove(completionHandler));
        break;
    case UnhandledPromptBehavior::Ignore:
        reportUnexpectedAlertOpen(WTFMove(completionHandler));
        break;
    }
}

void Session::dismissAndNotifyAlert(Function<void (CommandResult&&)>&& completionHandler)
{
    reportUnexpectedAlertOpen([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        dismissAlert([errorResult = WTFMove(result), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
            if (result.isError()) {
                completionHandler(WTFMove(result));
                return;
            }
            completionHandler(WTFMove(errorResult));
        });
    });
}

void Session::acceptAndNotifyAlert(Function<void (CommandResult&&)>&& completionHandler)
{
    reportUnexpectedAlertOpen([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        acceptAlert([errorResult = WTFMove(result), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
            if (result.isError()) {
                completionHandler(WTFMove(result));
                return;
            }
            completionHandler(WTFMove(errorResult));
        });
    });
}

void Session::reportUnexpectedAlertOpen(Function<void (CommandResult&&)>&& completionHandler)
{
    getAlertText([completionHandler = WTFMove(completionHandler)](CommandResult&& result) {
        Optional<String> alertText;
        if (!result.isError()) {
            auto valueString = result.result()->asString();
            if (!!valueString)
                alertText = valueString;
        }
        auto errorResult = CommandResult::fail(CommandResult::ErrorCode::UnexpectedAlertOpen);
        if (alertText) {
            auto additonalData = JSON::Object::create();
            additonalData->setString("text"_s, alertText.value());
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

    handleUserPrompts([this, protectedThis = makeRef(*this), url, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        auto parameters = JSON::Object::create();
        parameters->setString("handle"_s, m_toplevelBrowsingContext.value());
        parameters->setString("url"_s, url);
        parameters->setDouble("pageLoadTimeout"_s, m_pageLoadTimeout);
        if (auto pageLoadStrategy = pageLoadStrategyString())
            parameters->setString("pageLoadStrategy"_s, pageLoadStrategy.value());
        m_host->sendCommandToBackend("navigateBrowsingContext"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            switchToBrowsingContext({ });
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

    handleUserPrompts([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        auto parameters = JSON::Object::create();
        parameters->setString("handle"_s, m_toplevelBrowsingContext.value());
        m_host->sendCommandToBackend("getBrowsingContext"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto browsingContext = response.responseObject->getObject("context");
            if (!browsingContext) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            auto url = browsingContext->getString("url");
            if (!url) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            completionHandler(CommandResult::success(JSON::Value::create(url)));
        });
    });
}

void Session::back(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("handle"_s, m_toplevelBrowsingContext.value());
        parameters->setDouble("pageLoadTimeout"_s, m_pageLoadTimeout);
        if (auto pageLoadStrategy = pageLoadStrategyString())
            parameters->setString("pageLoadStrategy"_s, pageLoadStrategy.value());
        m_host->sendCommandToBackend("goBackInBrowsingContext"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            switchToBrowsingContext({ });
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

    handleUserPrompts([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("handle"_s, m_toplevelBrowsingContext.value());
        parameters->setDouble("pageLoadTimeout"_s, m_pageLoadTimeout);
        if (auto pageLoadStrategy = pageLoadStrategyString())
            parameters->setString("pageLoadStrategy"_s, pageLoadStrategy.value());
        m_host->sendCommandToBackend("goForwardInBrowsingContext"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            switchToBrowsingContext({ });
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

    handleUserPrompts([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("handle"_s, m_toplevelBrowsingContext.value());
        parameters->setDouble("pageLoadTimeout"_s, m_pageLoadTimeout);
        if (auto pageLoadStrategy = pageLoadStrategyString())
            parameters->setString("pageLoadStrategy"_s, pageLoadStrategy.value());
        m_host->sendCommandToBackend("reloadBrowsingContext"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            switchToBrowsingContext({ });
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

    handleUserPrompts([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
        parameters->setString("function"_s, "function() { return document.title; }"_s);
        parameters->setArray("arguments"_s, JSON::Array::create());
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
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

    auto parameters = JSON::Object::create();
    parameters->setString("handle"_s, m_toplevelBrowsingContext.value());
    m_host->sendCommandToBackend("getBrowsingContext"_s, WTFMove(parameters), [protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError || !response.responseObject) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }

        auto browsingContext = response.responseObject->getObject("context"_s);
        if (!browsingContext) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }

        auto handle = browsingContext->getString("handle"_s);
        if (!handle) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }

        completionHandler(CommandResult::success(JSON::Value::create(handle)));
    });
}

void Session::closeTopLevelBrowsingContext(const String& toplevelBrowsingContext, Function<void (CommandResult&&)>&& completionHandler)
{
    auto parameters = JSON::Object::create();
    parameters->setString("handle"_s, toplevelBrowsingContext);
    m_host->sendCommandToBackend("closeBrowsingContext"_s, WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
        if (!m_host->isConnected()) {
            // Closing the browsing context made the browser quit.
            completionHandler(CommandResult::success(JSON::Array::create()));
            return;
        }
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }

        getWindowHandles([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) {
            if (!m_host->isConnected()) {
                // Closing the browsing context made the browser quit.
                completionHandler(CommandResult::success(JSON::Array::create()));
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
        auto toplevelBrowsingContext = std::exchange(m_toplevelBrowsingContext, WTF::nullopt);
        m_currentBrowsingContext = WTF::nullopt;
        closeTopLevelBrowsingContext(toplevelBrowsingContext.value(), WTFMove(completionHandler));
    });
}

void Session::switchToWindow(const String& windowHandle, Function<void (CommandResult&&)>&& completionHandler)
{
    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, windowHandle);
    m_host->sendCommandToBackend("switchToBrowsingContext"_s, WTFMove(parameters), [this, protectedThis = makeRef(*this), windowHandle, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
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
    m_host->sendCommandToBackend("getBrowsingContexts"_s, JSON::Object::create(), [protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError || !response.responseObject) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }

        auto browsingContextArray = response.responseObject->getArray("contexts"_s);
        if (!browsingContextArray) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }

        auto windowHandles = JSON::Array::create();
        for (unsigned i = 0; i < browsingContextArray->length(); ++i) {
            auto browsingContext = browsingContextArray->get(i)->asObject();
            if (!browsingContext) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            auto handle = browsingContext->getString("handle"_s);
            if (!handle) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            windowHandles->pushString(handle);
        }
        completionHandler(CommandResult::success(WTFMove(windowHandles)));
    });
}

void Session::newWindow(Optional<String> typeHint, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), typeHint, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        RefPtr<JSON::Object> parameters;
        if (typeHint) {
            parameters = JSON::Object::create();
            parameters->setString("presentationHint"_s, typeHint.value() == "window" ? "Window"_s : "Tab"_s);
        }
        m_host->sendCommandToBackend("createBrowsingContext"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto handle = response.responseObject->getString("handle"_s);
            if (!handle) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            auto presentation = response.responseObject->getString("presentation"_s);
            if (!presentation) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            auto result = JSON::Object::create();
            result->setString("handle"_s, handle);
            result->setString("type"_s, presentation == "Window"_s ? "window"_s : "tab"_s);
            completionHandler(CommandResult::success(WTFMove(result)));
        });
    });
}

void Session::switchToFrame(RefPtr<JSON::Value>&& frameID, Function<void (CommandResult&&)>&& completionHandler)
{
    if (frameID->isNull()) {
        if (!m_toplevelBrowsingContext) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
            return;
        }
        switchToBrowsingContext({ });
        completionHandler(CommandResult::success());
        return;
    }

    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), frameID = WTFMove(frameID), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());

        if (auto frameIndex = frameID->asInteger()) {
            ASSERT(*frameIndex >= 0 && *frameIndex < std::numeric_limits<unsigned short>::max());
            parameters->setInteger("ordinal"_s, *frameIndex);
        } else {
            String frameElementID = extractElementID(*frameID);
            ASSERT(!frameElementID.isEmpty());
            parameters->setString("nodeHandle"_s, frameElementID);
        }

        m_host->sendCommandToBackend("resolveChildFrameHandle"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto frameHandle = response.responseObject->getString("result"_s);
            if (!frameHandle) {
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

    handleUserPrompts([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
        parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        m_host->sendCommandToBackend("resolveParentFrameHandle"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto frameHandle = response.responseObject->getString("result"_s);
            if (!frameHandle) {
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
    auto parameters = JSON::Object::create();
    parameters->setString("handle"_s, m_toplevelBrowsingContext.value());
    m_host->sendCommandToBackend("getBrowsingContext"_s, WTFMove(parameters), [protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError || !response.responseObject) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }

        auto browsingContext = response.responseObject->getObject("context"_s);
        if (!browsingContext) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }

        auto windowOrigin = browsingContext->getObject("windowOrigin"_s);
        if (!windowOrigin) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }

        auto x = windowOrigin->getDouble("x"_s);
        if (!x) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }

        auto y = windowOrigin->getDouble("y"_s);
        if (!y) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }

        auto windowSize = browsingContext->getObject("windowSize"_s);
        if (!windowSize) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }

        auto width = windowSize->getDouble("width"_s);
        if (!width) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }

        auto height = windowSize->getDouble("height"_s);
        if (!height) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }

        auto windowRect = JSON::Object::create();
        windowRect->setDouble("x"_s, *x);
        windowRect->setDouble("y"_s, *y);
        windowRect->setDouble("width"_s, *width);
        windowRect->setDouble("height"_s, *height);
        completionHandler(CommandResult::success(WTFMove(windowRect)));
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

void Session::setWindowRect(Optional<double> x, Optional<double> y, Optional<double> width, Optional<double> height, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), x, y, width, height, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        auto parameters = JSON::Object::create();
        parameters->setString("handle"_s, m_toplevelBrowsingContext.value());
        if (x && y) {
            auto windowOrigin = JSON::Object::create();
            windowOrigin->setDouble("x", x.value());
            windowOrigin->setDouble("y", y.value());
            parameters->setObject("origin"_s, WTFMove(windowOrigin));
        }
        if (width && height) {
            auto windowSize = JSON::Object::create();
            windowSize->setDouble("width", width.value());
            windowSize->setDouble("height", height.value());
            parameters->setObject("size"_s, WTFMove(windowSize));
        }
        m_host->sendCommandToBackend("setWindowFrameOfBrowsingContext"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)] (SessionHost::CommandResponse&& response) mutable {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            getToplevelBrowsingContextRect(WTFMove(completionHandler));
        });
    });
}

void Session::maximizeWindow(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        auto parameters = JSON::Object::create();
        parameters->setString("handle"_s, m_toplevelBrowsingContext.value());
        m_host->sendCommandToBackend("maximizeWindowOfBrowsingContext"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)] (SessionHost::CommandResponse&& response) mutable {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            getToplevelBrowsingContextRect(WTFMove(completionHandler));
        });
    });
}

void Session::minimizeWindow(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        auto parameters = JSON::Object::create();
        parameters->setString("handle"_s, m_toplevelBrowsingContext.value());
        m_host->sendCommandToBackend("hideWindowOfBrowsingContext"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)] (SessionHost::CommandResponse&& response) mutable {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            getToplevelBrowsingContextRect(WTFMove(completionHandler));
        });
    });
}

void Session::fullscreenWindow(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
        parameters->setString("function"_s, String(EnterFullscreenJavaScript, sizeof(EnterFullscreenJavaScript)));
        parameters->setArray("arguments"_s, JSON::Array::create());
        parameters->setBoolean("expectsImplicitCallbackArgument"_s, true);
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            getToplevelBrowsingContextRect(WTFMove(completionHandler));
        });
    });
}

RefPtr<JSON::Object> Session::createElement(RefPtr<JSON::Value>&& value)
{
    if (!value)
        return nullptr;

    auto valueObject = value->asObject();
    if (!valueObject)
        return nullptr;

    auto elementID = valueObject->getString("session-node-" + id());
    if (!elementID)
        return nullptr;

    auto elementObject = JSON::Object::create();
    elementObject->setString(webElementIdentifier(), elementID);
    return elementObject;
}

Ref<JSON::Object> Session::createElement(const String& elementID)
{
    auto elementObject = JSON::Object::create();
    elementObject->setString("session-node-" + id(), elementID);
    return elementObject;
}

RefPtr<JSON::Object> Session::extractElement(JSON::Value& value)
{
    String elementID = extractElementID(value);
    return !elementID.isEmpty() ? createElement(elementID).ptr() : nullptr;
}

String Session::extractElementID(JSON::Value& value)
{
    auto valueObject = value.asObject();
    if (!valueObject)
        return emptyString();

    auto elementID = valueObject->getString(webElementIdentifier());
    if (!elementID)
        return emptyString();

    return elementID;
}

void Session::computeElementLayout(const String& elementID, OptionSet<ElementLayoutOption> options, Function<void (Optional<Rect>&&, Optional<Point>&&, bool, RefPtr<JSON::Object>&&)>&& completionHandler)
{
    ASSERT(m_toplevelBrowsingContext.value());

    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
    parameters->setString("frameHandle"_s, m_currentBrowsingContext.valueOr(emptyString()));
    parameters->setString("nodeHandle"_s, elementID);
    parameters->setBoolean("scrollIntoViewIfNeeded"_s, options.contains(ElementLayoutOption::ScrollIntoViewIfNeeded));
    parameters->setString("coordinateSystem"_s, options.contains(ElementLayoutOption::UseViewportCoordinates) ? "LayoutViewport"_s : "Page"_s);
    m_host->sendCommandToBackend("computeElementLayout"_s, WTFMove(parameters), [protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
        if (response.isError || !response.responseObject) {
            completionHandler(WTF::nullopt, WTF::nullopt, false, WTFMove(response.responseObject));
            return;
        }

        auto rectObject = response.responseObject->getObject("rect"_s);
        if (!rectObject) {
            completionHandler(WTF::nullopt, WTF::nullopt, false, nullptr);
            return;
        }

        Optional<int> elementX;
        Optional<int> elementY;
        auto elementPosition = rectObject->getObject("origin"_s);
        if (elementPosition) {
            elementX = elementPosition->getInteger("x"_s);
            elementY = elementPosition->getInteger("y"_s);
        }
        if (!elementX || !elementY) {
            completionHandler(WTF::nullopt, WTF::nullopt, false, nullptr);
            return;
        }

        Optional<int> elementWidth;
        Optional<int> elementHeight;
        auto elementSize = rectObject->getObject("size"_s);
        if (elementSize) {
            elementWidth = elementSize->getInteger("width"_s);
            elementHeight = elementSize->getInteger("height"_s);
        }
        if (!elementWidth || !elementHeight) {
            completionHandler(WTF::nullopt, WTF::nullopt, false, nullptr);
            return;
        }

        Rect rect = { { elementX.value(), elementY.value() }, { elementWidth.value(), elementHeight.value() } };

        auto isObscured = response.responseObject->getBoolean("isObscured"_s);
        if (!isObscured) {
            completionHandler(WTF::nullopt, WTF::nullopt, false, nullptr);
            return;
        }

        auto inViewCenterPointObject = response.responseObject->getObject("inViewCenterPoint"_s);
        if (!inViewCenterPointObject) {
            completionHandler(rect, WTF::nullopt, *isObscured, nullptr);
            return;
        }

        auto inViewCenterPointX = inViewCenterPointObject->getInteger("x"_s);
        auto inViewCenterPointY = inViewCenterPointObject->getInteger("y"_s);
        if (!inViewCenterPointX || !inViewCenterPointY) {
            completionHandler(WTF::nullopt, WTF::nullopt, *isObscured, nullptr);
            return;
        }

        Point inViewCenterPoint = { *inViewCenterPointX, *inViewCenterPointY };
        completionHandler(rect, inViewCenterPoint, *isObscured, nullptr);
    });
}

void Session::findElements(const String& strategy, const String& selector, FindElementsMode mode, const String& rootElementID, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), strategy, selector, mode, rootElementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto arguments = JSON::Array::create();
        arguments->pushString(JSON::Value::create(strategy)->toJSONString());
        if (rootElementID.isEmpty())
            arguments->pushString(JSON::Value::null()->toJSONString());
        else
            arguments->pushString(createElement(rootElementID)->toJSONString());
        arguments->pushString(JSON::Value::create(selector)->toJSONString());
        arguments->pushString(JSON::Value::create(mode == FindElementsMode::Single)->toJSONString());
        arguments->pushString(JSON::Value::create(m_implicitWaitTimeout)->toJSONString());

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        parameters->setString("function"_s, String(FindNodesJavaScript, sizeof(FindNodesJavaScript)));
        parameters->setArray("arguments"_s, WTFMove(arguments));
        parameters->setBoolean("expectsImplicitCallbackArgument"_s, true);
        // If there's an implicit wait, use one second more as callback timeout.
        if (m_implicitWaitTimeout)
            parameters->setDouble("callbackTimeout"_s, m_implicitWaitTimeout + 1000);

        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [this, protectedThis, mode, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }


            switch (mode) {
            case FindElementsMode::Single: {
                auto elementObject = createElement(WTFMove(resultValue));
                if (!elementObject) {
                    completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchElement));
                    return;
                }
                completionHandler(CommandResult::success(WTFMove(elementObject)));
                break;
            }
            case FindElementsMode::Multiple: {
                auto elementsArray = resultValue->asArray();
                if (!elementsArray) {
                    completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchElement));
                    return;
                }

                auto elementObjectsArray = JSON::Array::create();
                unsigned elementsArrayLength = elementsArray->length();
                for (unsigned i = 0; i < elementsArrayLength; ++i) {
                    if (auto elementObject = createElement(elementsArray->get(i)))
                        elementObjectsArray->pushObject(elementObject.releaseNonNull());
                }
                completionHandler(CommandResult::success(WTFMove(elementObjectsArray)));
                break;
            }
            }
        });
    });
}

void Session::getActiveElement(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
        parameters->setString("function"_s, "function() { return document.activeElement; }"_s);
        parameters->setArray("arguments"_s, JSON::Array::create());
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            auto elementObject = createElement(WTFMove(resultValue));
            if (!elementObject) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchElement));
                return;
            }
            completionHandler(CommandResult::success(WTFMove(elementObject)));
        });
    });
}

void Session::isElementSelected(const String& elementID, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto arguments = JSON::Array::create();
        arguments->pushString(createElement(elementID)->toJSONString());
        arguments->pushString(JSON::Value::create("selected")->toJSONString());

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        parameters->setString("function"_s, String(ElementAttributeJavaScript, sizeof(ElementAttributeJavaScript)));
        parameters->setArray("arguments"_s, WTFMove(arguments));
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            if (resultValue->isNull()) {
                completionHandler(CommandResult::success(JSON::Value::create(false)));
                return;
            }

            auto booleanResult = resultValue->asString();
            if (!booleanResult || booleanResult != "true") {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            completionHandler(CommandResult::success(JSON::Value::create(true)));
        });
    });
}

void Session::getElementText(const String& elementID, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto arguments = JSON::Array::create();
        arguments->pushString(createElement(elementID)->toJSONString());

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        // FIXME: Add an atom to properly implement this instead of just using innerText.
        parameters->setString("function"_s, "function(element) { return element.innerText.replace(/^[^\\S\\xa0]+|[^\\S\\xa0]+$/g, '') }"_s);
        parameters->setArray("arguments"_s, WTFMove(arguments));
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

void Session::getElementTagName(const String& elementID, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto arguments = JSON::Array::create();
        arguments->pushString(createElement(elementID)->toJSONString());

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        parameters->setString("function"_s, "function(element) { return element.tagName.toLowerCase() }"_s);
        parameters->setArray("arguments"_s, WTFMove(arguments));
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

void Session::getElementRect(const String& elementID, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        computeElementLayout(elementID, { }, [protectedThis, completionHandler = WTFMove(completionHandler)](Optional<Rect>&& rect, Optional<Point>&&, bool, RefPtr<JSON::Object>&& error) {
            if (!rect || error) {
                completionHandler(CommandResult::fail(WTFMove(error)));
                return;
            }
            auto rectObject = JSON::Object::create();
            rectObject->setInteger("x"_s, rect.value().origin.x);
            rectObject->setInteger("y"_s, rect.value().origin.y);
            rectObject->setInteger("width"_s, rect.value().size.width);
            rectObject->setInteger("height"_s, rect.value().size.height);
            completionHandler(CommandResult::success(WTFMove(rectObject)));
        });
    });
}

void Session::isElementEnabled(const String& elementID, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto arguments = JSON::Array::create();
        arguments->pushString(createElement(elementID)->toJSONString());

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        parameters->setString("function"_s, String(ElementEnabledJavaScript, sizeof(ElementEnabledJavaScript)));
        parameters->setArray("arguments"_s, WTFMove(arguments));
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
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

    handleUserPrompts([this, protectedThis = makeRef(*this), elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto arguments = JSON::Array::create();
        arguments->pushString(createElement(elementID)->toJSONString());

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        parameters->setString("function"_s, String(ElementDisplayedJavaScript, sizeof(ElementDisplayedJavaScript)));
        parameters->setArray("arguments"_s, WTFMove(arguments));
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

void Session::getElementAttribute(const String& elementID, const String& attribute, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), elementID, attribute, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto arguments = JSON::Array::create();
        arguments->pushString(createElement(elementID)->toJSONString());
        arguments->pushString(JSON::Value::create(attribute)->toJSONString());

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        parameters->setString("function"_s, String(ElementAttributeJavaScript, sizeof(ElementAttributeJavaScript)));
        parameters->setArray("arguments"_s, WTFMove(arguments));
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

void Session::getElementProperty(const String& elementID, const String& property, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), elementID, property, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto arguments = JSON::Array::create();
        arguments->pushString(createElement(elementID)->toJSONString());

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        parameters->setString("function"_s, makeString("function(element) { return element.", property, "; }"));
        parameters->setArray("arguments"_s, WTFMove(arguments));
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

void Session::getElementCSSValue(const String& elementID, const String& cssProperty, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), elementID, cssProperty, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto arguments = JSON::Array::create();
        arguments->pushString(createElement(elementID)->toJSONString());

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        parameters->setString("function"_s, makeString("function(element) { return document.defaultView.getComputedStyle(element).getPropertyValue('", cssProperty, "'); }"));
        parameters->setArray("arguments"_s, WTFMove(arguments));
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

void Session::waitForNavigationToComplete(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::success());
        return;
    }

    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
    if (m_currentBrowsingContext)
        parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
    parameters->setDouble("pageLoadTimeout"_s, m_pageLoadTimeout);
    if (auto pageLoadStrategy = pageLoadStrategyString())
        parameters->setString("pageLoadStrategy"_s, pageLoadStrategy.value());
    m_host->sendCommandToBackend("waitForNavigationToComplete"_s, WTFMove(parameters), [this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            auto result = CommandResult::fail(WTFMove(response.responseObject));
            switch (result.errorCode()) {
            case CommandResult::ErrorCode::NoSuchWindow:
                // Window was closed, reset the top level browsing context and ignore the error.
                m_toplevelBrowsingContext = WTF::nullopt;
                m_currentBrowsingContext = WTF::nullopt;
                break;
            case CommandResult::ErrorCode::NoSuchFrame:
                // Navigation destroyed the current frame, reset the current browsing context and ignore the error.
                m_currentBrowsingContext = WTF::nullopt;
                break;
            default:
                completionHandler(WTFMove(result));
                return;
            }
        }
        completionHandler(CommandResult::success());
    });
}

void Session::elementIsFileUpload(const String& elementID, Function<void (CommandResult&&)>&& completionHandler)
{
    auto arguments = JSON::Array::create();
    arguments->pushString(createElement(elementID)->toJSONString());

    static const char isFileUploadScript[] =
        "function(element) {"
        "    if (element.tagName.toLowerCase() === 'input' && element.type === 'file')"
        "        return { 'fileUpload': true, 'multiple': element.hasAttribute('multiple') };"
        "    return { 'fileUpload': false };"
        "}";

    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
    if (m_currentBrowsingContext)
        parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
    parameters->setString("function"_s, isFileUploadScript);
    parameters->setArray("arguments"_s, WTFMove(arguments));
    m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError || !response.responseObject) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }

        auto valueString = response.responseObject->getString("result"_s);
        if (!valueString) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }

        auto resultValue = JSON::Value::parseJSON(valueString);
        if (!resultValue) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }

        completionHandler(CommandResult::success(WTFMove(resultValue)));
    });
}

Optional<Session::FileUploadType> Session::parseElementIsFileUploadResult(const RefPtr<JSON::Value>& resultValue)
{
    if (!resultValue)
        return WTF::nullopt;

    auto result = resultValue->asObject();
    if (!result)
        return WTF::nullopt;

    auto isFileUpload = result->getBoolean("fileUpload"_s);
    if (!isFileUpload || !*isFileUpload)
        return WTF::nullopt;

    auto multiple = result->getBoolean("multiple"_s);
    if (!multiple || !*multiple)
        return FileUploadType::Single;

    return FileUploadType::Multiple;
}

void Session::selectOptionElement(const String& elementID, Function<void (CommandResult&&)>&& completionHandler)
{
    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
    parameters->setString("frameHandle"_s, m_currentBrowsingContext.valueOr(emptyString()));
    parameters->setString("nodeHandle"_s, elementID);
    m_host->sendCommandToBackend("selectOptionElement"_s, WTFMove(parameters), [protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

void Session::elementClick(const String& elementID, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        elementIsFileUpload(elementID, [this, protectedThis, elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
            if (result.isError()) {
                completionHandler(WTFMove(result));
                return;
            }

            if (parseElementIsFileUploadResult(result.result())) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
                return;
            }
            OptionSet<ElementLayoutOption> options = { ElementLayoutOption::ScrollIntoViewIfNeeded, ElementLayoutOption::UseViewportCoordinates };
            computeElementLayout(elementID, options, [this, protectedThis, elementID, completionHandler = WTFMove(completionHandler)](Optional<Rect>&& rect, Optional<Point>&& inViewCenter, bool isObscured, RefPtr<JSON::Object>&& error) mutable {
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

                getElementTagName(elementID, [this, elementID, inViewCenter = WTFMove(inViewCenter), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
                    bool isOptionElement = false;
                    if (!result.isError()) {
                        auto tagName = result.result()->asString();
                        if (!!tagName)
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
        });
    });
}

void Session::elementIsEditable(const String& elementID, Function<void (CommandResult&&)>&& completionHandler)
{
    auto arguments = JSON::Array::create();
    arguments->pushString(createElement(elementID)->toJSONString());

    static const char isEditableScript[] =
        "function(element) {"
        "    if (element.disabled || element.readOnly)"
        "        return false;"
        "    var tagName = element.tagName.toLowerCase();"
        "    if (tagName === 'textarea' || element.isContentEditable)"
        "        return true;"
        "    if (tagName != 'input')"
        "        return false;"
        "    switch (element.type) {"
        "    case 'color': case 'date': case 'datetime-local': case 'email': case 'file': case 'month': case 'number': "
        "    case 'password': case 'range': case 'search': case 'tel': case 'text': case 'time': case 'url': case 'week':"
        "        return true;"
        "    }"
        "    return false;"
        "}";

    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
    if (m_currentBrowsingContext)
        parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
    parameters->setString("function"_s, isEditableScript);
    parameters->setArray("arguments"_s, WTFMove(arguments));
    m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError || !response.responseObject) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }

        auto valueString = response.responseObject->getString("result"_s);
        if (!valueString) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }

        auto resultValue = JSON::Value::parseJSON(valueString);
        if (!resultValue) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }

        completionHandler(CommandResult::success(WTFMove(resultValue)));
    });
}

void Session::elementClear(const String& elementID, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        elementIsEditable(elementID, [this, protectedThis, elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
            if (result.isError()) {
                completionHandler(WTFMove(result));
                return;
            }

            auto isEditable = result.result()->asBoolean();
            if (!isEditable || !*isEditable) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidElementState));
                return;
            }

            OptionSet<ElementLayoutOption> options = { ElementLayoutOption::ScrollIntoViewIfNeeded };
            computeElementLayout(elementID, options, [this, protectedThis, elementID, completionHandler = WTFMove(completionHandler)](Optional<Rect>&& rect, Optional<Point>&& inViewCenter, bool, RefPtr<JSON::Object>&& error) mutable {
                if (!rect || error) {
                    completionHandler(CommandResult::fail(WTFMove(error)));
                    return;
                }
                if (!inViewCenter) {
                    completionHandler(CommandResult::fail(CommandResult::ErrorCode::ElementNotInteractable));
                    return;
                }
                auto arguments = JSON::Array::create();
                arguments->pushString(createElement(elementID)->toJSONString());

                auto parameters = JSON::Object::create();
                parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
                if (m_currentBrowsingContext)
                    parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
                parameters->setString("function"_s, String(FormElementClearJavaScript, sizeof(FormElementClearJavaScript)));
                parameters->setArray("arguments"_s, WTFMove(arguments));
                m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
                    if (response.isError) {
                        completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                        return;
                    }
                    completionHandler(CommandResult::success());
                });
            });
        });
    });
}

void Session::setInputFileUploadFiles(const String& elementID, const String& text, bool multiple, Function<void (CommandResult&&)>&& completionHandler)
{
    Vector<String> files = text.split('\n');
    if (files.isEmpty()) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    if (!multiple && files.size() != 1) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    auto filenames = JSON::Array::create();
    for (const auto& file : files) {
        if (!FileSystem::fileExists(file)) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
            return;
        }
        filenames->pushString(file);
    }

    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
    parameters->setString("frameHandle"_s, m_currentBrowsingContext.valueOr(emptyString()));
    parameters->setString("nodeHandle"_s, elementID);
    parameters->setArray("filenames"_s, WTFMove(filenames));
    m_host->sendCommandToBackend("setFilesForInputFileUpload"_s, WTFMove(parameters), [protectedThis = makeRef(*this), elementID, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }

        completionHandler(CommandResult::success());
    });
}

String Session::virtualKeyForKey(UChar key, KeyModifier& modifier)
{
    // §17.4.2 Keyboard Actions.
    // https://www.w3.org/TR/webdriver/#keyboard-actions
    modifier = KeyModifier::None;
    switch (key) {
    case 0xE001U:
        return "Cancel"_s;
    case 0xE002U:
        return "Help"_s;
    case 0xE003U:
        return "Backspace"_s;
    case 0xE004U:
        return "Tab"_s;
    case 0xE005U:
        return "Clear"_s;
    case 0xE006U:
        return "Return"_s;
    case 0xE007U:
        return "Enter"_s;
    case 0xE008U:
    case 0xE050U:
        modifier = KeyModifier::Shift;
        return "Shift"_s;
    case 0xE009U:
    case 0xE051U:
        modifier = KeyModifier::Control;
        return "Control"_s;
    case 0xE00AU:
    case 0xE052U:
        modifier = KeyModifier::Alternate;
        return "Alternate"_s;
    case 0xE00BU:
        return "Pause"_s;
    case 0xE00CU:
        return "Escape"_s;
    case 0xE00DU:
        return "Space"_s;
    case 0xE00EU:
    case 0xE054U:
        return "PageUp"_s;
    case 0xE00FU:
    case 0xE055U:
        return "PageDown"_s;
    case 0xE010U:
    case 0xE056U:
        return "End"_s;
    case 0xE011U:
    case 0xE057U:
        return "Home"_s;
    case 0xE012U:
    case 0xE058U:
        return "LeftArrow"_s;
    case 0xE013U:
    case 0xE059U:
        return "UpArrow"_s;
    case 0xE014U:
    case 0xE05AU:
        return "RightArrow"_s;
    case 0xE015U:
    case 0xE05BU:
        return "DownArrow"_s;
    case 0xE016U:
    case 0xE05CU:
        return "Insert"_s;
    case 0xE017U:
    case 0xE05DU:
        return "Delete"_s;
    case 0xE018U:
        return "Semicolon"_s;
    case 0xE019U:
        return "Equals"_s;
    case 0xE01AU:
        return "NumberPad0"_s;
    case 0xE01BU:
        return "NumberPad1"_s;
    case 0xE01CU:
        return "NumberPad2"_s;
    case 0xE01DU:
        return "NumberPad3"_s;
    case 0xE01EU:
        return "NumberPad4"_s;
    case 0xE01FU:
        return "NumberPad5"_s;
    case 0xE020U:
        return "NumberPad6"_s;
    case 0xE021U:
        return "NumberPad7"_s;
    case 0xE022U:
        return "NumberPad8"_s;
    case 0xE023U:
        return "NumberPad9"_s;
    case 0xE024U:
        return "NumberPadMultiply"_s;
    case 0xE025U:
        return "NumberPadAdd"_s;
    case 0xE026U:
        return "NumberPadSeparator"_s;
    case 0xE027U:
        return "NumberPadSubtract"_s;
    case 0xE028U:
        return "NumberPadDecimal"_s;
    case 0xE029U:
        return "NumberPadDivide"_s;
    case 0xE031U:
        return "Function1"_s;
    case 0xE032U:
        return "Function2"_s;
    case 0xE033U:
        return "Function3"_s;
    case 0xE034U:
        return "Function4"_s;
    case 0xE035U:
        return "Function5"_s;
    case 0xE036U:
        return "Function6"_s;
    case 0xE037U:
        return "Function7"_s;
    case 0xE038U:
        return "Function8"_s;
    case 0xE039U:
        return "Function9"_s;
    case 0xE03AU:
        return "Function10"_s;
    case 0xE03BU:
        return "Function11"_s;
    case 0xE03CU:
        return "Function12"_s;
    case 0xE03DU:
    case 0xE053U:
        modifier = KeyModifier::Meta;
        return "Meta"_s;
    default:
        break;
    }
    return String();
}

void Session::elementSendKeys(const String& elementID, const String& text, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), elementID, text, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        elementIsFileUpload(elementID, [this, protectedThis, elementID, text, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
            if (result.isError()) {
                completionHandler(WTFMove(result));
                return;
            }

            auto fileUploadType = parseElementIsFileUploadResult(result.result());
            if (!fileUploadType || capabilities().strictFileInteractability.valueOr(false)) {
                // FIXME: move this to an atom.
                static const char focusScript[] =
                    "function focus(element) {"
                    "    let doc = element.ownerDocument || element;"
                    "    let prevActiveElement = doc.activeElement;"
                    "    if (element != prevActiveElement && prevActiveElement)"
                    "        prevActiveElement.blur();"
                    "    element.focus();"
                    "    let tagName = element.tagName.toUpperCase();"
                    "    if (tagName === 'BODY' || element === document.documentElement)"
                    "        return;"
                    "    let isTextElement = tagName === 'TEXTAREA' || (tagName === 'INPUT' && element.type === 'text');"
                    "    if (isTextElement && element.selectionEnd == 0)"
                    "        element.setSelectionRange(element.value.length, element.value.length);"
                    "    if (element != doc.activeElement)"
                    "        throw {name: 'ElementNotInteractable', message: 'Element is not focusable.'};"
                    "}";

                auto arguments = JSON::Array::create();
                arguments->pushString(createElement(elementID)->toJSONString());
                auto parameters = JSON::Object::create();
                parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
                if (m_currentBrowsingContext)
                    parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
                parameters->setString("function"_s, focusScript);
                parameters->setArray("arguments"_s, WTFMove(arguments));
                m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [this, protectedThis, fileUploadType, elementID, text, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
                    if (response.isError || !response.responseObject) {
                        completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                        return;
                    }

                    if (fileUploadType) {
                        setInputFileUploadFiles(elementID, text, fileUploadType.value() == FileUploadType::Multiple, WTFMove(completionHandler));
                        return;
                    }

                    unsigned stickyModifiers = 0;
                    auto textLength = text.length();
                    Vector<KeyboardInteraction> interactions;
                    interactions.reserveInitialCapacity(textLength);
                    for (unsigned i = 0; i < textLength; ++i) {
                        auto key = text[i];
                        KeyboardInteraction interaction;
                        KeyModifier modifier;
                        auto virtualKey = virtualKeyForKey(key, modifier);
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
                            interaction.text = String(&key, 1);
                        interactions.uncheckedAppend(WTFMove(interaction));
                    }

                    // Reset sticky modifiers if needed.
                    if (stickyModifiers) {
                        if (stickyModifiers & KeyModifier::Shift)
                            interactions.append({ KeyboardInteractionType::KeyRelease, WTF::nullopt, Optional<String>("Shift"_s) });
                        if (stickyModifiers & KeyModifier::Control)
                            interactions.append({ KeyboardInteractionType::KeyRelease, WTF::nullopt, Optional<String>("Control"_s) });
                        if (stickyModifiers & KeyModifier::Alternate)
                            interactions.append({ KeyboardInteractionType::KeyRelease, WTF::nullopt, Optional<String>("Alternate"_s) });
                        if (stickyModifiers & KeyModifier::Meta)
                            interactions.append({ KeyboardInteractionType::KeyRelease, WTF::nullopt, Optional<String>("Meta"_s) });
                    }

                    performKeyboardInteractions(WTFMove(interactions), WTFMove(completionHandler));
                });
            } else {
                setInputFileUploadFiles(elementID, text, fileUploadType.value() == FileUploadType::Multiple, WTFMove(completionHandler));
                return;
            }
        });
    });
}

void Session::getPageSource(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
        parameters->setString("function"_s, "function() { return document.documentElement.outerHTML; }"_s);
        parameters->setArray("arguments"_s, JSON::Array::create());
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

Ref<JSON::Value> Session::handleScriptResult(Ref<JSON::Value>&& resultValue)
{
    if (auto resultArray = resultValue->asArray()) {
        auto returnValueArray = JSON::Array::create();
        unsigned resultArrayLength = resultArray->length();
        for (unsigned i = 0; i < resultArrayLength; ++i)
            returnValueArray->pushValue(handleScriptResult(resultArray->get(i)));
        return returnValueArray;
    }

    if (auto element = createElement(resultValue.copyRef()))
        return element.releaseNonNull();

    if (auto resultObject = resultValue->asObject()) {
        auto returnValueObject = JSON::Object::create();
        auto end = resultObject->end();
        for (auto it = resultObject->begin(); it != end; ++it)
            returnValueObject->setValue(it->key, handleScriptResult(WTFMove(it->value)));
        return returnValueObject;
    }

    return WTFMove(resultValue);
}

void Session::executeScript(const String& script, RefPtr<JSON::Array>&& argumentsArray, ExecuteScriptMode mode, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), script, argumentsArray = WTFMove(argumentsArray), mode, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto arguments = JSON::Array::create();
        unsigned argumentsLength = argumentsArray->length();
        for (unsigned i = 0; i < argumentsLength; ++i) {
            auto argument = argumentsArray->get(i);
            if (auto element = extractElement(argument))
                arguments->pushString(element->toJSONString());
            else
                arguments->pushString(argument->toJSONString());
        }

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        parameters->setString("function"_s, "function(){\n" + script + "\n}");
        parameters->setArray("arguments"_s, WTFMove(arguments));
        if (mode == ExecuteScriptMode::Async)
            parameters->setBoolean("expectsImplicitCallbackArgument"_s, true);
        if (m_scriptTimeout != std::numeric_limits<double>::infinity())
            parameters->setDouble("callbackTimeout"_s, m_scriptTimeout);
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
            if (response.isError || !response.responseObject) {
                auto result = CommandResult::fail(WTFMove(response.responseObject));
                if (result.errorCode() == CommandResult::ErrorCode::UnexpectedAlertOpen)
                    completionHandler(CommandResult::success());
                else
                    completionHandler(WTFMove(result));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            completionHandler(CommandResult::success(handleScriptResult(resultValue.releaseNonNull())));
        });
    });
}

static String mouseButtonForAutomation(MouseButton button)
{
    switch (button) {
    case MouseButton::None:
        return "None"_s;
    case MouseButton::Left:
        return "Left"_s;
    case MouseButton::Middle:
        return "Middle"_s;
    case MouseButton::Right:
        return "Right"_s;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void Session::performMouseInteraction(int x, int y, MouseButton button, MouseInteraction interaction, Function<void (CommandResult&&)>&& completionHandler)
{
    auto parameters = JSON::Object::create();
    parameters->setString("handle"_s, m_toplevelBrowsingContext.value());
    auto position = JSON::Object::create();
    position->setInteger("x"_s, x);
    position->setInteger("y"_s, y);
    parameters->setObject("position"_s, WTFMove(position));
    parameters->setString("button"_s, mouseButtonForAutomation(button));
    switch (interaction) {
    case MouseInteraction::Move:
        parameters->setString("interaction"_s, "Move"_s);
        break;
    case MouseInteraction::Down:
        parameters->setString("interaction"_s, "Down"_s);
        break;
    case MouseInteraction::Up:
        parameters->setString("interaction"_s, "Up"_s);
        break;
    case MouseInteraction::SingleClick:
        parameters->setString("interaction"_s, "SingleClick"_s);
        break;
    case MouseInteraction::DoubleClick:
        parameters->setString("interaction"_s, "DoubleClick"_s);
        break;
    }
    parameters->setArray("modifiers"_s, JSON::Array::create());
    m_host->sendCommandToBackend("performMouseInteraction"_s, WTFMove(parameters), [protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

void Session::performKeyboardInteractions(Vector<KeyboardInteraction>&& interactions, Function<void (CommandResult&&)>&& completionHandler)
{
    auto parameters = JSON::Object::create();
    parameters->setString("handle"_s, m_toplevelBrowsingContext.value());
    auto interactionsArray = JSON::Array::create();
    for (const auto& interaction : interactions) {
        auto interactionObject = JSON::Object::create();
        switch (interaction.type) {
        case KeyboardInteractionType::KeyPress:
            interactionObject->setString("type"_s, "KeyPress"_s);
            break;
        case KeyboardInteractionType::KeyRelease:
            interactionObject->setString("type"_s, "KeyRelease"_s);
            break;
        case KeyboardInteractionType::InsertByKey:
            interactionObject->setString("type"_s, "InsertByKey"_s);
            break;
        }
        if (interaction.key)
            interactionObject->setString("key"_s, interaction.key.value());
        if (interaction.text)
            interactionObject->setString("text"_s, interaction.text.value());
        interactionsArray->pushObject(WTFMove(interactionObject));
    }
    parameters->setArray("interactions"_s, WTFMove(interactionsArray));
    m_host->sendCommandToBackend("performKeyboardInteractions"_s, WTFMove(parameters), [protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

static Optional<Session::Cookie> parseAutomationCookie(const JSON::Object& cookieObject)
{
    Session::Cookie cookie;

    cookie.name = cookieObject.getString("name"_s);
    if (!cookie.name)
        return WTF::nullopt;

    cookie.value = cookieObject.getString("value"_s);
    if (!cookie.value)
        return WTF::nullopt;

    auto path = cookieObject.getString("path"_s);
    if (!!path)
        cookie.path = path;

    auto domain = cookieObject.getString("domain"_s);
    if (!!domain)
        cookie.domain = domain;

    auto secure = cookieObject.getBoolean("secure"_s);
    if (secure)
        cookie.secure = *secure;

    auto httpOnly = cookieObject.getBoolean("httpOnly"_s);
    if (httpOnly)
        cookie.httpOnly = *httpOnly;

    auto session = cookieObject.getBoolean("session"_s);
    if (!session || !*session) {
        if (auto expiry = cookieObject.getDouble("expires"_s))
            cookie.expiry = *expiry;
    }

    return cookie;
}

static Ref<JSON::Object> builtAutomationCookie(const Session::Cookie& cookie)
{
    auto cookieObject = JSON::Object::create();
    cookieObject->setString("name"_s, cookie.name);
    cookieObject->setString("value"_s, cookie.value);
    cookieObject->setString("path"_s, cookie.path.valueOr("/"));
    cookieObject->setString("domain"_s, cookie.domain.valueOr(emptyString()));
    cookieObject->setBoolean("secure"_s, cookie.secure.valueOr(false));
    cookieObject->setBoolean("httpOnly"_s, cookie.httpOnly.valueOr(false));
    cookieObject->setBoolean("session"_s, !cookie.expiry);
    cookieObject->setDouble("expires"_s, cookie.expiry.valueOr(0));
    return cookieObject;
}

static Ref<JSON::Object> serializeCookie(const Session::Cookie& cookie)
{
    auto cookieObject = JSON::Object::create();
    cookieObject->setString("name"_s, cookie.name);
    cookieObject->setString("value"_s, cookie.value);
    if (cookie.path)
        cookieObject->setString("path"_s, cookie.path.value());
    if (cookie.domain)
        cookieObject->setString("domain"_s, cookie.domain.value());
    if (cookie.secure)
        cookieObject->setBoolean("secure"_s, cookie.secure.value());
    if (cookie.httpOnly)
        cookieObject->setBoolean("httpOnly"_s, cookie.httpOnly.value());
    if (cookie.expiry)
        cookieObject->setInteger("expiry"_s, cookie.expiry.value());
    return cookieObject;
}

void Session::getAllCookies(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
        m_host->sendCommandToBackend("getAllCookies"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto cookiesArray = response.responseObject->getArray("cookies"_s);
            if (!cookiesArray) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            auto cookies = JSON::Array::create();
            for (unsigned i = 0; i < cookiesArray->length(); ++i) {
                auto cookieObject = cookiesArray->get(i)->asObject();
                if (!cookieObject) {
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
    getAllCookies([name, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        auto cookiesArray = result.result()->asArray();
        for (unsigned i = 0; i < cookiesArray->length(); ++i) {
            auto cookieObject = cookiesArray->get(i)->asObject();
            auto cookieName = cookieObject->getString("name"_s);
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
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), cookie = builtAutomationCookie(cookie), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
        parameters->setObject("cookie"_s, WTFMove(cookie));
        m_host->sendCommandToBackend("addSingleCookie"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
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
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), name, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
        parameters->setString("cookieName"_s, name);
        m_host->sendCommandToBackend("deleteSingleCookie"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
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
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
        m_host->sendCommandToBackend("deleteAllCookies"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            completionHandler(CommandResult::success());
        });
    });
}

InputSource& Session::getOrCreateInputSource(const String& id, InputSource::Type type, Optional<PointerType> pointerType)
{
    auto addResult = m_activeInputSources.add(id, InputSource());
    if (addResult.isNewEntry)
        addResult.iterator->value = { type, pointerType };
    return addResult.iterator->value;
}

Session::InputSourceState& Session::inputSourceState(const String& id)
{
    return m_inputStateTable.ensure(id, [] { return InputSourceState(); }).iterator->value;
}

static const char* automationSourceType(InputSource::Type type)
{
    switch (type) {
    case InputSource::Type::None:
        return "Null";
    case InputSource::Type::Pointer:
        return "Mouse";
    case InputSource::Type::Key:
        return "Keyboard";
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static const char* automationOriginType(PointerOrigin::Type type)
{
    switch (type) {
    case PointerOrigin::Type::Viewport:
        return "Viewport";
    case PointerOrigin::Type::Pointer:
        return "Pointer";
    case PointerOrigin::Type::Element:
        return "Element";
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void Session::performActions(Vector<Vector<Action>>&& actionsByTick, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), actionsByTick = WTFMove(actionsByTick), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        // First check if we have actions and whether we need to resolve any pointer move element origin.
        unsigned actionsCount = 0;
        for (const auto& tick : actionsByTick)
            actionsCount += tick.size();
        if (!actionsCount) {
            completionHandler(CommandResult::success());
            return;
        }

        auto parameters = JSON::Object::create();
        parameters->setString("handle"_s, m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        auto inputSources = JSON::Array::create();
        for (const auto& inputSource : m_activeInputSources) {
            auto inputSourceObject = JSON::Object::create();
            inputSourceObject->setString("sourceId"_s, inputSource.key);
            inputSourceObject->setString("sourceType"_s, automationSourceType(inputSource.value.type));
            inputSources->pushObject(WTFMove(inputSourceObject));
        }
        parameters->setArray("inputSources"_s, WTFMove(inputSources));
        auto steps = JSON::Array::create();
        for (const auto& tick : actionsByTick) {
            auto states = JSON::Array::create();
            for (const auto& action : tick) {
                auto state = JSON::Object::create();
                auto& currentState = inputSourceState(action.id);
                state->setString("sourceId"_s, action.id);
                switch (action.type) {
                case Action::Type::None:
                    if (action.duration)
                        state->setDouble("duration"_s, action.duration.value());
                    break;
                case Action::Type::Pointer: {
                    switch (action.subtype) {
                    case Action::Subtype::PointerUp:
                        currentState.pressedButton = WTF::nullopt;
                        break;
                    case Action::Subtype::PointerDown:
                        currentState.pressedButton = action.button.value();
                        break;
                    case Action::Subtype::PointerMove: {
                        state->setString("origin"_s, automationOriginType(action.origin->type));
                        auto location = JSON::Object::create();
                        location->setInteger("x"_s, action.x.value());
                        location->setInteger("y"_s, action.y.value());
                        state->setObject("location"_s, WTFMove(location));
                        if (action.origin->type == PointerOrigin::Type::Element)
                            state->setString("nodeHandle"_s, action.origin->elementID.value());
                        FALLTHROUGH;
                    }
                    case Action::Subtype::Pause:
                        if (action.duration)
                            state->setDouble("duration"_s, action.duration.value());
                        break;
                    case Action::Subtype::PointerCancel:
                        currentState.pressedButton = WTF::nullopt;
                        break;
                    case Action::Subtype::KeyUp:
                    case Action::Subtype::KeyDown:
                        ASSERT_NOT_REACHED();
                    }
                    if (currentState.pressedButton)
                        state->setString("pressedButton"_s, mouseButtonForAutomation(currentState.pressedButton.value()));
                    break;
                }
                case Action::Type::Key:
                    switch (action.subtype) {
                    case Action::Subtype::KeyUp: {
                        KeyModifier modifier;
                        auto virtualKey = virtualKeyForKey(action.key.value()[0], modifier);
                        if (!virtualKey.isNull())
                            currentState.pressedVirtualKeys.remove(virtualKey);
                        else
                            currentState.pressedKey = WTF::nullopt;
                        break;
                    }
                    case Action::Subtype::KeyDown: {
                        KeyModifier modifier;
                        auto virtualKey = virtualKeyForKey(action.key.value()[0], modifier);
                        if (!virtualKey.isNull())
                            currentState.pressedVirtualKeys.add(virtualKey);
                        else
                            currentState.pressedKey = action.key.value();
                        break;
                    }
                    case Action::Subtype::Pause:
                        if (action.duration)
                            state->setDouble("duration"_s, action.duration.value());
                        break;
                    case Action::Subtype::PointerUp:
                    case Action::Subtype::PointerDown:
                    case Action::Subtype::PointerMove:
                    case Action::Subtype::PointerCancel:
                        ASSERT_NOT_REACHED();
                    }
                    if (currentState.pressedKey)
                        state->setString("pressedCharKey"_s, currentState.pressedKey.value());
                    if (!currentState.pressedVirtualKeys.isEmpty()) {
                        // FIXME: support parsing and tracking multiple virtual keys.
                        Ref<JSON::Array> virtualKeys = JSON::Array::create();
                        for (const auto& virtualKey : currentState.pressedVirtualKeys)
                            virtualKeys->pushString(virtualKey);
                        state->setArray("pressedVirtualKeys"_s, WTFMove(virtualKeys));
                    }
                    break;
                }
                states->pushObject(WTFMove(state));
            }
            auto stepStates = JSON::Object::create();
            stepStates->setArray("states"_s, WTFMove(states));
            steps->pushObject(WTFMove(stepStates));
        }

        parameters->setArray("steps"_s, WTFMove(steps));
        m_host->sendCommandToBackend("performInteractionSequence"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)] (SessionHost::CommandResponse&& response) {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            completionHandler(CommandResult::success());
        });
    });
}

void Session::releaseActions(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    m_activeInputSources.clear();
    m_inputStateTable.clear();

    auto parameters = JSON::Object::create();
    parameters->setString("handle"_s, m_toplevelBrowsingContext.value());
    m_host->sendCommandToBackend("cancelInteractionSequence"_s, WTFMove(parameters), [protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

void Session::dismissAlert(Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
    m_host->sendCommandToBackend("dismissCurrentJavaScriptDialog"_s, WTFMove(parameters), [protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
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

    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
    m_host->sendCommandToBackend("acceptCurrentJavaScriptDialog"_s, WTFMove(parameters), [protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
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

    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
    m_host->sendCommandToBackend("messageOfCurrentJavaScriptDialog"_s, WTFMove(parameters), [protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError || !response.responseObject) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }

        auto valueString = response.responseObject->getString("message"_s);
        if (!valueString) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
            return;
        }

        completionHandler(CommandResult::success(JSON::Value::create(valueString)));
    });
}

void Session::sendAlertText(const String& text, Function<void (CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, m_toplevelBrowsingContext.value());
    parameters->setString("userInput"_s, text);
    m_host->sendCommandToBackend("setUserInputForCurrentJavaScriptPrompt"_s, WTFMove(parameters), [protectedThis = makeRef(*this), completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

void Session::takeScreenshot(Optional<String> elementID, Optional<bool> scrollIntoView, Function<void (CommandResult&&)>&& completionHandler)
{
    if ((elementID && !m_currentBrowsingContext) || (!elementID && !m_toplevelBrowsingContext)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = makeRef(*this), elementID, scrollIntoView, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("handle"_s, m_toplevelBrowsingContext.value());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        if (elementID)
            parameters->setString("nodeHandle"_s, elementID.value());
        parameters->setBoolean("clipToViewport"_s, true);
        if (scrollIntoView.valueOr(false))
            parameters->setBoolean("scrollIntoViewIfNeeded"_s, true);
        m_host->sendCommandToBackend("takeScreenshot"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto data = response.responseObject->getString("data"_s);
            if (!data) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError));
                return;
            }

            completionHandler(CommandResult::success(JSON::Value::create(data)));
        });
    });
}

} // namespace WebDriver
