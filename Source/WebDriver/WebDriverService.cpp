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
#include "WebDriverService.h"

#include "Capabilities.h"
#include "CommandResult.h"
#include "SessionHost.h"
#include <inspector/InspectorValues.h>
#include <wtf/RunLoop.h>
#include <wtf/text/WTFString.h>

using namespace Inspector;

namespace WebDriver {

WebDriverService::WebDriverService()
    : m_server(*this)
{
}

static void printUsageStatement(const char* programName)
{
    printf("Usage: %s options\n", programName);
    printf("  -h, --help                Prints this help message\n");
    printf("  -p <port>, --port=<port>  Port number the driver will use\n");
    printf("\n");
}

int WebDriverService::run(int argc, char** argv)
{
    String portString;
    for (unsigned i = 1 ; i < argc; ++i) {
        const char* arg = argv[i];
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            printUsageStatement(argv[0]);
            return EXIT_SUCCESS;
        }

        if (!strcmp(arg, "-p") && portString.isNull()) {
            if (++i == argc) {
                printUsageStatement(argv[0]);
                return EXIT_FAILURE;
            }
            portString = argv[i];
            continue;
        }

        static const unsigned portStrLength = strlen("--port=");
        if (!strncmp(arg, "--port=", portStrLength) && portString.isNull()) {
            portString = String(arg + portStrLength);
            continue;
        }
    }

    if (portString.isNull()) {
        printUsageStatement(argv[0]);
        return EXIT_FAILURE;
    }

    bool ok;
    unsigned port = portString.toUInt(&ok);
    if (!ok) {
        fprintf(stderr, "Invalid port %s provided\n", portString.ascii().data());
        return EXIT_FAILURE;
    }

    RunLoop::initializeMainRunLoop();

    if (!m_server.listen(port))
        return EXIT_FAILURE;

    RunLoop::run();

    return EXIT_SUCCESS;
}

void WebDriverService::quit()
{
    m_server.disconnect();
    RunLoop::main().stop();
}

const WebDriverService::Command WebDriverService::s_commands[] = {
    { HTTPMethod::Post, "/session", &WebDriverService::newSession },
    { HTTPMethod::Delete, "/session/$sessionId", &WebDriverService::deleteSession },
    { HTTPMethod::Post, "/session/$sessionId/timeouts", &WebDriverService::setTimeouts },

    { HTTPMethod::Post, "/session/$sessionId/url", &WebDriverService::go },
    { HTTPMethod::Get, "/session/$sessionId/url", &WebDriverService::getCurrentURL },
    { HTTPMethod::Post, "/session/$sessionId/back", &WebDriverService::back },
    { HTTPMethod::Post, "/session/$sessionId/forward", &WebDriverService::forward },
    { HTTPMethod::Post, "/session/$sessionId/refresh", &WebDriverService::refresh },
    { HTTPMethod::Get, "/session/$sessionId/title", &WebDriverService::getTitle },

    { HTTPMethod::Get, "/session/$sessionId/window", &WebDriverService::getWindowHandle },
    { HTTPMethod::Delete, "/session/$sessionId/window", &WebDriverService::closeWindow },
    { HTTPMethod::Post, "/session/$sessionId/window", &WebDriverService::switchToWindow },
    { HTTPMethod::Get, "/session/$sessionId/window/handles", &WebDriverService::getWindowHandles },
    { HTTPMethod::Post, "/session/$sessionId/frame", &WebDriverService::switchToFrame },
    { HTTPMethod::Post, "/session/$sessionId/frame/parent", &WebDriverService::switchToParentFrame },

    // FIXME: Not in the spec, but still used by Selenium.
    { HTTPMethod::Get, "/session/$sessionId/window/position", &WebDriverService::getWindowPosition },
    { HTTPMethod::Post, "/session/$sessionId/window/position", &WebDriverService::setWindowPosition },
    { HTTPMethod::Get, "/session/$sessionId/window/size", &WebDriverService::getWindowSize },
    { HTTPMethod::Post, "/session/$sessionId/window/size", &WebDriverService::setWindowSize },

    { HTTPMethod::Post, "/session/$sessionId/element", &WebDriverService::findElement },
    { HTTPMethod::Post, "/session/$sessionId/elements", &WebDriverService::findElements },
    { HTTPMethod::Post, "/session/$sessionId/element/$elementId/element", &WebDriverService::findElementFromElement },
    { HTTPMethod::Post, "/session/$sessionId/element/$elementId/elements", &WebDriverService::findElementsFromElement },

    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/selected", &WebDriverService::isElementSelected },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/attribute/$name", &WebDriverService::getElementAttribute },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/text", &WebDriverService::getElementText },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/name", &WebDriverService::getElementTagName },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/rect", &WebDriverService::getElementRect },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/enabled", &WebDriverService::isElementEnabled },

    { HTTPMethod::Post, "/session/$sessionId/element/$elementId/click", &WebDriverService::elementClick },
    { HTTPMethod::Post, "/session/$sessionId/element/$elementId/clear", &WebDriverService::elementClear },
    { HTTPMethod::Post, "/session/$sessionId/element/$elementId/value", &WebDriverService::elementSendKeys },

    // FIXME: Not in the spec, but still used by Selenium.
    { HTTPMethod::Post, "/session/$sessionId/element/$elementId/submit", &WebDriverService::elementSubmit },

    { HTTPMethod::Post, "/session/$sessionId/execute/sync", &WebDriverService::executeScript },
    { HTTPMethod::Post, "/session/$sessionId/execute/async", &WebDriverService::executeAsyncScript },

    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/displayed", &WebDriverService::isElementDisplayed },
};

std::optional<WebDriverService::HTTPMethod> WebDriverService::toCommandHTTPMethod(const String& method)
{
    auto lowerCaseMethod = method.convertToASCIILowercase();
    if (lowerCaseMethod == "get")
        return WebDriverService::HTTPMethod::Get;
    if (lowerCaseMethod == "post" || lowerCaseMethod == "put")
        return WebDriverService::HTTPMethod::Post;
    if (lowerCaseMethod == "delete")
        return WebDriverService::HTTPMethod::Delete;

    return std::nullopt;
}

bool WebDriverService::findCommand(const String& method, const String& path, CommandHandler* handler, HashMap<String, String>& parameters)
{
    auto commandMethod = toCommandHTTPMethod(method);
    if (!commandMethod)
        return false;

    size_t length = WTF_ARRAY_LENGTH(s_commands);
    for (size_t i = 0; i < length; ++i) {
        if (s_commands[i].method != *commandMethod)
            continue;

        Vector<String> pathTokens;
        path.split("/", pathTokens);
        Vector<String> commandTokens;
        String::fromUTF8(s_commands[i].uriTemplate).split("/", commandTokens);
        if (pathTokens.size() != commandTokens.size())
            continue;

        bool allMatched = true;
        for (size_t j = 0; j < pathTokens.size() && allMatched; ++j) {
            if (commandTokens[j][0] == '$')
                parameters.set(commandTokens[j].substring(1), pathTokens[j]);
            else if (commandTokens[j] != pathTokens[j])
                allMatched = false;
        }

        if (allMatched) {
            *handler = s_commands[i].handler;
            return true;
        }

        parameters.clear();
    }

    return false;
}

void WebDriverService::handleRequest(HTTPRequestHandler::Request&& request, Function<void (HTTPRequestHandler::Response&&)>&& replyHandler)
{
    CommandHandler handler;
    HashMap<String, String> parameters;
    if (!findCommand(request.method, request.path, &handler, parameters)) {
        sendResponse(WTFMove(replyHandler), CommandResult::fail(CommandResult::ErrorCode::UnknownCommand, String("Unknown command: " + request.path)));
        return;
    }

    RefPtr<InspectorObject> parametersObject;
    if (request.dataLength) {
        RefPtr<InspectorValue> messageValue;
        if (!InspectorValue::parseJSON(String::fromUTF8(request.data, request.dataLength), messageValue)) {
            sendResponse(WTFMove(replyHandler), CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
            return;
        }

        if (!messageValue->asObject(parametersObject)) {
            sendResponse(WTFMove(replyHandler), CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
            return;
        }
    } else
        parametersObject = InspectorObject::create();
    for (const auto& parameter : parameters)
        parametersObject->setString(parameter.key, parameter.value);

    ((*this).*handler)(WTFMove(parametersObject), [this, replyHandler = WTFMove(replyHandler)](CommandResult&& result) mutable {
        sendResponse(WTFMove(replyHandler), WTFMove(result));
    });
}

void WebDriverService::sendResponse(Function<void (HTTPRequestHandler::Response&&)>&& replyHandler, CommandResult&& result) const
{
    RefPtr<InspectorObject> responseObject;
    if (result.isError()) {
        responseObject = InspectorObject::create();
        responseObject->setString(ASCIILiteral("error"), result.errorString());
        responseObject->setString(ASCIILiteral("message"), result.errorMessage().value_or(emptyString()));
        responseObject->setString(ASCIILiteral("stacktrace"), emptyString());
    } else {
        responseObject = InspectorObject::create();
        auto resultValue = result.result();
        responseObject->setValue(ASCIILiteral("value"), resultValue ? WTFMove(resultValue) : InspectorValue::null());
    }
    replyHandler({ result.httpStatusCode(), responseObject->toJSONString().utf8(), ASCIILiteral("application/json; charset=utf-8") });
}

bool WebDriverService::parseCapabilities(InspectorObject& desiredCapabilities, Capabilities& capabilities, Function<void (CommandResult&&)>& completionHandler)
{
    RefPtr<InspectorValue> value;
    if (desiredCapabilities.getValue(ASCIILiteral("browserName"), value) && !value->asString(capabilities.browserName)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, String("browserName parameter is invalid in capabilities")));
        return false;
    }
    if (desiredCapabilities.getValue(ASCIILiteral("version"), value) && !value->asString(capabilities.browserVersion)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, String("version parameter is invalid in capabilities")));
        return false;
    }
    if (desiredCapabilities.getValue(ASCIILiteral("platform"), value) && !value->asString(capabilities.platform)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, String("platform parameter is invalid in capabilities")));
        return false;
    }
    // FIXME: parse all other well-known capabilities: acceptInsecureCerts, pageLoadStrategy, proxy, setWindowRect, timeouts, unhandledPromptBehavior.
    return platformParseCapabilities(desiredCapabilities, capabilities, completionHandler);
}

RefPtr<Session> WebDriverService::findSessionOrCompleteWithError(InspectorObject& parameters, Function<void (CommandResult&&)>& completionHandler)
{
    String sessionID;
    if (!parameters.getString(ASCIILiteral("sessionId"), sessionID)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return nullptr;
    }

    auto session = m_sessions.get(sessionID);
    if (!session) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidSessionID));
        return nullptr;
    }

    return session;
}

void WebDriverService::newSession(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §8.1 New Session.
    // https://www.w3.org/TR/webdriver/#new-session
    RefPtr<InspectorObject> capabilitiesObject;
    if (!parameters->getObject(ASCIILiteral("capabilities"), capabilitiesObject)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated));
        return;
    }
    RefPtr<InspectorValue> requiredCapabilitiesValue;
    RefPtr<InspectorObject> requiredCapabilities;
    if (!capabilitiesObject->getValue(ASCIILiteral("alwaysMatch"), requiredCapabilitiesValue))
        requiredCapabilities = InspectorObject::create();
    else if (!requiredCapabilitiesValue->asObject(requiredCapabilities)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, String("alwaysMatch is invalid in capabilities")));
        return;
    }
    // FIXME: process firstMatch capabilities.

    Capabilities capabilities;
    if (!parseCapabilities(*requiredCapabilities, capabilities, completionHandler))
        return;

    auto sessionHost = std::make_unique<SessionHost>(WTFMove(capabilities));
    auto* sessionHostPtr = sessionHost.get();
    sessionHostPtr->connectToBrowser([this, sessionHost = WTFMove(sessionHost), completionHandler = WTFMove(completionHandler)](SessionHost::Succeeded succeeded) mutable {
        if (succeeded == SessionHost::Succeeded::No) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, String("Failed to connect to browser")));
            return;
        }

        RefPtr<Session> session = Session::create(WTFMove(sessionHost));
        session->createTopLevelBrowsingContext([this, session, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
            if (result.isError()) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, result.errorString()));
                return;
            }

            m_activeSession = session.get();
            m_sessions.add(session->id(), session);
            RefPtr<InspectorObject> resultObject = InspectorObject::create();
            resultObject->setString(ASCIILiteral("sessionId"), session->id());
            RefPtr<InspectorObject> capabilities = InspectorObject::create();
            capabilities->setString(ASCIILiteral("browserName"), session->capabilities().browserName);
            capabilities->setString(ASCIILiteral("version"), session->capabilities().browserVersion);
            capabilities->setString(ASCIILiteral("platform"), session->capabilities().platform);
            resultObject->setObject(ASCIILiteral("value"), WTFMove(capabilities));
            completionHandler(CommandResult::success(WTFMove(resultObject)));
        });
    });
}

void WebDriverService::deleteSession(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §8.2 Delete Session.
    // https://www.w3.org/TR/webdriver/#delete-session
    String sessionID;
    if (!parameters->getString(ASCIILiteral("sessionId"), sessionID)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    auto session = m_sessions.take(sessionID);
    if (!session) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidSessionID));
        return;
    }

    if (m_activeSession == session.get())
        m_activeSession = nullptr;

    session->close(WTFMove(completionHandler));
}

void WebDriverService::setTimeouts(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §8.5 Set Timeouts.
    // https://www.w3.org/TR/webdriver/#set-timeouts
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    Session::Timeouts timeouts;
    auto end = parameters->end();
    for (auto it = parameters->begin(); it != end; ++it) {
        if (it->key == "sessionId")
            continue;

        int timeoutMS;
        if (!it->value->asInteger(timeoutMS) || timeoutMS < 0 || timeoutMS > INT_MAX) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
            return;
        }

        if (it->key == "script")
            timeouts.script = Seconds::fromMilliseconds(timeoutMS);
        else if (it->key == "page load")
            timeouts.pageLoad = Seconds::fromMilliseconds(timeoutMS);
        else if (it->key == "implicit")
            timeouts.implicit = Seconds::fromMilliseconds(timeoutMS);
        else {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
            return;
        }
    }

    session->setTimeouts(timeouts, WTFMove(completionHandler));
}

void WebDriverService::go(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.1 Go.
    // https://www.w3.org/TR/webdriver/#go
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    String url;
    if (!parameters->getString(ASCIILiteral("url"), url)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    session->go(url, WTFMove(completionHandler));
}

void WebDriverService::getCurrentURL(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.2 Get Current URL.
    // https://www.w3.org/TR/webdriver/#get-current-url
    if (auto session = findSessionOrCompleteWithError(*parameters, completionHandler))
        session->getCurrentURL(WTFMove(completionHandler));
}

void WebDriverService::back(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.3 Back.
    // https://www.w3.org/TR/webdriver/#back
    if (auto session = findSessionOrCompleteWithError(*parameters, completionHandler))
        session->back(WTFMove(completionHandler));
}

void WebDriverService::forward(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.4 Forward.
    // https://www.w3.org/TR/webdriver/#forward
    if (auto session = findSessionOrCompleteWithError(*parameters, completionHandler))
        session->forward(WTFMove(completionHandler));
}

void WebDriverService::refresh(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.5 Refresh.
    // https://www.w3.org/TR/webdriver/#refresh
    if (auto session = findSessionOrCompleteWithError(*parameters, completionHandler))
        session->refresh(WTFMove(completionHandler));
}

void WebDriverService::getTitle(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.6 Get Title.
    // https://www.w3.org/TR/webdriver/#get-title
    if (auto session = findSessionOrCompleteWithError(*parameters, completionHandler))
        session->getTitle(WTFMove(completionHandler));
}

void WebDriverService::getWindowHandle(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.1 Get Window Handle.
    // https://www.w3.org/TR/webdriver/#get-window-handle
    if (auto session = findSessionOrCompleteWithError(*parameters, completionHandler))
        session->getWindowHandle(WTFMove(completionHandler));
}

void WebDriverService::getWindowPosition(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    if (auto session = findSessionOrCompleteWithError(*parameters, completionHandler))
        session->getWindowPosition(WTFMove(completionHandler));
}

void WebDriverService::setWindowPosition(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    int windowX;
    if (!parameters->getInteger(ASCIILiteral("x"), windowX)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    int windowY;
    if (!parameters->getInteger(ASCIILiteral("y"), windowY)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    session->setWindowPosition(windowX, windowY, WTFMove(completionHandler));
}

void WebDriverService::getWindowSize(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    if (auto session = findSessionOrCompleteWithError(*parameters, completionHandler))
        session->getWindowSize(WTFMove(completionHandler));
}

void WebDriverService::setWindowSize(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    int windowWidth;
    if (!parameters->getInteger(ASCIILiteral("width"), windowWidth)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    int windowHeight;
    if (!parameters->getInteger(ASCIILiteral("height"), windowHeight)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    session->setWindowSize(windowWidth, windowHeight, WTFMove(completionHandler));
}

void WebDriverService::closeWindow(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.2 Close Window.
    // https://www.w3.org/TR/webdriver/#close-window
    if (auto session = findSessionOrCompleteWithError(*parameters, completionHandler))
        session->closeWindow(WTFMove(completionHandler));
}

void WebDriverService::switchToWindow(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.3 Switch To Window.
    // https://www.w3.org/TR/webdriver/#switch-to-window
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    String handle;
    if (!parameters->getString(ASCIILiteral("handle"), handle)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    session->switchToWindow(handle, WTFMove(completionHandler));
}

void WebDriverService::getWindowHandles(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.4 Get Window Handles.
    // https://www.w3.org/TR/webdriver/#get-window-handles
    if (auto session = findSessionOrCompleteWithError(*parameters, completionHandler))
        session->getWindowHandles(WTFMove(completionHandler));
}

void WebDriverService::switchToFrame(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.5 Switch To Frame.
    // https://www.w3.org/TR/webdriver/#switch-to-frame
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    RefPtr<InspectorValue> frameID;
    if (!parameters->getValue(ASCIILiteral("id"), frameID)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    session->switchToFrame(WTFMove(frameID), WTFMove(completionHandler));
}

void WebDriverService::switchToParentFrame(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.6 Switch To Parent Frame.
    // https://www.w3.org/TR/webdriver/#switch-to-parent-frame
    if (auto session = findSessionOrCompleteWithError(*parameters, completionHandler))
        session->switchToParentFrame(WTFMove(completionHandler));
}

static std::optional<String> findElementOrCompleteWithError(InspectorObject& parameters, Function<void (CommandResult&&)>& completionHandler)
{
    String elementID;
    if (!parameters.getString(ASCIILiteral("elementId"), elementID) || elementID.isEmpty()) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return std::nullopt;
    }
    return elementID;
}

static bool findStrategyAndSelectorOrCompleteWithError(InspectorObject& parameters, Function<void (CommandResult&&)>& completionHandler, String& strategy, String& selector)
{
    if (!parameters.getString(ASCIILiteral("using"), strategy)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return false;
    }
    if (!parameters.getString(ASCIILiteral("value"), selector)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return false;
    }
    return true;
}

void WebDriverService::findElement(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §12.2 Find Element.
    // https://www.w3.org/TR/webdriver/#find-element
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    String strategy, selector;
    if (!findStrategyAndSelectorOrCompleteWithError(*parameters, completionHandler, strategy, selector))
        return;

    session->findElements(strategy, selector, Session::FindElementsMode::Single, emptyString(), WTFMove(completionHandler));
}

void WebDriverService::findElements(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §12.3 Find Elements.
    // https://www.w3.org/TR/webdriver/#find-elements
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    String strategy, selector;
    if (!findStrategyAndSelectorOrCompleteWithError(*parameters, completionHandler, strategy, selector))
        return;

    session->findElements(strategy, selector, Session::FindElementsMode::Multiple, emptyString(), WTFMove(completionHandler));
}

void WebDriverService::findElementFromElement(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §12.4 Find Element From Element.
    // https://www.w3.org/TR/webdriver/#find-element-from-element
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    String strategy, selector;
    if (!findStrategyAndSelectorOrCompleteWithError(*parameters, completionHandler, strategy, selector))
        return;

    session->findElements(strategy, selector, Session::FindElementsMode::Single, elementID.value(), WTFMove(completionHandler));
}

void WebDriverService::findElementsFromElement(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §12.5 Find Elements From Element.
    // https://www.w3.org/TR/webdriver/#find-elements-from-element
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    String strategy, selector;
    if (!findStrategyAndSelectorOrCompleteWithError(*parameters, completionHandler, strategy, selector))
        return;

    session->findElements(strategy, selector, Session::FindElementsMode::Multiple, elementID.value(), WTFMove(completionHandler));
}

void WebDriverService::isElementSelected(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §13.1 Is Element Selected.
    // https://www.w3.org/TR/webdriver/#is-element-selected
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    session->isElementSelected(elementID.value(), WTFMove(completionHandler));
}

void WebDriverService::getElementAttribute(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §13.2 Get Element Attribute.
    // https://www.w3.org/TR/webdriver/#get-element-attribute
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    String attribute;
    if (!parameters->getString(ASCIILiteral("name"), attribute)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    session->getElementAttribute(elementID.value(), attribute, WTFMove(completionHandler));
}

void WebDriverService::getElementText(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §13.5 Get Element Text.
    // https://www.w3.org/TR/webdriver/#get-element-text
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    session->getElementText(elementID.value(), WTFMove(completionHandler));
}

void WebDriverService::getElementTagName(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §13.6 Get Element Tag Name.
    // https://www.w3.org/TR/webdriver/#get-element-tag-name
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    session->getElementTagName(elementID.value(), WTFMove(completionHandler));
}

void WebDriverService::getElementRect(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §13.7 Get Element Rect.
    // https://www.w3.org/TR/webdriver/#get-element-rect
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    session->getElementRect(elementID.value(), WTFMove(completionHandler));
}

void WebDriverService::isElementEnabled(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §13.8 Is Element Enabled.
    // https://www.w3.org/TR/webdriver/#is-element-enabled
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    session->isElementEnabled(elementID.value(), WTFMove(completionHandler));
}

void WebDriverService::isElementDisplayed(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §C. Element Displayedness.
    // https://www.w3.org/TR/webdriver/#element-displayedness
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    session->isElementDisplayed(elementID.value(), WTFMove(completionHandler));
}

void WebDriverService::elementClick(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §14.1 Element Click.
    // https://www.w3.org/TR/webdriver/#element-click
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    session->elementClick(elementID.value(), WTFMove(completionHandler));
}

void WebDriverService::elementClear(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §14.2 Element Clear.
    // https://www.w3.org/TR/webdriver/#element-clear
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    session->elementClear(elementID.value(), WTFMove(completionHandler));
}

void WebDriverService::elementSendKeys(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §14.3 Element Send Keys.
    // https://www.w3.org/TR/webdriver/#element-send-keys
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    RefPtr<InspectorArray> valueArray;
    if (!parameters->getArray(ASCIILiteral("value"), valueArray)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    unsigned valueArrayLength = valueArray->length();
    if (!valueArrayLength) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }
    Vector<String> value;
    value.reserveInitialCapacity(valueArrayLength);
    for (unsigned i = 0; i < valueArrayLength; ++i) {
        if (auto keyValue = valueArray->get(i)) {
            String key;
            if (keyValue->asString(key))
                value.uncheckedAppend(WTFMove(key));
        }
    }
    if (!value.size()) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    session->elementSendKeys(elementID.value(), WTFMove(value), WTFMove(completionHandler));
}

void WebDriverService::elementSubmit(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    session->elementSubmit(elementID.value(), WTFMove(completionHandler));
}

static bool findScriptAndArgumentsOrCompleteWithError(InspectorObject& parameters, Function<void (CommandResult&&)>& completionHandler, String& script, RefPtr<InspectorArray>& arguments)
{
    if (!parameters.getString(ASCIILiteral("script"), script)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return false;
    }
    if (!parameters.getArray(ASCIILiteral("args"), arguments)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return false;
    }
    return true;
}

void WebDriverService::executeScript(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §15.2.1 Execute Script.
    // https://www.w3.org/TR/webdriver/#execute-script
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    String script;
    RefPtr<InspectorArray> arguments;
    if (!findScriptAndArgumentsOrCompleteWithError(*parameters, completionHandler, script, arguments))
        return;

    session->executeScript(script, WTFMove(arguments), Session::ExecuteScriptMode::Sync, WTFMove(completionHandler));
}

void WebDriverService::executeAsyncScript(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §15.2.2 Execute Async Script.
    // https://www.w3.org/TR/webdriver/#execute-async-script
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    String script;
    RefPtr<InspectorArray> arguments;
    if (!findScriptAndArgumentsOrCompleteWithError(*parameters, completionHandler, script, arguments))
        return;

    session->executeScript(script, WTFMove(arguments), Session::ExecuteScriptMode::Async, WTFMove(completionHandler));
}

} // namespace WebDriver
