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
#include "Logging.h"
#include "SessionHost.h"
#include "WebDriverAtoms.h"
#include <optional>
#include <wtf/ASCIICType.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/FileSystem.h>
#include <wtf/HashSet.h>
#include <wtf/HexNumber.h>
#include <wtf/JSONValues.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/UUID.h>
#include <wtf/text/MakeString.h>

#if ENABLE(WEBDRIVER_BIDI)
#include "WebSocketServer.h"
#include <cstdint>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>
#endif

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

const String& Session::shadowRootIdentifier()
{
    // The shadow root identifier is constant defined by the spec.
    static NeverDestroyed<String> shadowRootID { "shadow-6066-11e4-a52e-4f735466cecf"_s };
    return shadowRootID;
}

Session::Session(Ref<SessionHost>&& host)
    : m_host(WTFMove(host))
    , m_scriptTimeout(defaultScriptTimeout)
    , m_pageLoadTimeout(defaultPageLoadTimeout)
    , m_implicitWaitTimeout(defaultImplicitWaitTimeout)
{
    auto protectedCapabilities = capabilities();
    if (protectedCapabilities.timeouts)
        setTimeouts(protectedCapabilities.timeouts.value(), [](CommandResult&&) { });
}

#if ENABLE(WEBDRIVER_BIDI)
Session::Session(Ref<SessionHost>&& host, WeakPtr<WebSocketServer>&& bidiServer)
    : Session(WTFMove(host))
{
    m_bidiServer = WTFMove(bidiServer);
    m_host->setBidiHandler(this);
}
#endif

Session::~Session()
{
#if ENABLE(WEBDRIVER_BIDI)
    m_bidiServer->removeResourceForSession(id());
#endif
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

static std::optional<String> firstWindowHandleInResult(JSON::Value& result)
{
    auto handles = result.asArray();
    if (handles && handles->length()) {
        auto handle = handles->get(0)->asString();
        if (!!handle)
            return handle;
    }
    return std::nullopt;
}

void Session::closeAllToplevelBrowsingContexts(const String& toplevelBrowsingContext, Function<void(CommandResult&&)>&& completionHandler)
{
    closeTopLevelBrowsingContext(toplevelBrowsingContext, [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
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

void Session::close(Function<void(CommandResult&&)>&& completionHandler)
{
    m_toplevelBrowsingContext = std::nullopt;
    m_currentBrowsingContext = std::nullopt;
    m_currentParentBrowsingContext = std::nullopt;
    getWindowHandles([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        // We shouldn't close the windows if we connected to an already running browser, leaving this decision
        // to the client script. For example, the client script might want to keep the browser open for debugging
        if (!m_host->isRemoteBrowser()) {
            if (auto handle = firstWindowHandleInResult(*result.result())) {
                closeAllToplevelBrowsingContexts(handle.value(), WTFMove(completionHandler));
                return;
            }
        }

        if (m_host->isConnected()) {
            m_host->sendCommandToBackend("deleteSession"_s, nullptr, [completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
                if (response.isError) {
                    completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                    return;
                }
                completionHandler(CommandResult::success());
            });
            return;
        }

        completionHandler(CommandResult::success());
    });
}

void Session::getTimeouts(Function<void(CommandResult&&)>&& completionHandler)
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

void Session::setTimeouts(const Timeouts& timeouts, Function<void(CommandResult&&)>&& completionHandler)
{
    if (timeouts.script)
        m_scriptTimeout = timeouts.script.value();
    if (timeouts.pageLoad)
        m_pageLoadTimeout = timeouts.pageLoad.value();
    if (timeouts.implicit)
        m_implicitWaitTimeout = timeouts.implicit.value();
    completionHandler(CommandResult::success());
}

String Session::uncheckedTopLevelBrowsingContext() const
{
    ASSERT(m_toplevelBrowsingContext);
    return m_toplevelBrowsingContext.value();
}

void Session::switchToTopLevelBrowsingContext(const String& toplevelBrowsingContext)
{
    m_toplevelBrowsingContext = toplevelBrowsingContext;
    m_currentBrowsingContext = String();
    m_currentParentBrowsingContext = String();
}

void Session::switchToBrowsingContext(const String& browsingContext, Function<void(CommandResult&&)>&& completionHandler)
{
    m_currentBrowsingContext = browsingContext;
    if (browsingContext.isEmpty()) {
        m_currentParentBrowsingContext = String();
        completionHandler(CommandResult::success());
        return;
    }

    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
    parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
    m_host->sendCommandToBackend("resolveParentFrameHandle"_s, WTFMove(parameters), [this, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (!response.isError && response.responseObject)
            m_currentParentBrowsingContext = response.responseObject->getString("result"_s);
        completionHandler(CommandResult::success());
    });
}

std::optional<String> Session::pageLoadStrategyString() const
{
    auto protectedCapabilities = capabilities();
    if (!protectedCapabilities.pageLoadStrategy)
        return std::nullopt;

    switch (protectedCapabilities.pageLoadStrategy.value()) {
    case PageLoadStrategy::None:
        return String("None"_s);
    case PageLoadStrategy::Normal:
        return String("Normal"_s);
    case PageLoadStrategy::Eager:
        return String("Eager"_s);
    }

    return std::nullopt;
}

void Session::createTopLevelBrowsingContext(Function<void(CommandResult&&)>&& completionHandler)
{
    ASSERT(!m_toplevelBrowsingContext);
    m_host->sendCommandToBackend("createBrowsingContext"_s, nullptr, [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
        if (response.isError || !response.responseObject) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }

        auto handle = response.responseObject->getString("handle"_s);
        if (!handle) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve handle for the new browsing context"_s));
            return;
        }

        switchToTopLevelBrowsingContext(handle);
        completionHandler(CommandResult::success());
    });
}

void Session::handleUserPrompts(Function<void(CommandResult&&)>&& completionHandler)
{
    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
    m_host->sendCommandToBackend("isShowingJavaScriptDialog"_s, WTFMove(parameters), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
        if (response.isError || !response.responseObject) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }

        auto isShowingJavaScriptDialog = response.responseObject->getBoolean("result"_s);
        if (!isShowingJavaScriptDialog) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not determine if a JavaScript dialog is showing"_s));
            return;
        }

        if (!isShowingJavaScriptDialog.value()) {
            completionHandler(CommandResult::success());
            return;
        }

        handleUnexpectedAlertOpen(WTFMove(completionHandler));
    });
}

void Session::handleUnexpectedAlertOpen(Function<void(CommandResult&&)>&& completionHandler)
{
    switch (capabilities().unhandledPromptBehavior.value_or(UnhandledPromptBehavior::DismissAndNotify)) {
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

void Session::dismissAndNotifyAlert(Function<void(CommandResult&&)>&& completionHandler)
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

void Session::acceptAndNotifyAlert(Function<void(CommandResult&&)>&& completionHandler)
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

void Session::reportUnexpectedAlertOpen(Function<void(CommandResult&&)>&& completionHandler)
{
    getAlertText([completionHandler = WTFMove(completionHandler)](CommandResult&& result) {
        std::optional<String> alertText;
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

void Session::go(const String& url, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, url, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        auto parameters = JSON::Object::create();
        parameters->setString("handle"_s, uncheckedTopLevelBrowsingContext());
        parameters->setString("url"_s, url);
        parameters->setDouble("pageLoadTimeout"_s, m_pageLoadTimeout);
        if (auto pageLoadStrategy = pageLoadStrategyString())
            parameters->setString("pageLoadStrategy"_s, pageLoadStrategy.value());
        m_host->sendCommandToBackend("navigateBrowsingContext"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            switchToBrowsingContext({ }, WTFMove(completionHandler));
        });
    });
}

void Session::getCurrentURL(Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        auto parameters = JSON::Object::create();
        parameters->setString("handle"_s, uncheckedTopLevelBrowsingContext());
        m_host->sendCommandToBackend("getBrowsingContext"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto browsingContext = response.responseObject->getObject("context"_s);
            if (!browsingContext) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve browsing context information"_s));
                return;
            }

            auto url = browsingContext->getString("url"_s);
            if (!url) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve URL from browsing context"_s));
                return;
            }

            completionHandler(CommandResult::success(JSON::Value::create(url)));
        });
    });
}

void Session::back(Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("handle"_s, uncheckedTopLevelBrowsingContext());
        parameters->setDouble("pageLoadTimeout"_s, m_pageLoadTimeout);
        if (auto pageLoadStrategy = pageLoadStrategyString())
            parameters->setString("pageLoadStrategy"_s, pageLoadStrategy.value());
        m_host->sendCommandToBackend("goBackInBrowsingContext"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            switchToBrowsingContext({ }, WTFMove(completionHandler));
        });
    });
}

void Session::forward(Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("handle"_s, uncheckedTopLevelBrowsingContext());
        parameters->setDouble("pageLoadTimeout"_s, m_pageLoadTimeout);
        if (auto pageLoadStrategy = pageLoadStrategyString())
            parameters->setString("pageLoadStrategy"_s, pageLoadStrategy.value());
        m_host->sendCommandToBackend("goForwardInBrowsingContext"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            switchToBrowsingContext({ }, WTFMove(completionHandler));
        });
    });
}

void Session::refresh(Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("handle"_s, uncheckedTopLevelBrowsingContext());
        parameters->setDouble("pageLoadTimeout"_s, m_pageLoadTimeout);
        if (auto pageLoadStrategy = pageLoadStrategyString())
            parameters->setString("pageLoadStrategy"_s, pageLoadStrategy.value());
        m_host->sendCommandToBackend("reloadBrowsingContext"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            switchToBrowsingContext({ }, WTFMove(completionHandler));
        });
    });
}

void Session::getTitle(Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
        parameters->setString("function"_s, "function() { return document.title; }"_s);
        parameters->setArray("arguments"_s, JSON::Array::create());
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve title from browsing context"_s));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not parse title from browsing context"_s));
                return;
            }

            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

void Session::getWindowHandle(Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    auto parameters = JSON::Object::create();
    parameters->setString("handle"_s, uncheckedTopLevelBrowsingContext());
    m_host->sendCommandToBackend("getBrowsingContext"_s, WTFMove(parameters), [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError || !response.responseObject) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }

        auto browsingContext = response.responseObject->getObject("context"_s);
        if (!browsingContext) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve browsing context information"_s));
            return;
        }

        auto handle = browsingContext->getString("handle"_s);
        if (!handle) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve browsing context handle"_s));
            return;
        }

        completionHandler(CommandResult::success(JSON::Value::create(handle)));
    });
}

void Session::closeTopLevelBrowsingContext(const String& toplevelBrowsingContext, Function<void(CommandResult&&)>&& completionHandler)
{
    auto parameters = JSON::Object::create();
    parameters->setString("handle"_s, toplevelBrowsingContext);
    m_host->sendCommandToBackend("closeBrowsingContext"_s, WTFMove(parameters), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
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

void Session::closeWindow(Function<void(CommandResult&&)>&& completionHandler)
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

        if (!m_toplevelBrowsingContext) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
            return;
        }
        auto toplevelBrowsingContext = std::exchange(m_toplevelBrowsingContext, std::nullopt);
        m_currentBrowsingContext = std::nullopt;
        m_currentParentBrowsingContext = std::nullopt;
        closeTopLevelBrowsingContext(toplevelBrowsingContext.value(), WTFMove(completionHandler));
    });
}

void Session::switchToBrowsingContext(const String& toplevelBrowsingContext, const String& browsingContext, Function<void(CommandResult&&)>&& completionHandler)
{
    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, toplevelBrowsingContext);
    parameters->setString("frameHandle"_s, browsingContext);
    m_host->sendCommandToBackend("switchToBrowsingContext"_s, WTFMove(parameters), [completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

void Session::switchToWindow(const String& windowHandle, Function<void(CommandResult&&)>&& completionHandler)
{
    switchToBrowsingContext(windowHandle, { }, [this, protectedThis = Ref { *this }, windowHandle, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        switchToTopLevelBrowsingContext(windowHandle);
        completionHandler(CommandResult::success());
    });
}

void Session::getWindowHandles(Function<void(CommandResult&&)>&& completionHandler)
{
    m_host->sendCommandToBackend("getBrowsingContexts"_s, JSON::Object::create(), [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError || !response.responseObject) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }

        auto browsingContextArray = response.responseObject->getArray("contexts"_s);
        if (!browsingContextArray) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve browsing contexts"_s));
            return;
        }

        auto windowHandles = JSON::Array::create();
        for (unsigned i = 0; i < browsingContextArray->length(); ++i) {
            auto browsingContext = browsingContextArray->get(i)->asObject();
            if (!browsingContext) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Invalid browsing context information returned"_s));
                return;
            }

            auto handle = browsingContext->getString("handle"_s);
            if (!handle) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "No handle returned for browsing context"_s));
                return;
            }

            windowHandles->pushString(handle);
        }
        completionHandler(CommandResult::success(WTFMove(windowHandles)));
    });
}

void Session::newWindow(std::optional<String> typeHint, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, typeHint, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        RefPtr<JSON::Object> parameters;
        if (typeHint) {
            parameters = JSON::Object::create();
            parameters->setString("presentationHint"_s, typeHint.value() == "window"_s ? "Window"_s : "Tab"_s);
        }
        m_host->sendCommandToBackend("createBrowsingContext"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto handle = response.responseObject->getString("handle"_s);
            if (!handle) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "No handle returned for the new browsing context"_s));
                return;
            }

            auto presentation = response.responseObject->getString("presentation"_s);
            if (!presentation) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "No presentation returned for the new browsing context"_s));
                return;
            }

            auto result = JSON::Object::create();
            result->setString("handle"_s, handle);
            result->setString("type"_s, presentation == "Window"_s ? "window"_s : "tab"_s);
            completionHandler(CommandResult::success(WTFMove(result)));
        });
    });
}

void Session::switchToFrame(RefPtr<JSON::Value>&& frameID, Function<void(CommandResult&&)>&& completionHandler)
{
    if (frameID->isNull()) {
        if (!m_toplevelBrowsingContext) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
            return;
        }
        switchToBrowsingContext({ }, WTFMove(completionHandler));
        return;
    }

    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, frameID = WTFMove(frameID), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());

        if (auto frameIndex = frameID->asInteger()) {
            if (*frameIndex < 0 || *frameIndex > std::numeric_limits<unsigned short>::max()) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
                return;
            }
            parameters->setInteger("ordinal"_s, *frameIndex);
        } else {
            String frameElementID = extractElementID(*frameID);
            ASSERT(!frameElementID.isEmpty());
            parameters->setString("nodeHandle"_s, frameElementID);
        }

        m_host->sendCommandToBackend("resolveChildFrameHandle"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto frameHandle = response.responseObject->getString("result"_s);
            if (!frameHandle) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not resolve child frame handle"_s));
                return;
            }

            switchToBrowsingContext(uncheckedTopLevelBrowsingContext(), frameHandle, [this, protectedThis, frameHandle, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
                if (result.isError()) {
                    completionHandler(WTFMove(result));
                    return;
                }
                switchToBrowsingContext(frameHandle, WTFMove(completionHandler));
            });
        });
    });
}

void Session::switchToParentFrame(Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentParentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        switchToBrowsingContext(uncheckedTopLevelBrowsingContext(), m_currentParentBrowsingContext.value(), [this, protectedThis, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
            if (result.isError()) {
                if (result.errorCode() == CommandResult::ErrorCode::NoSuchFrame)
                    completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
                else
                    completionHandler(WTFMove(result));
                return;
            }

            switchToBrowsingContext(m_currentParentBrowsingContext.value(), WTFMove(completionHandler));
        });
    });
}

void Session::getToplevelBrowsingContextRect(Function<void(CommandResult&&)>&& completionHandler)
{
    auto parameters = JSON::Object::create();
    parameters->setString("handle"_s, uncheckedTopLevelBrowsingContext());
    m_host->sendCommandToBackend("getBrowsingContext"_s, WTFMove(parameters), [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError || !response.responseObject) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }

        auto browsingContext = response.responseObject->getObject("context"_s);
        if (!browsingContext) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve browsing context information"_s));
            return;
        }

        auto windowOrigin = browsingContext->getObject("windowOrigin"_s);
        if (!windowOrigin) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve window origin from browsing context"_s));
            return;
        }

        auto x = windowOrigin->getDouble("x"_s);
        if (!x) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve x coordinate from window origin"_s));
            return;
        }

        auto y = windowOrigin->getDouble("y"_s);
        if (!y) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve y coordinate from window origin"_s));
            return;
        }

        auto windowSize = browsingContext->getObject("windowSize"_s);
        if (!windowSize) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve window size from browsing context"_s));
            return;
        }

        auto width = windowSize->getDouble("width"_s);
        if (!width) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve width from window size"_s));
            return;
        }

        auto height = windowSize->getDouble("height"_s);
        if (!height) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve height from window size"_s));
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

void Session::getWindowRect(Function<void(CommandResult&&)>&& completionHandler)
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

void Session::setWindowRect(std::optional<double> x, std::optional<double> y, std::optional<double> width, std::optional<double> height, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, x, y, width, height, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        auto parameters = JSON::Object::create();
        parameters->setString("handle"_s, uncheckedTopLevelBrowsingContext());
        if (x && y) {
            auto windowOrigin = JSON::Object::create();
            windowOrigin->setDouble("x"_s, x.value());
            windowOrigin->setDouble("y"_s, y.value());
            parameters->setObject("origin"_s, WTFMove(windowOrigin));
        }
        if (width && height) {
            auto windowSize = JSON::Object::create();
            windowSize->setDouble("width"_s, width.value());
            windowSize->setDouble("height"_s, height.value());
            parameters->setObject("size"_s, WTFMove(windowSize));
        }
        m_host->sendCommandToBackend("setWindowFrameOfBrowsingContext"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            getToplevelBrowsingContextRect(WTFMove(completionHandler));
        });
    });
}

void Session::maximizeWindow(Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        auto parameters = JSON::Object::create();
        parameters->setString("handle"_s, uncheckedTopLevelBrowsingContext());
        m_host->sendCommandToBackend("maximizeWindowOfBrowsingContext"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            getToplevelBrowsingContextRect(WTFMove(completionHandler));
        });
    });
}

void Session::minimizeWindow(Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        auto parameters = JSON::Object::create();
        parameters->setString("handle"_s, uncheckedTopLevelBrowsingContext());
        m_host->sendCommandToBackend("hideWindowOfBrowsingContext"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            getToplevelBrowsingContextRect(WTFMove(completionHandler));
        });
    });
}

void Session::fullscreenWindow(Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
        parameters->setString("function"_s, StringImpl::createWithoutCopying(EnterFullscreenJavaScript));
        parameters->setArray("arguments"_s, JSON::Array::create());
        parameters->setBoolean("expectsImplicitCallbackArgument"_s, true);
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve fullscreen information from browsing context"_s));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not parse fullscreen information"_s));
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

    auto elementID = valueObject->getString(makeString("session-node-"_s, id()));
    if (!elementID)
        return nullptr;

    auto elementObject = JSON::Object::create();
    elementObject->setString(webElementIdentifier(), elementID);
    return elementObject;
}

Ref<JSON::Object> Session::createElement(const String& elementID)
{
    auto elementObject = JSON::Object::create();
    elementObject->setString(makeString("session-node-"_s, id()), elementID);
    return elementObject;
}

RefPtr<JSON::Object> Session::createShadowRoot(RefPtr<JSON::Value>&& value)
{
    if (!value)
        return nullptr;

    auto valueObject = value->asObject();
    if (!valueObject)
        return nullptr;

    auto elementID = valueObject->getString(makeString("session-node-"_s, id()));
    if (!elementID)
        return nullptr;

    auto elementObject = JSON::Object::create();
    elementObject->setString(shadowRootIdentifier(), elementID);
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

void Session::computeElementLayout(const String& elementID, OptionSet<ElementLayoutOption> options, Function<void(std::optional<Rect>&&, std::optional<Point>&&, bool, RefPtr<JSON::Object>&&)>&& completionHandler)
{

    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
    parameters->setString("frameHandle"_s, m_currentBrowsingContext.value_or(emptyString()));
    parameters->setString("nodeHandle"_s, elementID);
    parameters->setBoolean("scrollIntoViewIfNeeded"_s, options.contains(ElementLayoutOption::ScrollIntoViewIfNeeded));
    parameters->setString("coordinateSystem"_s, options.contains(ElementLayoutOption::UseViewportCoordinates) ? "LayoutViewport"_s : "Page"_s);
    m_host->sendCommandToBackend("computeElementLayout"_s, WTFMove(parameters), [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
        if (response.isError || !response.responseObject) {
            completionHandler(std::nullopt, std::nullopt, false, WTFMove(response.responseObject));
            return;
        }

        auto rectObject = response.responseObject->getObject("rect"_s);
        if (!rectObject) {
            completionHandler(std::nullopt, std::nullopt, false, nullptr);
            return;
        }

        std::optional<int> elementX;
        std::optional<int> elementY;
        auto elementPosition = rectObject->getObject("origin"_s);
        if (elementPosition) {
            elementX = elementPosition->getInteger("x"_s);
            elementY = elementPosition->getInteger("y"_s);
        }
        if (!elementX || !elementY) {
            completionHandler(std::nullopt, std::nullopt, false, nullptr);
            return;
        }

        std::optional<int> elementWidth;
        std::optional<int> elementHeight;
        auto elementSize = rectObject->getObject("size"_s);
        if (elementSize) {
            elementWidth = elementSize->getInteger("width"_s);
            elementHeight = elementSize->getInteger("height"_s);
        }
        if (!elementWidth || !elementHeight) {
            completionHandler(std::nullopt, std::nullopt, false, nullptr);
            return;
        }

        Rect rect = { { elementX.value(), elementY.value() }, { elementWidth.value(), elementHeight.value() } };

        auto isObscured = response.responseObject->getBoolean("isObscured"_s);
        if (!isObscured) {
            completionHandler(std::nullopt, std::nullopt, false, nullptr);
            return;
        }

        auto inViewCenterPointObject = response.responseObject->getObject("inViewCenterPoint"_s);
        if (!inViewCenterPointObject) {
            completionHandler(rect, std::nullopt, *isObscured, nullptr);
            return;
        }

        auto inViewCenterPointX = inViewCenterPointObject->getInteger("x"_s);
        auto inViewCenterPointY = inViewCenterPointObject->getInteger("y"_s);
        if (!inViewCenterPointX || !inViewCenterPointY) {
            completionHandler(std::nullopt, std::nullopt, *isObscured, nullptr);
            return;
        }

        Point inViewCenterPoint = { *inViewCenterPointX, *inViewCenterPointY };
        completionHandler(rect, inViewCenterPoint, *isObscured, nullptr);
    });
}

void Session::findElements(const String& strategy, const String& selector, FindElementsMode mode, const String& rootElementID, ElementIsShadowRoot isShadowRoot, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, strategy, selector, mode, rootElementID, isShadowRoot, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
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
        parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        parameters->setString("function"_s, StringImpl::createWithoutCopying(FindNodesJavaScript));
        parameters->setArray("arguments"_s, WTFMove(arguments));
        parameters->setBoolean("expectsImplicitCallbackArgument"_s, true);
        // If there's an implicit wait, use one second more as callback timeout.
        if (m_implicitWaitTimeout)
            parameters->setDouble("callbackTimeout"_s, m_implicitWaitTimeout + 1000);

        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [this, protectedThis, mode, isShadowRoot, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                auto result = CommandResult::fail(WTFMove(response.responseObject));
                if (isShadowRoot == ElementIsShadowRoot::Yes && result.errorCode() == CommandResult::ErrorCode::StaleElementReference) {
                    completionHandler(CommandResult::fail(CommandResult::ErrorCode::DetachedShadowRoot));
                    return;
                }
                completionHandler(WTFMove(result));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve element info from browsing context"_s));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not parse element info"_s));
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

void Session::getActiveElement(Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
        parameters->setString("function"_s, "function() { return document.activeElement; }"_s);
        parameters->setArray("arguments"_s, JSON::Array::create());
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve active element information from browsing context"_s));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not parse active element information"_s));
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

void Session::getElementShadowRoot(const String& elementID, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        auto arguments = JSON::Array::create();
        arguments->pushString(createElement(elementID)->toJSONString());

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        parameters->setString("function"_s, "function(element) { return element.shadowRoot; }"_s);
        parameters->setArray("arguments"_s, WTFMove(arguments));
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [this, protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve shadow root information from browsing context"_s));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not parse shadow root information"_s));
                return;
            }

            auto elementObject = createShadowRoot(WTFMove(resultValue));
            if (!elementObject) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchShadowRoot));
                return;
            }
            completionHandler(CommandResult::success(WTFMove(elementObject)));
        });
    });
}

void Session::isElementSelected(const String& elementID, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto arguments = JSON::Array::create();
        arguments->pushString(createElement(elementID)->toJSONString());
        arguments->pushString(JSON::Value::create(makeString("selected"_s))->toJSONString());

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        parameters->setString("function"_s, StringImpl::createWithoutCopying(ElementAttributeJavaScript));
        parameters->setArray("arguments"_s, WTFMove(arguments));
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve element selection state from browsing context"_s));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not parse element selection state"_s));
                return;
            }

            if (resultValue->isNull()) {
                completionHandler(CommandResult::success(JSON::Value::create(false)));
                return;
            }

            auto booleanResult = resultValue->asString();
            if (!booleanResult || booleanResult != "true"_s) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Element selection state is not a boolean"_s));
                return;
            }

            completionHandler(CommandResult::success(JSON::Value::create(true)));
        });
    });
}

void Session::getElementText(const String& elementID, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto arguments = JSON::Array::create();
        arguments->pushString(createElement(elementID)->toJSONString());

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        // FIXME: Add an atom to properly implement this instead of just using innerText.
        parameters->setString("function"_s, StringImpl::createWithoutCopying(ElementTextJavaScript));
        parameters->setArray("arguments"_s, WTFMove(arguments));
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve element text from browsing context"_s));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not parse element text"_s));
                return;
            }

            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

void Session::getElementTagName(const String& elementID, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto arguments = JSON::Array::create();
        arguments->pushString(createElement(elementID)->toJSONString());

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
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
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve element tag name from browsing context"_s));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not parse element tag name"_s));
                return;
            }

            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

void Session::getElementRect(const String& elementID, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        computeElementLayout(elementID, { }, [protectedThis, completionHandler = WTFMove(completionHandler)](std::optional<Rect>&& rect, std::optional<Point>&&, bool, RefPtr<JSON::Object>&& error) {
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

void Session::isElementEnabled(const String& elementID, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto arguments = JSON::Array::create();
        arguments->pushString(createElement(elementID)->toJSONString());

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        parameters->setString("function"_s, StringImpl::createWithoutCopying(ElementEnabledJavaScript));
        parameters->setArray("arguments"_s, WTFMove(arguments));
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve element enabled state from browsing context"_s));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not parse element enabled state"_s));
                return;
            }

            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

void Session::getComputedRole(const String& elementID, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
        parameters->setString("frameHandle"_s, m_currentBrowsingContext.value_or(emptyString()));
        parameters->setString("nodeHandle"_s, elementID);
        m_host->sendCommandToBackend("getComputedRole"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("role"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve computed role from browsing context"_s));
                return;
            }

            auto resultValue = JSON::Value::create(valueString);
            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

void Session::getComputedLabel(const String& elementID, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
        parameters->setString("frameHandle"_s, m_currentBrowsingContext.value_or(emptyString()));
        parameters->setString("nodeHandle"_s, elementID);
        m_host->sendCommandToBackend("getComputedLabel"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("label"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve computed label from browsing context"_s));
                return;
            }

            auto resultValue = JSON::Value::create(valueString);
            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

void Session::isElementDisplayed(const String& elementID, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto arguments = JSON::Array::create();
        arguments->pushString(createElement(elementID)->toJSONString());

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        parameters->setString("function"_s, StringImpl::createWithoutCopying(ElementDisplayedJavaScript));
        parameters->setArray("arguments"_s, WTFMove(arguments));
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve element display state from browsing context"_s));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not parse element display state"_s));
                return;
            }

            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

void Session::getElementAttribute(const String& elementID, const String& attribute, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, elementID, attribute, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto arguments = JSON::Array::create();
        arguments->pushString(createElement(elementID)->toJSONString());
        arguments->pushString(JSON::Value::create(attribute)->toJSONString());

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        parameters->setString("function"_s, StringImpl::createWithoutCopying(ElementAttributeJavaScript));
        parameters->setArray("arguments"_s, WTFMove(arguments));
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve element attribute from browsing context"_s));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not parse element attribute"_s));
                return;
            }

            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

void Session::getElementProperty(const String& elementID, const String& property, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, elementID, property, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto arguments = JSON::Array::create();
        arguments->pushString(createElement(elementID)->toJSONString());

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        parameters->setString("function"_s, makeString("function(element) { return element."_s, property, "; }"_s));
        parameters->setArray("arguments"_s, WTFMove(arguments));
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve element property from browsing context"_s));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not parse element property"_s));
                return;
            }

            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

void Session::getElementCSSValue(const String& elementID, const String& cssProperty, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, elementID, cssProperty, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto arguments = JSON::Array::create();
        arguments->pushString(createElement(elementID)->toJSONString());

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        parameters->setString("function"_s, makeString("function(element) { return document.defaultView.getComputedStyle(element).getPropertyValue('"_s, cssProperty, "'); }"_s));
        parameters->setArray("arguments"_s, WTFMove(arguments));
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve element CSS value from browsing context"_s));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not parse element CSS value"_s));
                return;
            }

            completionHandler(CommandResult::success(WTFMove(resultValue)));
        });
    });
}

void Session::waitForNavigationToComplete(Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::success());
        return;
    }

    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
    if (m_currentBrowsingContext)
        parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
    parameters->setDouble("pageLoadTimeout"_s, m_pageLoadTimeout);
    if (auto pageLoadStrategy = pageLoadStrategyString())
        parameters->setString("pageLoadStrategy"_s, pageLoadStrategy.value());
    m_host->sendCommandToBackend("waitForNavigationToComplete"_s, WTFMove(parameters), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            auto result = CommandResult::fail(WTFMove(response.responseObject));
            switch (result.errorCode()) {
            case CommandResult::ErrorCode::NoSuchWindow:
                // Window was closed, reset the top level browsing context and ignore the error.
                m_toplevelBrowsingContext = std::nullopt;
                m_currentBrowsingContext = std::nullopt;
                m_currentParentBrowsingContext = std::nullopt;
                break;
            case CommandResult::ErrorCode::NoSuchFrame:
                // Navigation destroyed the current frame, reset the current browsing context and ignore the error.
                m_currentBrowsingContext = std::nullopt;
                break;
            default:
                completionHandler(WTFMove(result));
                return;
            }
        }
        completionHandler(CommandResult::success());
    });
}

void Session::elementIsFileUpload(const String& elementID, Function<void(CommandResult&&)>&& completionHandler)
{
    auto arguments = JSON::Array::create();
    arguments->pushString(createElement(elementID)->toJSONString());

    static constexpr auto isFileUploadScript =
        "function(element) {"
        "    if (element.tagName.toLowerCase() === 'input' && element.type === 'file')"
        "        return { 'fileUpload': true, 'multiple': element.hasAttribute('multiple') };"
        "    return { 'fileUpload': false };"
        "}"_s;

    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
    if (m_currentBrowsingContext)
        parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
    parameters->setString("function"_s, isFileUploadScript);
    parameters->setArray("arguments"_s, WTFMove(arguments));
    m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError || !response.responseObject) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }

        auto valueString = response.responseObject->getString("result"_s);
        if (!valueString) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve element file upload information from browsing context"_s));
            return;
        }

        auto resultValue = JSON::Value::parseJSON(valueString);
        if (!resultValue) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not parse element file upload information"_s));
            return;
        }

        completionHandler(CommandResult::success(WTFMove(resultValue)));
    });
}

std::optional<Session::FileUploadType> Session::parseElementIsFileUploadResult(const RefPtr<JSON::Value>& resultValue)
{
    if (!resultValue)
        return std::nullopt;

    auto result = resultValue->asObject();
    if (!result)
        return std::nullopt;

    auto isFileUpload = result->getBoolean("fileUpload"_s);
    if (!isFileUpload || !*isFileUpload)
        return std::nullopt;

    auto multiple = result->getBoolean("multiple"_s);
    if (!multiple || !*multiple)
        return FileUploadType::Single;

    return FileUploadType::Multiple;
}

void Session::selectOptionElement(const String& elementID, Function<void(CommandResult&&)>&& completionHandler)
{
    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
    parameters->setString("frameHandle"_s, m_currentBrowsingContext.value_or(emptyString()));
    parameters->setString("nodeHandle"_s, elementID);
    m_host->sendCommandToBackend("selectOptionElement"_s, WTFMove(parameters), [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

void Session::elementClick(const String& elementID, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
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
            computeElementLayout(elementID, options, [this, protectedThis, elementID, completionHandler = WTFMove(completionHandler)](std::optional<Rect>&& rect, std::optional<Point>&& inViewCenter, bool isObscured, RefPtr<JSON::Object>&& error) mutable {
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
                            isOptionElement = tagName == "option"_s;
                    }

                    Function<void(CommandResult &&)> continueAfterClickFunction = [this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
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

void Session::elementIsEditable(const String& elementID, Function<void(CommandResult&&)>&& completionHandler)
{
    auto arguments = JSON::Array::create();
    arguments->pushString(createElement(elementID)->toJSONString());

    static constexpr auto isEditableScript =
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
        "}"_s;

    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
    if (m_currentBrowsingContext)
        parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
    parameters->setString("function"_s, isEditableScript);
    parameters->setArray("arguments"_s, WTFMove(arguments));
    m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError || !response.responseObject) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }

        auto valueString = response.responseObject->getString("result"_s);
        if (!valueString) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve element editable state from browsing context"_s));
            return;
        }

        auto resultValue = JSON::Value::parseJSON(valueString);
        if (!resultValue) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not parse element editable state"_s));
            return;
        }

        completionHandler(CommandResult::success(WTFMove(resultValue)));
    });
}

void Session::elementClear(const String& elementID, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
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
            computeElementLayout(elementID, options, [this, protectedThis, elementID, completionHandler = WTFMove(completionHandler)](std::optional<Rect>&& rect, std::optional<Point>&& inViewCenter, bool, RefPtr<JSON::Object>&& error) mutable {
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
                parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
                if (m_currentBrowsingContext)
                    parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
                parameters->setString("function"_s, StringImpl::createWithoutCopying(FormElementClearJavaScript));
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

void Session::setInputFileUploadFiles(const String& elementID, const String& text, bool multiple, Function<void(CommandResult&&)>&& completionHandler)
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
    parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
    parameters->setString("frameHandle"_s, m_currentBrowsingContext.value_or(emptyString()));
    parameters->setString("nodeHandle"_s, elementID);
    parameters->setArray("filenames"_s, WTFMove(filenames));
    m_host->sendCommandToBackend("setFilesForInputFileUpload"_s, WTFMove(parameters), [protectedThis = Ref { *this }, elementID, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }

        completionHandler(CommandResult::success());
    });
}

String Session::virtualKeyForKey(char16_t key, KeyModifier& modifier)
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
        modifier = KeyModifier::Shift;
        return "Shift"_s;
    case 0xE050U:
        modifier = KeyModifier::Shift;
        return "ShiftRight"_s;
    case 0xE009U:
        modifier = KeyModifier::Control;
        return "Control"_s;
    case 0xE051U:
        modifier = KeyModifier::Control;
        return "ControlRight"_s;
    case 0xE00AU:
        modifier = KeyModifier::Alternate;
        return "Alternate"_s;
    case 0xE052U:
        modifier = KeyModifier::Alternate;
        return "AlternateRight"_s;
    case 0xE00BU:
        return "Pause"_s;
    case 0xE00CU:
        return "Escape"_s;
    case 0xE00DU:
        return "Space"_s;
    case 0xE00EU:
        return "PageUp"_s;
    case 0xE054U:
        return "PageUpRight"_s;
    case 0xE00FU:
        return "PageDown"_s;
    case 0xE055U:
        return "PageDownRight"_s;
    case 0xE010U:
        return "End"_s;
    case 0xE056U:
        return "EndRight"_s;
    case 0xE011U:
        return "Home"_s;
    case 0xE057U:
        return "HomeRight"_s;
    case 0xE012U:
        return "LeftArrow"_s;
    case 0xE058U:
        return "LeftArrowRight"_s;
    case 0xE013U:
        return "UpArrow"_s;
    case 0xE059U:
        return "UpArrowRight"_s;
    case 0xE014U:
        return "RightArrow"_s;
    case 0xE05AU:
        return "RightArrowRight"_s;
    case 0xE015U:
        return "DownArrow"_s;
    case 0xE05BU:
        return "DownArrowRight"_s;
    case 0xE016U:
        return "Insert"_s;
    case 0xE05CU:
        return "InsertRight"_s;
    case 0xE017U:
        return "Delete"_s;
    case 0xE05DU:
        return "DeleteRight"_s;
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
        modifier = KeyModifier::Meta;
        return "Meta"_s;
    case 0xE053U:
        modifier = KeyModifier::Meta;
        return "MetaRight"_s;
    default:
        break;
    }
    return String();
}

void Session::elementSendKeys(const String& elementID, const String& text, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, elementID, text, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
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
            if (!fileUploadType || capabilities().strictFileInteractability.value_or(false)) {
                // FIXME: move this to an atom.
                static constexpr auto focusScript =
                    "function focus(element) {"
                    "    let doc = element.ownerDocument || element;"
                    "    let prevActiveElement = doc.activeElement;"
                    "    let elementRootNode = element.getRootNode();"
                    "    if (elementRootNode.activeElement !== element && prevActiveElement)"
                    "        prevActiveElement.blur();"
                    "    element.focus();"
                    "    let tagName = element.tagName.toUpperCase();"
                    "    if (tagName === 'BODY' || element === document.documentElement)"
                    "        return;"
                    "    let isTextElement = tagName === 'TEXTAREA' || (tagName === 'INPUT' && element.type === 'text');"
                    "    if (isTextElement && element.selectionEnd == 0)"
                    "        element.setSelectionRange(element.value.length, element.value.length);"
                    "    if (elementRootNode.activeElement !== element)"
                    "        throw {name: 'ElementNotInteractable', message: 'Element is not focusable.'};"
                    "}"_s;

                auto arguments = JSON::Array::create();
                arguments->pushString(createElement(elementID)->toJSONString());
                auto parameters = JSON::Object::create();
                parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
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
                            interaction.text = span(key);
                        interactions.append(WTFMove(interaction));
                    }

                    // Reset sticky modifiers if needed.
                    if (stickyModifiers) {
                        if (stickyModifiers & KeyModifier::Shift)
                            interactions.append({ KeyboardInteractionType::KeyRelease, std::nullopt, std::optional<String>("Shift"_s) });
                        if (stickyModifiers & KeyModifier::Control)
                            interactions.append({ KeyboardInteractionType::KeyRelease, std::nullopt, std::optional<String>("Control"_s) });
                        if (stickyModifiers & KeyModifier::Alternate)
                            interactions.append({ KeyboardInteractionType::KeyRelease, std::nullopt, std::optional<String>("Alternate"_s) });
                        if (stickyModifiers & KeyModifier::Meta)
                            interactions.append({ KeyboardInteractionType::KeyRelease, std::nullopt, std::optional<String>("Meta"_s) });
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

void Session::getPageSource(Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
        parameters->setString("function"_s, "function() { return document.documentElement.outerHTML; }"_s);
        parameters->setArray("arguments"_s, JSON::Array::create());
        m_host->sendCommandToBackend("evaluateJavaScriptFunction"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto valueString = response.responseObject->getString("result"_s);
            if (!valueString) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve page source from browsing context"_s));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not parse page source information"_s));
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

void Session::executeScript(const String& script, RefPtr<JSON::Array>&& argumentsArray, ExecuteScriptMode mode, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, script, argumentsArray = WTFMove(argumentsArray), mode, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
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
        parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        parameters->setString("function"_s, makeString("function(){\n"_s, script, "\n}"_s));
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
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve script result from browsing context"_s));
                return;
            }

            auto resultValue = JSON::Value::parseJSON(valueString);
            if (!resultValue) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not parse script result"_s));
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

void Session::performMouseInteraction(int x, int y, MouseButton button, MouseInteraction interaction, Function<void(CommandResult&&)>&& completionHandler)
{
    auto parameters = JSON::Object::create();
    parameters->setString("handle"_s, uncheckedTopLevelBrowsingContext());
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
    m_host->sendCommandToBackend("performMouseInteraction"_s, WTFMove(parameters), [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

void Session::performKeyboardInteractions(Vector<KeyboardInteraction>&& interactions, Function<void(CommandResult&&)>&& completionHandler)
{
    auto parameters = JSON::Object::create();
    parameters->setString("handle"_s, uncheckedTopLevelBrowsingContext());
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
    m_host->sendCommandToBackend("performKeyboardInteractions"_s, WTFMove(parameters), [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

static std::optional<Session::Cookie> parseAutomationCookie(const JSON::Object& cookieObject)
{
    Session::Cookie cookie;

    cookie.name = cookieObject.getString("name"_s);
    if (!cookie.name)
        return std::nullopt;

    cookie.value = cookieObject.getString("value"_s);
    if (!cookie.value)
        return std::nullopt;

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

    auto sameSite = cookieObject.getString("sameSite"_s);
    if (!!sameSite)
        cookie.sameSite = sameSite;

    return cookie;
}

static Ref<JSON::Object> builtAutomationCookie(const Session::Cookie& cookie)
{
    auto cookieObject = JSON::Object::create();
    cookieObject->setString("name"_s, cookie.name);
    cookieObject->setString("value"_s, cookie.value);
    cookieObject->setString("path"_s, cookie.path.value_or("/"_s));
    cookieObject->setString("domain"_s, cookie.domain.value_or(emptyString()));
    cookieObject->setBoolean("secure"_s, cookie.secure.value_or(false));
    cookieObject->setBoolean("httpOnly"_s, cookie.httpOnly.value_or(false));
    cookieObject->setBoolean("session"_s, !cookie.expiry);
    cookieObject->setDouble("expires"_s, cookie.expiry.value_or(0));
    cookieObject->setString("sameSite"_s, cookie.sameSite.value_or("None"_s));
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
    if (cookie.sameSite)
        cookieObject->setString("sameSite"_s, cookie.sameSite.value());
    return cookieObject;
}

void Session::getAllCookies(Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
        m_host->sendCommandToBackend("getAllCookies"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto cookiesArray = response.responseObject->getArray("cookies"_s);
            if (!cookiesArray) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve cookies from browsing context"_s));
                return;
            }

            auto cookies = JSON::Array::create();
            for (unsigned i = 0; i < cookiesArray->length(); ++i) {
                auto cookieObject = cookiesArray->get(i)->asObject();
                if (!cookieObject) {
                    completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Invalid cookie object in browsing context"_s));
                    return;
                }

                auto cookie = parseAutomationCookie(*cookieObject);
                if (!cookie) {
                    completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Invalid cookie data in browsing context"_s));
                    return;
                }
                cookies->pushObject(serializeCookie(cookie.value()));
            }
            completionHandler(CommandResult::success(WTFMove(cookies)));
        });
    });
}

void Session::getNamedCookie(const String& name, Function<void(CommandResult&&)>&& completionHandler)
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

void Session::addCookie(const Cookie& cookie, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, cookie = builtAutomationCookie(cookie), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
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

void Session::deleteCookie(const String& name, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, name, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
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

void Session::deleteAllCookies(Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
        m_host->sendCommandToBackend("deleteAllCookies"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            completionHandler(CommandResult::success());
        });
    });
}

InputSource& Session::getOrCreateInputSource(const String& id, InputSource::Type type, std::optional<PointerType> pointerType)
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

static ASCIILiteral automationSourceType(const InputSource& inputSource)
{
    switch (inputSource.type) {
    case InputSource::Type::None:
        return "Null"_s;
    case InputSource::Type::Pointer:
        switch (inputSource.pointerType.value_or(PointerType::Mouse)) {
        case PointerType::Mouse:
            return "Mouse"_s;
        case PointerType::Touch:
            return "Touch"_s;
        case PointerType::Pen:
            return "Pen"_s;
        }
        break;
    case InputSource::Type::Key:
        return "Keyboard"_s;
    case InputSource::Type::Wheel:
        return "Wheel"_s;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static ASCIILiteral automationOriginType(PointerOrigin::Type type)
{
    switch (type) {
    case PointerOrigin::Type::Viewport:
        return "Viewport"_s;
    case PointerOrigin::Type::Pointer:
        return "Pointer"_s;
    case PointerOrigin::Type::Element:
        return "Element"_s;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void Session::performActions(Vector<Vector<Action>>&& actionsByTick, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_currentBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, actionsByTick = WTFMove(actionsByTick), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
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
        parameters->setString("handle"_s, uncheckedTopLevelBrowsingContext());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());

        HashSet<String> inputSourcesSet;
        auto steps = JSON::Array::create();
        for (const auto& tick : actionsByTick) {
            auto states = JSON::Array::create();
            for (const auto& action : tick) {
                inputSourcesSet.add(action.id);
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
                        currentState.pressedButton = std::nullopt;
                        break;
                    case Action::Subtype::PointerDown:
                        currentState.pressedButton = action.button.value();
                        break;
                    case Action::Subtype::PointerMove: {
                        if (!action.x || !action.y)
                            return completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Pointer move action must have x and y coordinates."_s));
                        state->setString("origin"_s, automationOriginType(action.origin->type));
                        auto location = JSON::Object::create();
                        location->setInteger("x"_s, action.x.value());
                        location->setInteger("y"_s, action.y.value());
                        state->setObject("location"_s, WTFMove(location));
                        if (action.origin->type == PointerOrigin::Type::Element)
                            state->setString("nodeHandle"_s, action.origin->elementID.value());
                        [[fallthrough]];
                    }
                    case Action::Subtype::Pause:
                        if (action.duration)
                            state->setDouble("duration"_s, action.duration.value());
                        break;
                    case Action::Subtype::PointerCancel:
                        currentState.pressedButton = std::nullopt;
                        break;
                    case Action::Subtype::KeyUp:
                    case Action::Subtype::KeyDown:
                    case Action::Subtype::Scroll:
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
                            currentState.pressedKey = std::nullopt;
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
                    case Action::Subtype::Scroll:
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
                case Action::Type::Wheel:
                    switch (action.subtype) {
                    case Action::Subtype::Scroll: {
                        if (!action.x || !action.y)
                            return completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Scroll action must have x and y coordinates."_s));
                        if (!action.deltaX || !action.deltaY)
                            return completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Scroll action must have deltaX and deltaY values."_s));
                        state->setString("origin"_s, automationOriginType(action.origin->type));
                        auto location = JSON::Object::create();
                        location->setInteger("x"_s, action.x.value());
                        location->setInteger("y"_s, action.y.value());
                        state->setObject("location"_s, WTFMove(location));

                        auto delta = JSON::Object::create();
                        delta->setInteger("width"_s, action.deltaX.value());
                        delta->setInteger("height"_s, action.deltaY.value());
                        state->setObject("delta"_s, WTFMove(delta));

                        if (action.origin->type == PointerOrigin::Type::Element)
                            state->setString("nodeHandle"_s, action.origin->elementID.value());
                        [[fallthrough]];
                    }
                    case Action::Subtype::Pause:
                        if (action.duration)
                            state->setDouble("duration"_s, action.duration.value());
                        break;
                    case Action::Subtype::PointerUp:
                    case Action::Subtype::PointerDown:
                    case Action::Subtype::PointerMove:
                    case Action::Subtype::PointerCancel:
                    case Action::Subtype::KeyUp:
                    case Action::Subtype::KeyDown:
                        ASSERT_NOT_REACHED();
                    }
                }
                states->pushObject(WTFMove(state));
            }
            auto stepStates = JSON::Object::create();
            stepStates->setArray("states"_s, WTFMove(states));
            steps->pushObject(WTFMove(stepStates));
        }

        parameters->setArray("steps"_s, WTFMove(steps));

        auto inputSources = JSON::Array::create();
        for (const auto& id : inputSourcesSet) {
            const auto& inputSource = m_activeInputSources.get(id);
            auto inputSourceObject = JSON::Object::create();
            inputSourceObject->setString("sourceId"_s, id);
            inputSourceObject->setString("sourceType"_s, automationSourceType(inputSource));
            inputSources->pushObject(WTFMove(inputSourceObject));
        }
        parameters->setArray("inputSources"_s, WTFMove(inputSources));

        m_host->sendCommandToBackend("performInteractionSequence"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
            if (response.isError) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }
            completionHandler(CommandResult::success());
        });
    });
}

void Session::releaseActions(Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    m_activeInputSources.clear();
    m_inputStateTable.clear();

    auto parameters = JSON::Object::create();
    parameters->setString("handle"_s, uncheckedTopLevelBrowsingContext());
    m_host->sendCommandToBackend("cancelInteractionSequence"_s, WTFMove(parameters), [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

void Session::dismissAlert(Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
    m_host->sendCommandToBackend("dismissCurrentJavaScriptDialog"_s, WTFMove(parameters), [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

void Session::acceptAlert(Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
    m_host->sendCommandToBackend("acceptCurrentJavaScriptDialog"_s, WTFMove(parameters), [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

void Session::getAlertText(Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
    m_host->sendCommandToBackend("messageOfCurrentJavaScriptDialog"_s, WTFMove(parameters), [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError || !response.responseObject) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }

        auto valueString = response.responseObject->getString("message"_s);
        if (!valueString) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve alert text from browsing context"_s));
            return;
        }

        completionHandler(CommandResult::success(JSON::Value::create(valueString)));
    });
}

void Session::sendAlertText(const String& text, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!m_toplevelBrowsingContext) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    auto parameters = JSON::Object::create();
    parameters->setString("browsingContextHandle"_s, uncheckedTopLevelBrowsingContext());
    parameters->setString("userInput"_s, text);
    m_host->sendCommandToBackend("setUserInputForCurrentJavaScriptPrompt"_s, WTFMove(parameters), [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
            return;
        }
        completionHandler(CommandResult::success());
    });
}

void Session::takeScreenshot(std::optional<String> elementID, std::optional<bool> scrollIntoView, Function<void(CommandResult&&)>&& completionHandler)
{
    if ((elementID && !m_currentBrowsingContext) || (!elementID && !m_toplevelBrowsingContext)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::NoSuchWindow));
        return;
    }

    handleUserPrompts([this, protectedThis = Ref { *this }, elementID, scrollIntoView, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        auto parameters = JSON::Object::create();
        parameters->setString("handle"_s, uncheckedTopLevelBrowsingContext());
        if (m_currentBrowsingContext)
            parameters->setString("frameHandle"_s, m_currentBrowsingContext.value());
        if (elementID)
            parameters->setString("nodeHandle"_s, elementID.value());
        parameters->setBoolean("clipToViewport"_s, true);
        if (scrollIntoView.value_or(false))
            parameters->setBoolean("scrollIntoViewIfNeeded"_s, true);
        m_host->sendCommandToBackend("takeScreenshot"_s, WTFMove(parameters), [protectedThis, completionHandler = WTFMove(completionHandler)](SessionHost::CommandResponse&& response) mutable {
            if (response.isError || !response.responseObject) {
                completionHandler(CommandResult::fail(WTFMove(response.responseObject)));
                return;
            }

            auto data = response.responseObject->getString("data"_s);
            if (!data) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, "Could not retrieve screenshot data from browsing context"_s));
                return;
            }

            completionHandler(CommandResult::success(JSON::Value::create(data)));
        });
    });
}

#if ENABLE(WEBDRIVER_BIDI)
void Session::dispatchBidiMessage(RefPtr<JSON::Object>&& message)
{
    // Validate bidi message
    auto params = message->getObject("params"_s);
    if (!params) {
        RELEASE_LOG(WebDriverBiDi, "Session::dispatchBidiMessage: Missing 'params' field in payload");
        m_bidiServer->sendErrorResponse(this->id(), std::nullopt, CommandResult::ErrorCode::UnknownError, "Received malformed bidi message from browser: Missing 'params' field."_s);
        return;
    }

    auto bidiMessageString = params->getString("message"_s);
    if (!bidiMessageString) {
        RELEASE_LOG(WebDriverBiDi, "Session::dispatchBidiMessage: Missing actual bidi message in payload");
        m_bidiServer->sendErrorResponse(this->id(), std::nullopt, CommandResult::ErrorCode::UnknownError, "Received malformed bidi message from browser: Missing 'message' field."_s);
        return;
    }

    auto bidiMessageValue = JSON::Value::parseJSON(bidiMessageString);
    if (!bidiMessageValue) {
        RELEASE_LOG(WebDriverBiDi, "Session::dispatchBidiMessage: Bidi message with invalid JSON.");
        m_bidiServer->sendErrorResponse(this->id(), std::nullopt, CommandResult::ErrorCode::UnknownError, "Received malformed bidi message from browser: Invalid JSON message."_s);
        return;
    }

    auto bidiMessage = bidiMessageValue->asObject();
    LOG(WebDriverBiDi, "Session::dispatchBidiMessage: received bidi message %s", bidiMessageValue->toJSONString().utf8().data());

    // FIXME: Move event subscription into the browser
    if (bidiMessage->getString("type"_s) == "event"_s) {
        if (bidiMessage->size() < 3 || (bidiMessage->find("method"_s) == bidiMessage->end()) || (bidiMessage->find("params"_s) == bidiMessage->end())) {
            RELEASE_LOG(WebDriverBiDi, "Session::dispatchBidiMessage: Malformed bidi event: %s", bidiMessageValue->toJSONString().utf8().data());
            return;
        }

        auto bidiMethod = bidiMessage->getString("method"_s);
        if (!eventIsEnabled(bidiMethod, { uncheckedTopLevelBrowsingContext() })) {
            RELEASE_LOG(WebDriverBiDi, "Message %s is an unknown event or not enabled, ignoring.", bidiMethod.utf8().data());
            return;
        }
        if (bidiMethod == "log.entryAdded"_s)
            doLogEntryAdded(WTFMove(bidiMessage));
        return;
    }

    m_bidiServer->sendMessage(this->id(), bidiMessage->toJSONString());
}

void Session::doLogEntryAdded(RefPtr<JSON::Object>&& message)
{
    // https://w3c.github.io/webdriver-bidi/#event-log-entryAdded
    auto params = message->getObject("params"_s);
    if (!params) {
        RELEASE_LOG(WebDriverBiDi, "Log event without parameter information, ignoring.");
        return;
    }

    auto method = params->getString("method"_s);

    String level;
    if (method == "error"_s || method == "assert"_s)
        level = "error"_s;
    else if (method == "debug"_s || method == "trace"_s)
        level = "debug"_s;
    else if (method == "warn"_s)
        level = "warn"_s;
    else
        level = "info"_s;

    // WebDriver uses the ECMA time format, which is the number of milliseconds since the Unix epoch.
    // https://tc39.es/ecma262/#sec-time-values-and-time-range
    RefPtr<JSON::Value> timestampValue = JSON::Value::create(params->getDouble("timestamp"_s).value_or(0));
    auto messageText = params->getString("text"_s);

    // TODO Get the source from the current realm record
    // https://bugs.webkit.org/show_bug.cgi?id=282978

    // TODO Get the current stacktrace for assert, error, trace, and warn messages
    // https://bugs.webkit.org/show_bug.cgi?id=282979

    auto messageType = params->getString("type"_s);

    auto entry = JSON::Object::create();
    entry->setString("type"_s, messageType);
    entry->setString("level"_s, level);
    entry->setString("text"_s, messageText);
    entry->setValue("timestamp"_s, *timestampValue);

    if (messageType == "console"_s) {
        // TODO Support formatter string and multiple arguments
        // This will require changes either on `Automation.json::doLogEntryAdded`, adding the arguments,
        // or moving the actual event processing to the browser, so we just relay it back to the client.
        // https://bugs.webkit.org/show_bug.cgi?id=282976
        // TODO Implement the full serialization algorithm
        // https://bugs.webkit.org/show_bug.cgi?id=282977
        auto args = JSON::Array::create();
        auto arg = JSON::Object::create();
        arg->setString("type"_s, "string"_s);
        arg->setString("value"_s, messageText);
        args->pushObject(WTFMove(arg));

        entry->setString("method"_s, method);
        entry->setArray("args"_s, args);
    } else if (messageType == "javascript"_s) {
        // Despite not having the stack information, the spec requires this
        // field for JavaScript messages.
        entry->setString("stackTrace"_s, emptyString());
    }

    auto body = JSON::Object::create();
    body->setObject("params"_s, WTFMove(entry));

    emitEvent("log.entryAdded"_s, WTFMove(body));

    // TODO Implement event buffering, to save the log entries for later emission when the user subscribes to it
    // https://bugs.webkit.org/show_bug.cgi?id=282980
}

void Session::emitEvent(const String& eventName, RefPtr<JSON::Object>&& body)
{
    // https://w3c.github.io/webdriver-bidi/#emit-an-event
    body->setString("type"_s, "event"_s);
    body->setString("method"_s, eventName);
    m_bidiServer->sendMessage(this->id(), body->toJSONString());
}

bool Session::eventIsEnabled(const String& eventName, const Vector<String>&)
{
    // https://w3c.github.io/webdriver-bidi/#event-is-enabled

    AtomString atomEventName { eventName };

    if (!m_eventSubscriptionCounts.contains(atomEventName))
        return false;

    for (const auto& subscription : m_eventSubscriptions) {
        // FIXME: Add support to subscribe to specific browsing contexts
        // https://bugs.webkit.org/show_bug.cgi?id=282981
        if (!subscription.value.isGlobal())
            continue;

        if (subscription.value.events.contains(atomEventName))
            return true;
    }

    return false;
}

void Session::subscribeForEvents(const Vector<String>& events, Vector<String>&& browsingContextIDs, Vector<String>&& userContextIDs, Function<void(CommandResult&&)>&& completionHandler)
{
    // FIXME: Process/validate list of event names (e.g. expanding if given only the module name)
    // https://bugs.webkit.org/show_bug.cgi?id=291371
    auto subscriptionID = WTF::createVersion4UUIDString();

    for (const auto& event : events) {
        auto addResult = m_eventSubscriptionCounts.add(AtomString { event }, 1);
        if (!addResult.isNewEntry)
            addResult.iterator->value++;
    }

    Vector<AtomString> atomEventNames;
    for (const auto& event : events)
        atomEventNames.append(AtomString { event });

    m_eventSubscriptions.add(subscriptionID, EventSubscription { subscriptionID, WTFMove(atomEventNames), WTFMove(browsingContextIDs), WTFMove(userContextIDs) });
    completionHandler(CommandResult::success(JSON::Value::create(subscriptionID)));
}

void Session::unsubscribeByIDs(const Vector<EventSubscriptionID>& subscriptionIDs, Function<void(CommandResult&&)>&& completionHandler)
{
    for (const auto& id : subscriptionIDs) {
        if (!m_eventSubscriptions.contains(id)) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "At least one subscription id is unknown"_s));
            return;
        }
    }

    for (const auto& id : subscriptionIDs) {
        const auto& subscription = m_eventSubscriptions.get(id);

        for (const auto& event : subscription.events) {
            auto removeResult = m_eventSubscriptionCounts.find(event);
            if (removeResult != m_eventSubscriptionCounts.end()) {
                if (!(--removeResult->value))
                    m_eventSubscriptionCounts.remove(event);
            }
        }
        m_eventSubscriptions.remove(id);
    }
    completionHandler(CommandResult::success());
}

void Session::unsubscribeByEventName(const Vector<String>& eventNames, Function<void(CommandResult&&)>&& completionHandler)

{
    HashMap<String, EventSubscription> subscriptionsToKeep;
    HashSet<String> matchedEvents;
    // FIXME: Process/validate list of event names (e.g. expanding if given only the module name)
    // https://bugs.webkit.org/show_bug.cgi?id=291371
    for (const auto& eventName : eventNames) {
        auto atomEventName = AtomString { eventName };
        for (const auto& subscription : m_eventSubscriptions) {
            if (!subscription.value.events.contains(atomEventName)) {
                subscriptionsToKeep.add(subscription.value.id, subscription.value);
                continue;
            }

            // FIXME: Add support to subscribe to specific browsing contexts
            // https://bugs.webkit.org/show_bug.cgi?id=282981
            // In this case, only process them if we were given the "contexts" parameter
            if (!subscription.value.isGlobal()) {
                subscriptionsToKeep.add(subscription.value.id, subscription.value);
                continue;
            }

            auto currentSubscriptionEventNames = subscription.value.events;
            currentSubscriptionEventNames.removeAll(atomEventName);
            matchedEvents.add(eventName);
            if (!currentSubscriptionEventNames.isEmpty()) {
                auto clonedSubscription = subscription.value;
                clonedSubscription.events = currentSubscriptionEventNames;
                subscriptionsToKeep.add(clonedSubscription.id, WTFMove(clonedSubscription));
            }
        }
    }

    if (matchedEvents.size() != eventNames.size()) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    // Only modify the actual subscriptions after we validated all requested events.
    m_eventSubscriptions = WTFMove(subscriptionsToKeep);
    m_eventSubscriptionCounts.clear();
    for (const auto& subscription : m_eventSubscriptions.values()) {
        for (const auto& event : subscription.events) {
            auto addResult = m_eventSubscriptionCounts.add(event, 1);
            if (!addResult.isNewEntry)
                addResult.iterator->value++;
        }
    }

    completionHandler(CommandResult::success());
}

void Session::relayBidiCommand(const String& message, unsigned commandId, Function<void(WebSocketMessageHandler::Message&&)>&& completionHandler)
{
    auto parameters = JSON::Object::create();
    parameters->setString("message"_s, message);
    m_host->sendCommandToBackend("processBidiMessage"_s, WTFMove(parameters), [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), commandId](SessionHost::CommandResponse&& response) {
        if (response.isError) {
            auto errorCode = CommandResult::ErrorCode::UnknownError;
            std::optional<String> errorMessage;
            if (response.responseObject)
                errorMessage = response.responseObject->getString("message"_s);
            completionHandler(WebSocketMessageHandler::Message::fail(errorCode, std::nullopt, errorMessage, { commandId }));
        }
    });
}

#endif // ENABLE(WEBDRIVER_BIDI)

} // namespace WebDriver
