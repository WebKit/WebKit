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
    for (int i = 1 ; i < argc; ++i) {
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

    m_server.disconnect();

    return EXIT_SUCCESS;
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
    { HTTPMethod::Get, "/session/$sessionId/window/rect", &WebDriverService::getWindowRect },
    { HTTPMethod::Post, "/session/$sessionId/window/rect", &WebDriverService::setWindowRect },

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

    { HTTPMethod::Get, "/session/$sessionId/cookie", &WebDriverService::getAllCookies },
    { HTTPMethod::Get, "/session/$sessionId/cookie/$name", &WebDriverService::getNamedCookie },
    { HTTPMethod::Post, "/session/$sessionId/cookie", &WebDriverService::addCookie },
    { HTTPMethod::Delete, "/session/$sessionId/cookie/$name", &WebDriverService::deleteCookie },
    { HTTPMethod::Delete, "/session/$sessionId/cookie", &WebDriverService::deleteAllCookies },

    { HTTPMethod::Post, "/session/$sessionId/alert/dismiss", &WebDriverService::dismissAlert },
    { HTTPMethod::Post, "/session/$sessionId/alert/accept", &WebDriverService::acceptAlert },
    { HTTPMethod::Get, "/session/$sessionId/alert/text", &WebDriverService::getAlertText },
    { HTTPMethod::Post, "/session/$sessionId/alert/text", &WebDriverService::sendAlertText },

    { HTTPMethod::Get, "/session/$sessionId/screenshot", &WebDriverService::takeScreenshot },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/screenshot", &WebDriverService::takeElementScreenshot },


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
    // §6.3 Processing Model.
    // https://w3c.github.io/webdriver/webdriver-spec.html#processing-model
    RefPtr<InspectorValue> resultValue;
    if (result.isError()) {
        // When required to send an error.
        // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-send-an-error
        // Let body be a new JSON Object initialised with the following properties: "error", "message", "stacktrace".
        auto errorObject = InspectorObject::create();
        errorObject->setString(ASCIILiteral("error"), result.errorString());
        errorObject->setString(ASCIILiteral("message"), result.errorMessage().value_or(emptyString()));
        errorObject->setString(ASCIILiteral("stacktrace"), emptyString());
        // If the error data dictionary contains any entries, set the "data" field on body to a new JSON Object populated with the dictionary.
        if (auto& additionalData = result.additionalErrorData())
            errorObject->setObject(ASCIILiteral("data"), RefPtr<InspectorObject> { additionalData });
        // Send a response with status and body as arguments.
        resultValue = WTFMove(errorObject);
    } else if (auto value = result.result())
        resultValue = WTFMove(value);
    else
        resultValue = InspectorValue::null();

    // When required to send a response.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-send-a-response
    RefPtr<InspectorObject> responseObject = InspectorObject::create();
    responseObject->setValue(ASCIILiteral("value"), WTFMove(resultValue));
    replyHandler({ result.httpStatusCode(), responseObject->toJSONString().utf8(), ASCIILiteral("application/json; charset=utf-8") });
}

static std::optional<Timeouts> deserializeTimeouts(InspectorObject& timeoutsObject)
{
    // §8.5 Set Timeouts.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-deserialize-as-a-timeout
    Timeouts timeouts;
    auto end = timeoutsObject.end();
    for (auto it = timeoutsObject.begin(); it != end; ++it) {
        if (it->key == "sessionId")
            continue;

        int timeoutMS;
        if (!it->value->asInteger(timeoutMS) || timeoutMS < 0 || timeoutMS > INT_MAX)
            return std::nullopt;

        if (it->key == "script")
            timeouts.script = Seconds::fromMilliseconds(timeoutMS);
        else if (it->key == "pageLoad")
            timeouts.pageLoad = Seconds::fromMilliseconds(timeoutMS);
        else if (it->key == "implicit")
            timeouts.implicit = Seconds::fromMilliseconds(timeoutMS);
        else
            return std::nullopt;
    }
    return timeouts;
}

static std::optional<PageLoadStrategy> deserializePageLoadStrategy(const String& pageLoadStrategy)
{
    if (pageLoadStrategy == "none")
        return PageLoadStrategy::None;
    if (pageLoadStrategy == "normal")
        return PageLoadStrategy::Normal;
    if (pageLoadStrategy == "eager")
        return PageLoadStrategy::Eager;
    return std::nullopt;
}

static std::optional<UnhandledPromptBehavior> deserializeUnhandledPromptBehavior(const String& unhandledPromptBehavior)
{
    if (unhandledPromptBehavior == "dismiss")
        return UnhandledPromptBehavior::Dismiss;
    if (unhandledPromptBehavior == "accept")
        return UnhandledPromptBehavior::Accept;
    if (unhandledPromptBehavior == "ignore")
        return UnhandledPromptBehavior::Ignore;
    return std::nullopt;
}

void WebDriverService::parseCapabilities(const InspectorObject& matchedCapabilities, Capabilities& capabilities) const
{
    // Matched capabilities have already been validated.
    bool acceptInsecureCerts;
    if (matchedCapabilities.getBoolean(ASCIILiteral("acceptInsecureCerts"), acceptInsecureCerts))
        capabilities.acceptInsecureCerts = acceptInsecureCerts;
    String browserName;
    if (matchedCapabilities.getString(ASCIILiteral("browserName"), browserName))
        capabilities.browserName = browserName;
    String browserVersion;
    if (matchedCapabilities.getString(ASCIILiteral("browserVersion"), browserVersion))
        capabilities.browserVersion = browserVersion;
    String platformName;
    if (matchedCapabilities.getString(ASCIILiteral("platformName"), platformName))
        capabilities.platformName = platformName;
    RefPtr<InspectorObject> timeouts;
    if (matchedCapabilities.getObject(ASCIILiteral("timeouts"), timeouts))
        capabilities.timeouts = deserializeTimeouts(*timeouts);
    String pageLoadStrategy;
    if (matchedCapabilities.getString(ASCIILiteral("pageLoadStrategy"), pageLoadStrategy))
        capabilities.pageLoadStrategy = deserializePageLoadStrategy(pageLoadStrategy);
    String unhandledPromptBehavior;
    if (matchedCapabilities.getString(ASCIILiteral("unhandledPromptBehavior"), unhandledPromptBehavior))
        capabilities.unhandledPromptBehavior = deserializeUnhandledPromptBehavior(unhandledPromptBehavior);
    platformParseCapabilities(matchedCapabilities, capabilities);
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

RefPtr<InspectorObject> WebDriverService::validatedCapabilities(const InspectorObject& capabilities) const
{
    // §7.2 Processing Capabilities.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-validate-capabilities
    RefPtr<InspectorObject> result = InspectorObject::create();
    auto end = capabilities.end();
    for (auto it = capabilities.begin(); it != end; ++it) {
        if (it->value->isNull())
            result->setValue(it->key, RefPtr<InspectorValue>(it->value));
        else if (it->key == "acceptInsecureCerts") {
            bool acceptInsecureCerts;
            if (!it->value->asBoolean(acceptInsecureCerts))
                return nullptr;
            result->setBoolean(it->key, acceptInsecureCerts);
        } else if (it->key == "browserName" || it->key == "browserVersion" || it->key == "platformName") {
            String stringValue;
            if (!it->value->asString(stringValue))
                return nullptr;
            result->setString(it->key, stringValue);
        } else if (it->key == "pageLoadStrategy") {
            String pageLoadStrategy;
            if (!it->value->asString(pageLoadStrategy) || !deserializePageLoadStrategy(pageLoadStrategy))
                return nullptr;
            result->setString(it->key, pageLoadStrategy);
        } else if (it->key == "proxy") {
            // FIXME: implement proxy support.
        } else if (it->key == "timeouts") {
            RefPtr<InspectorObject> timeouts;
            if (!it->value->asObject(timeouts) || !deserializeTimeouts(*timeouts))
                return nullptr;
            result->setValue(it->key, RefPtr<InspectorValue>(it->value));
        } else if (it->key == "unhandledPromptBehavior") {
            String unhandledPromptBehavior;
            if (!it->value->asString(unhandledPromptBehavior) || !deserializeUnhandledPromptBehavior(unhandledPromptBehavior))
                return nullptr;
            result->setString(it->key, unhandledPromptBehavior);
        } else if (it->key.find(":") != notFound) {
            if (!platformValidateCapability(it->key, it->value))
                return nullptr;
            result->setValue(it->key, RefPtr<InspectorValue>(it->value));
        }
    }
    return result;
}

RefPtr<InspectorObject> WebDriverService::mergeCapabilities(const InspectorObject& requiredCapabilities, const InspectorObject& firstMatchCapabilities) const
{
    // §7.2 Processing Capabilities.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-merging-capabilities
    RefPtr<InspectorObject> result = InspectorObject::create();
    auto requiredEnd = requiredCapabilities.end();
    for (auto it = requiredCapabilities.begin(); it != requiredEnd; ++it)
        result->setValue(it->key, RefPtr<InspectorValue>(it->value));

    auto firstMatchEnd = firstMatchCapabilities.end();
    for (auto it = firstMatchCapabilities.begin(); it != firstMatchEnd; ++it) {
        if (requiredCapabilities.find(it->key) != requiredEnd)
            return nullptr;

        result->setValue(it->key, RefPtr<InspectorValue>(it->value));
    }

    return result;
}

std::optional<String> WebDriverService::matchCapabilities(const InspectorObject& mergedCapabilities) const
{
    // §7.2 Processing Capabilities.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-matching-capabilities
    Capabilities matchedCapabilities = platformCapabilities();

    // Some capabilities like browser name and version might need to launch the browser,
    // so we only reject the known capabilities that don't match.
    auto end = mergedCapabilities.end();
    for (auto it = mergedCapabilities.begin(); it != end; ++it) {
        if (it->key == "browserName" && matchedCapabilities.browserName) {
            String browserName;
            it->value->asString(browserName);
            if (!equalIgnoringASCIICase(matchedCapabilities.browserName.value(), browserName))
                return makeString("expected browserName ", matchedCapabilities.browserName.value(), " but got ", browserName);
        } else if (it->key == "browserVersion" && matchedCapabilities.browserVersion) {
            String browserVersion;
            it->value->asString(browserVersion);
            if (!platformCompareBrowserVersions(browserVersion, matchedCapabilities.browserVersion.value()))
                return makeString("requested browserVersion is ", browserVersion, " but actual version is ", matchedCapabilities.browserVersion.value());
        } else if (it->key == "platformName" && matchedCapabilities.platformName) {
            String platformName;
            it->value->asString(platformName);
            if (!equalLettersIgnoringASCIICase(platformName, "any") && !equalIgnoringASCIICase(matchedCapabilities.platformName.value(), platformName))
                return makeString("expected platformName ", matchedCapabilities.platformName.value(), " but got ", platformName);
        } else if (it->key == "acceptInsecureCerts" && matchedCapabilities.acceptInsecureCerts) {
            bool acceptInsecureCerts;
            it->value->asBoolean(acceptInsecureCerts);
            if (acceptInsecureCerts && !matchedCapabilities.acceptInsecureCerts.value())
                return String("browser doesn't accept insecure TLS certificates");
        } else if (it->key == "proxy") {
            // FIXME: implement proxy support.
        } else if (auto errorString = platformMatchCapability(it->key, it->value))
            return errorString;
    }

    return std::nullopt;
}

RefPtr<InspectorObject> WebDriverService::processCapabilities(const InspectorObject& parameters, Function<void (CommandResult&&)>& completionHandler) const
{
    // §7.2 Processing Capabilities.
    // https://w3c.github.io/webdriver/webdriver-spec.html#processing-capabilities

    // 1. Let capabilities request be the result of getting the property "capabilities" from parameters.
    RefPtr<InspectorObject> capabilitiesObject;
    if (!parameters.getObject(ASCIILiteral("capabilities"), capabilitiesObject)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated));
        return nullptr;
    }

    // 2. Let required capabilities be the result of getting the property "alwaysMatch" from capabilities request.
    RefPtr<InspectorValue> requiredCapabilitiesValue;
    RefPtr<InspectorObject> requiredCapabilities;
    if (!capabilitiesObject->getValue(ASCIILiteral("alwaysMatch"), requiredCapabilitiesValue))
        // 2.1. If required capabilities is undefined, set the value to an empty JSON Object.
        requiredCapabilities = InspectorObject::create();
    else if (!requiredCapabilitiesValue->asObject(requiredCapabilities)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, String("alwaysMatch is invalid in capabilities")));
        return nullptr;
    }

    // 2.2. Let required capabilities be the result of trying to validate capabilities with argument required capabilities.
    requiredCapabilities = validatedCapabilities(*requiredCapabilities);
    if (!requiredCapabilities) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, String("Invalid alwaysMatch capabilities")));
        return nullptr;
    }

    // 3. Let all first match capabilities be the result of getting the property "firstMatch" from capabilities request.
    RefPtr<InspectorValue> firstMatchCapabilitiesValue;
    RefPtr<InspectorArray> firstMatchCapabilitiesList;
    if (!capabilitiesObject->getValue(ASCIILiteral("firstMatch"), firstMatchCapabilitiesValue)) {
        // 3.1. If all first match capabilities is undefined, set the value to a JSON List with a single entry of an empty JSON Object.
        firstMatchCapabilitiesList = InspectorArray::create();
        firstMatchCapabilitiesList->pushObject(InspectorObject::create());
    } else if (!firstMatchCapabilitiesValue->asArray(firstMatchCapabilitiesList)) {
        // 3.2. If all first match capabilities is not a JSON List, return error with error code invalid argument.
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, String("firstMatch is invalid in capabilities")));
        return nullptr;
    }

    // 4. Let validated first match capabilities be an empty JSON List.
    Vector<RefPtr<InspectorObject>> validatedFirstMatchCapabilitiesList;
    auto firstMatchCapabilitiesListLength = firstMatchCapabilitiesList->length();
    validatedFirstMatchCapabilitiesList.reserveInitialCapacity(firstMatchCapabilitiesListLength);
    // 5. For each first match capabilities corresponding to an indexed property in all first match capabilities.
    for (unsigned i = 0; i < firstMatchCapabilitiesListLength; ++i) {
        RefPtr<InspectorValue> firstMatchCapabilitiesValue = firstMatchCapabilitiesList->get(i);
        RefPtr<InspectorObject> firstMatchCapabilities;
        if (!firstMatchCapabilitiesValue->asObject(firstMatchCapabilities)) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, String("Invalid capabilities found in firstMatch")));
            return nullptr;
        }
        // 5.1. Let validated capabilities be the result of trying to validate capabilities with argument first match capabilities.
        firstMatchCapabilities = validatedCapabilities(*firstMatchCapabilities);
        if (!firstMatchCapabilities) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, String("Invalid firstMatch capabilities")));
            return nullptr;
        }
        // 5.2. Append validated capabilities to validated first match capabilities.
        validatedFirstMatchCapabilitiesList.uncheckedAppend(WTFMove(firstMatchCapabilities));
    }

    // 6. For each first match capabilities corresponding to an indexed property in validated first match capabilities.
    std::optional<String> errorString;
    for (auto& validatedFirstMatchCapabilies : validatedFirstMatchCapabilitiesList) {
        // 6.1. Let merged capabilities be the result of trying to merge capabilities with required capabilities and first match capabilities as arguments.
        auto mergedCapabilities = mergeCapabilities(*requiredCapabilities, *validatedFirstMatchCapabilies);
        if (!mergedCapabilities) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, String("Same capability found in firstMatch and alwaysMatch")));
            return nullptr;
        }
        // 6.2. Let matched capabilities be the result of trying to match capabilities with merged capabilities as an argument.
        errorString = matchCapabilities(*mergedCapabilities);
        if (!errorString) {
            // 6.3. If matched capabilities is not null return matched capabilities.
            return mergedCapabilities;
        }
    }

    completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, errorString ? errorString.value() : String("Invalid capabilities")));
    return nullptr;
}

void WebDriverService::newSession(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §8.1 New Session.
    // https://www.w3.org/TR/webdriver/#new-session
    auto matchedCapabilities = processCapabilities(*parameters, completionHandler);
    if (!matchedCapabilities)
        return;

    Capabilities capabilities;
    parseCapabilities(*matchedCapabilities, capabilities);
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
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, result.errorMessage()));
                return;
            }

            m_activeSession = session.get();
            m_sessions.add(session->id(), session);

            const auto& capabilities = session->capabilities();
            if (capabilities.timeouts)
                session->setTimeouts(capabilities.timeouts.value(), [](CommandResult&&) { });

            RefPtr<InspectorObject> resultObject = InspectorObject::create();
            resultObject->setString(ASCIILiteral("sessionId"), session->id());
            RefPtr<InspectorObject> capabilitiesObject = InspectorObject::create();
            if (capabilities.browserName)
                capabilitiesObject->setString(ASCIILiteral("browserName"), capabilities.browserName.value());
            if (capabilities.browserVersion)
                capabilitiesObject->setString(ASCIILiteral("browserVersion"), capabilities.browserVersion.value());
            if (capabilities.platformName)
                capabilitiesObject->setString(ASCIILiteral("platformName"), capabilities.platformName.value());
            if (capabilities.acceptInsecureCerts)
                capabilitiesObject->setBoolean(ASCIILiteral("acceptInsecureCerts"), capabilities.acceptInsecureCerts.value());
            if (capabilities.timeouts) {
                RefPtr<InspectorObject> timeoutsObject = InspectorObject::create();
                if (capabilities.timeouts.value().script)
                    timeoutsObject->setInteger(ASCIILiteral("script"), capabilities.timeouts.value().script.value().millisecondsAs<int>());
                if (capabilities.timeouts.value().pageLoad)
                    timeoutsObject->setInteger(ASCIILiteral("pageLoad"), capabilities.timeouts.value().pageLoad.value().millisecondsAs<int>());
                if (capabilities.timeouts.value().implicit)
                    timeoutsObject->setInteger(ASCIILiteral("implicit"), capabilities.timeouts.value().implicit.value().millisecondsAs<int>());
                capabilitiesObject->setObject(ASCIILiteral("timeouts"), WTFMove(timeoutsObject));
            }
            if (capabilities.pageLoadStrategy) {
                switch (capabilities.pageLoadStrategy.value()) {
                case PageLoadStrategy::None:
                    capabilitiesObject->setString(ASCIILiteral("pageLoadStrategy"), "none");
                    break;
                case PageLoadStrategy::Normal:
                    capabilitiesObject->setString(ASCIILiteral("pageLoadStrategy"), "normal");
                    break;
                case PageLoadStrategy::Eager:
                    capabilitiesObject->setString(ASCIILiteral("pageLoadStrategy"), "eager");
                    break;
                }
            }
            if (capabilities.unhandledPromptBehavior) {
                switch (capabilities.unhandledPromptBehavior.value()) {
                case UnhandledPromptBehavior::Dismiss:
                    capabilitiesObject->setString(ASCIILiteral("unhandledPromptBehavior"), "dismiss");
                    break;
                case UnhandledPromptBehavior::Accept:
                    capabilitiesObject->setString(ASCIILiteral("unhandledPromptBehavior"), "accept");
                    break;
                case UnhandledPromptBehavior::Ignore:
                    capabilitiesObject->setString(ASCIILiteral("unhandledPromptBehavior"), "ignore");
                    break;
                }
            }
            resultObject->setObject(ASCIILiteral("capabilities"), WTFMove(capabilitiesObject));
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
        completionHandler(CommandResult::success());
        return;
    }

    session->close([this, session, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (m_activeSession == session.get())
            m_activeSession = nullptr;
        completionHandler(WTFMove(result));
    });
}

void WebDriverService::setTimeouts(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §8.5 Set Timeouts.
    // https://www.w3.org/TR/webdriver/#set-timeouts
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    auto timeouts = deserializeTimeouts(*parameters);
    if (!timeouts) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    session->setTimeouts(timeouts.value(), WTFMove(completionHandler));
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

    session->waitForNavigationToComplete([session, url = WTFMove(url), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->go(url, WTFMove(completionHandler));
    });
}

void WebDriverService::getCurrentURL(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.2 Get Current URL.
    // https://www.w3.org/TR/webdriver/#get-current-url
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    session->waitForNavigationToComplete([session, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->getCurrentURL(WTFMove(completionHandler));
    });
}

void WebDriverService::back(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.3 Back.
    // https://www.w3.org/TR/webdriver/#back
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    session->waitForNavigationToComplete([session, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->back(WTFMove(completionHandler));
    });
}

void WebDriverService::forward(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.4 Forward.
    // https://www.w3.org/TR/webdriver/#forward
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    session->waitForNavigationToComplete([session, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->forward(WTFMove(completionHandler));
    });
}

void WebDriverService::refresh(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.5 Refresh.
    // https://www.w3.org/TR/webdriver/#refresh
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    session->waitForNavigationToComplete([session, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->refresh(WTFMove(completionHandler));
    });
}

void WebDriverService::getTitle(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.6 Get Title.
    // https://www.w3.org/TR/webdriver/#get-title
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    session->waitForNavigationToComplete([session, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->getTitle(WTFMove(completionHandler));
    });
}

void WebDriverService::getWindowHandle(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.1 Get Window Handle.
    // https://www.w3.org/TR/webdriver/#get-window-handle
    if (auto session = findSessionOrCompleteWithError(*parameters, completionHandler))
        session->getWindowHandle(WTFMove(completionHandler));
}

void WebDriverService::getWindowRect(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.7.1 Get Window Rect.
    // https://w3c.github.io/webdriver/webdriver-spec.html#get-window-rect
    if (auto session = findSessionOrCompleteWithError(*parameters, completionHandler))
        session->getWindowRect(WTFMove(completionHandler));
}

static std::optional<double> valueAsNumberInRange(const InspectorValue& value, double minAllowed = 0, double maxAllowed = INT_MAX)
{
    double number;
    if (!value.asDouble(number))
        return std::nullopt;

    if (std::isnan(number) || std::isinf(number))
        return std::nullopt;

    if (number < minAllowed || number > maxAllowed)
        return std::nullopt;

    return number;
}

void WebDriverService::setWindowRect(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.7.2 Set Window Rect.
    // https://w3c.github.io/webdriver/webdriver-spec.html#set-window-rect
    RefPtr<InspectorValue> value;
    std::optional<double> width;
    if (parameters->getValue(ASCIILiteral("width"), value)) {
        if (auto number = valueAsNumberInRange(*value))
            width = number;
        else if (!value->isNull()) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
            return;
        }
    }
    std::optional<double> height;
    if (parameters->getValue(ASCIILiteral("height"), value)) {
        if (auto number = valueAsNumberInRange(*value))
            height = number;
        else if (!value->isNull()) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
            return;
        }
    }
    std::optional<double> x;
    if (parameters->getValue(ASCIILiteral("x"), value)) {
        if (auto number = valueAsNumberInRange(*value, INT_MIN))
            x = number;
        else if (!value->isNull()) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
            return;
        }
    }
    std::optional<double> y;
    if (parameters->getValue(ASCIILiteral("y"), value)) {
        if (auto number = valueAsNumberInRange(*value, INT_MIN))
            y = number;
        else if (!value->isNull()) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
            return;
        }
    }

    // FIXME: If the remote end does not support the Set Window Rect command for the current
    // top-level browsing context for any reason, return error with error code unsupported operation.

    if (auto session = findSessionOrCompleteWithError(*parameters, completionHandler))
        session->setWindowRect(x, y, width, height, WTFMove(completionHandler));
}

void WebDriverService::closeWindow(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.2 Close Window.
    // https://www.w3.org/TR/webdriver/#close-window
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    session->closeWindow([this, session, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        RefPtr<InspectorArray> handles;
        if (result.result()->asArray(handles) && !handles->length()) {
            m_sessions.remove(session->id());
            if (m_activeSession == session.get())
                m_activeSession = nullptr;
        }
        completionHandler(WTFMove(result));
    });
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

    session->waitForNavigationToComplete([session, frameID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->switchToFrame(WTFMove(frameID), WTFMove(completionHandler));
    });
}

void WebDriverService::switchToParentFrame(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.6 Switch To Parent Frame.
    // https://www.w3.org/TR/webdriver/#switch-to-parent-frame
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    session->waitForNavigationToComplete([session, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->switchToParentFrame(WTFMove(completionHandler));
    });
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

    session->waitForNavigationToComplete([session, strategy = WTFMove(strategy), selector = WTFMove(selector), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->findElements(strategy, selector, Session::FindElementsMode::Single, emptyString(), WTFMove(completionHandler));
    });
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

    session->waitForNavigationToComplete([session, strategy = WTFMove(strategy), selector = WTFMove(selector), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->findElements(strategy, selector, Session::FindElementsMode::Multiple, emptyString(), WTFMove(completionHandler));
    });
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

    session->waitForNavigationToComplete([session, script = WTFMove(script), arguments = WTFMove(arguments), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->executeScript(script, WTFMove(arguments), Session::ExecuteScriptMode::Sync, WTFMove(completionHandler));
    });
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

    session->waitForNavigationToComplete([session, script = WTFMove(script), arguments = WTFMove(arguments), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->executeScript(script, WTFMove(arguments), Session::ExecuteScriptMode::Async, WTFMove(completionHandler));
    });
}

void WebDriverService::getAllCookies(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §16.1 Get All Cookies.
    // https://w3c.github.io/webdriver/webdriver-spec.html#get-all-cookies
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    session->waitForNavigationToComplete([session, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->getAllCookies(WTFMove(completionHandler));
    });
}

void WebDriverService::getNamedCookie(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §16.2 Get Named Cookie.
    // https://w3c.github.io/webdriver/webdriver-spec.html#get-named-cookie
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    String name;
    if (!parameters->getString(ASCIILiteral("name"), name)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    session->waitForNavigationToComplete([session, name = WTFMove(name), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->getNamedCookie(name, WTFMove(completionHandler));
    });
}

static std::optional<Session::Cookie> deserializeCookie(InspectorObject& cookieObject)
{
    Session::Cookie cookie;
    if (!cookieObject.getString(ASCIILiteral("name"), cookie.name) || cookie.name.isEmpty())
        return std::nullopt;
    if (!cookieObject.getString(ASCIILiteral("value"), cookie.value) || cookie.value.isEmpty())
        return std::nullopt;

    RefPtr<InspectorValue> value;
    if (cookieObject.getValue(ASCIILiteral("path"), value)) {
        String path;
        if (!value->asString(path))
            return std::nullopt;
        cookie.path = path;
    }
    if (cookieObject.getValue(ASCIILiteral("domain"), value)) {
        String domain;
        if (!value->asString(domain))
            return std::nullopt;
        cookie.domain = domain;
    }
    if (cookieObject.getValue(ASCIILiteral("secure"), value)) {
        bool secure;
        if (!value->asBoolean(secure))
            return std::nullopt;
        cookie.secure = secure;
    }
    if (cookieObject.getValue(ASCIILiteral("httpOnly"), value)) {
        bool httpOnly;
        if (!value->asBoolean(httpOnly))
            return std::nullopt;
        cookie.httpOnly = httpOnly;
    }
    if (cookieObject.getValue(ASCIILiteral("expiry"), value)) {
        int expiry;
        if (!value->asInteger(expiry) || expiry < 0 || expiry > INT_MAX)
            return std::nullopt;

        cookie.expiry = static_cast<unsigned>(expiry);
    }

    return cookie;
}

void WebDriverService::addCookie(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §16.3 Add Cookie.
    // https://w3c.github.io/webdriver/webdriver-spec.html#add-cookie
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    RefPtr<InspectorObject> cookieObject;
    if (!parameters->getObject(ASCIILiteral("cookie"), cookieObject)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    auto cookie = deserializeCookie(*cookieObject);
    if (!cookie) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    session->waitForNavigationToComplete([session, cookie = WTFMove(cookie), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->addCookie(cookie.value(), WTFMove(completionHandler));
    });
}

void WebDriverService::deleteCookie(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §16.4 Delete Cookie.
    // https://w3c.github.io/webdriver/webdriver-spec.html#delete-cookie
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    String name;
    if (!parameters->getString(ASCIILiteral("name"), name)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    session->waitForNavigationToComplete([session, name = WTFMove(name), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->deleteCookie(name, WTFMove(completionHandler));
    });
}

void WebDriverService::deleteAllCookies(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §16.5 Delete All Cookies.
    // https://w3c.github.io/webdriver/webdriver-spec.html#delete-all-cookies
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    session->waitForNavigationToComplete([session, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->deleteAllCookies(WTFMove(completionHandler));
    });
}

void WebDriverService::dismissAlert(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §18.1 Dismiss Alert.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dismiss-alert
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    session->waitForNavigationToComplete([session, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->dismissAlert(WTFMove(completionHandler));
    });
}

void WebDriverService::acceptAlert(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §18.2 Accept Alert.
    // https://w3c.github.io/webdriver/webdriver-spec.html#accept-alert
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    session->waitForNavigationToComplete([session, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->acceptAlert(WTFMove(completionHandler));
    });
}

void WebDriverService::getAlertText(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §18.3 Get Alert Text.
    // https://w3c.github.io/webdriver/webdriver-spec.html#get-alert-text
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    session->waitForNavigationToComplete([session, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->getAlertText(WTFMove(completionHandler));
    });
}

void WebDriverService::sendAlertText(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §18.4 Send Alert Text.
    // https://w3c.github.io/webdriver/webdriver-spec.html#send-alert-text
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    String text;
    if (!parameters->getString(ASCIILiteral("text"), text)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    session->waitForNavigationToComplete([session, text = WTFMove(text), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->sendAlertText(text, WTFMove(completionHandler));
    });
}

void WebDriverService::takeScreenshot(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §19.1 Take Screenshot.
    // https://w3c.github.io/webdriver/webdriver-spec.html#take-screenshot
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    session->waitForNavigationToComplete([session, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->takeScreenshot(std::nullopt, std::nullopt, WTFMove(completionHandler));
    });
}

void WebDriverService::takeElementScreenshot(RefPtr<InspectorObject>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §19.2 Take Element Screenshot.
    // https://w3c.github.io/webdriver/webdriver-spec.html#take-element-screenshot
    auto session = findSessionOrCompleteWithError(*parameters, completionHandler);
    if (!session)
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    bool scrollIntoView = true;
    parameters->getBoolean(ASCIILiteral("scroll"), scrollIntoView);

    session->waitForNavigationToComplete([session, elementID, scrollIntoView, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        session->takeScreenshot(elementID.value(), scrollIntoView, WTFMove(completionHandler));
    });
}

} // namespace WebDriver
