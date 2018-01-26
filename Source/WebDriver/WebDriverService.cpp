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
#include <wtf/RunLoop.h>
#include <wtf/text/WTFString.h>

namespace WebDriver {

// https://w3c.github.io/webdriver/webdriver-spec.html#dfn-maximum-safe-integer
static const double maxSafeInteger = 9007199254740991.0; // 2 ^ 53 - 1

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
    { HTTPMethod::Get, "/status", &WebDriverService::status },
    { HTTPMethod::Get, "/session/$sessionId/timeouts", &WebDriverService::getTimeouts },
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
    { HTTPMethod::Get, "/session/$sessionId/element/active", &WebDriverService::getActiveElement },

    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/selected", &WebDriverService::isElementSelected },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/attribute/$name", &WebDriverService::getElementAttribute },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/property/$name", &WebDriverService::getElementProperty },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/css/$name", &WebDriverService::getElementCSSValue },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/text", &WebDriverService::getElementText },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/name", &WebDriverService::getElementTagName },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/rect", &WebDriverService::getElementRect },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/enabled", &WebDriverService::isElementEnabled },

    { HTTPMethod::Post, "/session/$sessionId/element/$elementId/click", &WebDriverService::elementClick },
    { HTTPMethod::Post, "/session/$sessionId/element/$elementId/clear", &WebDriverService::elementClear },
    { HTTPMethod::Post, "/session/$sessionId/element/$elementId/value", &WebDriverService::elementSendKeys },

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

bool WebDriverService::findCommand(HTTPMethod method, const String& path, CommandHandler* handler, HashMap<String, String>& parameters)
{
    size_t length = WTF_ARRAY_LENGTH(s_commands);
    for (size_t i = 0; i < length; ++i) {
        if (s_commands[i].method != method)
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
    auto method = toCommandHTTPMethod(request.method);
    if (!method) {
        sendResponse(WTFMove(replyHandler), CommandResult::fail(CommandResult::ErrorCode::UnknownCommand, String("Unknown method: " + request.method)));
        return;
    }
    CommandHandler handler;
    HashMap<String, String> parameters;
    if (!findCommand(method.value(), request.path, &handler, parameters)) {
        sendResponse(WTFMove(replyHandler), CommandResult::fail(CommandResult::ErrorCode::UnknownCommand, String("Unknown command: " + request.path)));
        return;
    }

    RefPtr<JSON::Object> parametersObject;
    if (method.value() == HTTPMethod::Post && request.dataLength) {
        RefPtr<JSON::Value> messageValue;
        if (!JSON::Value::parseJSON(String::fromUTF8(request.data, request.dataLength), messageValue)) {
            sendResponse(WTFMove(replyHandler), CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
            return;
        }

        if (!messageValue->asObject(parametersObject)) {
            sendResponse(WTFMove(replyHandler), CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
            return;
        }
    } else
        parametersObject = JSON::Object::create();
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
    RefPtr<JSON::Value> resultValue;
    if (result.isError()) {
        // When required to send an error.
        // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-send-an-error
        // Let body be a new JSON Object initialised with the following properties: "error", "message", "stacktrace".
        auto errorObject = JSON::Object::create();
        errorObject->setString(ASCIILiteral("error"), result.errorString());
        errorObject->setString(ASCIILiteral("message"), result.errorMessage().value_or(emptyString()));
        errorObject->setString(ASCIILiteral("stacktrace"), emptyString());
        // If the error data dictionary contains any entries, set the "data" field on body to a new JSON Object populated with the dictionary.
        if (auto& additionalData = result.additionalErrorData())
            errorObject->setObject(ASCIILiteral("data"), RefPtr<JSON::Object> { additionalData });
        // Send a response with status and body as arguments.
        resultValue = WTFMove(errorObject);
    } else if (auto value = result.result())
        resultValue = WTFMove(value);
    else
        resultValue = JSON::Value::null();

    // When required to send a response.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-send-a-response
    RefPtr<JSON::Object> responseObject = JSON::Object::create();
    responseObject->setValue(ASCIILiteral("value"), WTFMove(resultValue));
    replyHandler({ result.httpStatusCode(), responseObject->toJSONString().utf8(), ASCIILiteral("application/json; charset=utf-8") });
}

static std::optional<double> valueAsNumberInRange(const JSON::Value& value, double minAllowed = 0, double maxAllowed = std::numeric_limits<int>::max())
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

static std::optional<uint64_t> unsignedValue(JSON::Value& value)
{
    auto number = valueAsNumberInRange(value, 0, maxSafeInteger);
    if (!number)
        return std::nullopt;

    auto intValue = static_cast<uint64_t>(number.value());
    // If the contained value is a double, bail in case it doesn't match the integer
    // value, i.e. if the double value was not originally in integer form.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-integer
    if (number.value() != intValue)
        return std::nullopt;

    return intValue;
}

static std::optional<Timeouts> deserializeTimeouts(JSON::Object& timeoutsObject)
{
    // §8.5 Set Timeouts.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-deserialize-as-a-timeout
    Timeouts timeouts;
    auto end = timeoutsObject.end();
    for (auto it = timeoutsObject.begin(); it != end; ++it) {
        if (it->key == "sessionId")
            continue;

        // If value is not an integer, or it is less than 0 or greater than the maximum safe integer, return error with error code invalid argument.
        auto timeoutMS = unsignedValue(*it->value);
        if (!timeoutMS)
            return std::nullopt;

        if (it->key == "script")
            timeouts.script = Seconds::fromMilliseconds(timeoutMS.value());
        else if (it->key == "pageLoad")
            timeouts.pageLoad = Seconds::fromMilliseconds(timeoutMS.value());
        else if (it->key == "implicit")
            timeouts.implicit = Seconds::fromMilliseconds(timeoutMS.value());
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
    if (unhandledPromptBehavior == "dismiss and notify")
        return UnhandledPromptBehavior::DismissAndNotify;
    if (unhandledPromptBehavior == "accept and notify")
        return UnhandledPromptBehavior::AcceptAndNotify;
    if (unhandledPromptBehavior == "ignore")
        return UnhandledPromptBehavior::Ignore;
    return std::nullopt;
}

void WebDriverService::parseCapabilities(const JSON::Object& matchedCapabilities, Capabilities& capabilities) const
{
    // Matched capabilities have already been validated.
    bool acceptInsecureCerts;
    if (matchedCapabilities.getBoolean(ASCIILiteral("acceptInsecureCerts"), acceptInsecureCerts))
        capabilities.acceptInsecureCerts = acceptInsecureCerts;
    bool setWindowRect;
    if (matchedCapabilities.getBoolean(ASCIILiteral("setWindowRect"), setWindowRect))
        capabilities.setWindowRect = setWindowRect;
    String browserName;
    if (matchedCapabilities.getString(ASCIILiteral("browserName"), browserName))
        capabilities.browserName = browserName;
    String browserVersion;
    if (matchedCapabilities.getString(ASCIILiteral("browserVersion"), browserVersion))
        capabilities.browserVersion = browserVersion;
    String platformName;
    if (matchedCapabilities.getString(ASCIILiteral("platformName"), platformName))
        capabilities.platformName = platformName;
    RefPtr<JSON::Object> timeouts;
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

bool WebDriverService::findSessionOrCompleteWithError(JSON::Object& parameters, Function<void (CommandResult&&)>& completionHandler)
{
    String sessionID;
    if (!parameters.getString(ASCIILiteral("sessionId"), sessionID)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return false;
    }

    if (!m_session || m_session->id() != sessionID) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidSessionID));
        return false;
    }

    return true;
}

RefPtr<JSON::Object> WebDriverService::validatedCapabilities(const JSON::Object& capabilities) const
{
    // §7.2 Processing Capabilities.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-validate-capabilities
    RefPtr<JSON::Object> result = JSON::Object::create();
    auto end = capabilities.end();
    for (auto it = capabilities.begin(); it != end; ++it) {
        if (it->value->isNull())
            continue;
        if (it->key == "acceptInsecureCerts") {
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
            RefPtr<JSON::Object> timeouts;
            if (!it->value->asObject(timeouts) || !deserializeTimeouts(*timeouts))
                return nullptr;
            result->setValue(it->key, RefPtr<JSON::Value>(it->value));
        } else if (it->key == "unhandledPromptBehavior") {
            String unhandledPromptBehavior;
            if (!it->value->asString(unhandledPromptBehavior) || !deserializeUnhandledPromptBehavior(unhandledPromptBehavior))
                return nullptr;
            result->setString(it->key, unhandledPromptBehavior);
        } else if (it->key.find(":") != notFound) {
            if (!platformValidateCapability(it->key, it->value))
                return nullptr;
            result->setValue(it->key, RefPtr<JSON::Value>(it->value));
        } else
            return nullptr;
    }
    return result;
}

RefPtr<JSON::Object> WebDriverService::mergeCapabilities(const JSON::Object& requiredCapabilities, const JSON::Object& firstMatchCapabilities) const
{
    // §7.2 Processing Capabilities.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-merging-capabilities
    RefPtr<JSON::Object> result = JSON::Object::create();
    auto requiredEnd = requiredCapabilities.end();
    for (auto it = requiredCapabilities.begin(); it != requiredEnd; ++it)
        result->setValue(it->key, RefPtr<JSON::Value>(it->value));

    auto firstMatchEnd = firstMatchCapabilities.end();
    for (auto it = firstMatchCapabilities.begin(); it != firstMatchEnd; ++it)
        result->setValue(it->key, RefPtr<JSON::Value>(it->value));

    return result;
}

RefPtr<JSON::Object> WebDriverService::matchCapabilities(const JSON::Object& mergedCapabilities) const
{
    // §7.2 Processing Capabilities.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-matching-capabilities
    Capabilities platformCapabilities = this->platformCapabilities();

    // Some capabilities like browser name and version might need to launch the browser,
    // so we only reject the known capabilities that don't match.
    RefPtr<JSON::Object> matchedCapabilities = JSON::Object::create();
    if (platformCapabilities.browserName)
        matchedCapabilities->setString(ASCIILiteral("browserName"), platformCapabilities.browserName.value());
    if (platformCapabilities.browserVersion)
        matchedCapabilities->setString(ASCIILiteral("browserVersion"), platformCapabilities.browserVersion.value());
    if (platformCapabilities.platformName)
        matchedCapabilities->setString(ASCIILiteral("platformName"), platformCapabilities.platformName.value());
    if (platformCapabilities.acceptInsecureCerts)
        matchedCapabilities->setBoolean(ASCIILiteral("acceptInsecureCerts"), platformCapabilities.acceptInsecureCerts.value());
    if (platformCapabilities.setWindowRect)
        matchedCapabilities->setBoolean(ASCIILiteral("setWindowRect"), platformCapabilities.setWindowRect.value());

    auto end = mergedCapabilities.end();
    for (auto it = mergedCapabilities.begin(); it != end; ++it) {
        if (it->key == "browserName" && platformCapabilities.browserName) {
            String browserName;
            it->value->asString(browserName);
            if (!equalIgnoringASCIICase(platformCapabilities.browserName.value(), browserName))
                return nullptr;
        } else if (it->key == "browserVersion" && platformCapabilities.browserVersion) {
            String browserVersion;
            it->value->asString(browserVersion);
            if (!platformCompareBrowserVersions(browserVersion, platformCapabilities.browserVersion.value()))
                return nullptr;
        } else if (it->key == "platformName" && platformCapabilities.platformName) {
            String platformName;
            it->value->asString(platformName);
            if (!equalLettersIgnoringASCIICase(platformName, "any") && platformCapabilities.platformName.value() != platformName)
                return nullptr;
        } else if (it->key == "acceptInsecureCerts" && platformCapabilities.acceptInsecureCerts) {
            bool acceptInsecureCerts;
            it->value->asBoolean(acceptInsecureCerts);
            if (acceptInsecureCerts && !platformCapabilities.acceptInsecureCerts.value())
                return nullptr;
        } else if (it->key == "proxy") {
            // FIXME: implement proxy support.
        } else if (!platformMatchCapability(it->key, it->value))
            return nullptr;
        matchedCapabilities->setValue(it->key, RefPtr<JSON::Value>(it->value));
    }

    return matchedCapabilities;
}

Vector<Capabilities> WebDriverService::processCapabilities(const JSON::Object& parameters, Function<void (CommandResult&&)>& completionHandler) const
{
    // §7.2 Processing Capabilities.
    // https://w3c.github.io/webdriver/webdriver-spec.html#processing-capabilities

    // 1. Let capabilities request be the result of getting the property "capabilities" from parameters.
    RefPtr<JSON::Object> capabilitiesObject;
    if (!parameters.getObject(ASCIILiteral("capabilities"), capabilitiesObject)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return { };
    }

    // 2. Let required capabilities be the result of getting the property "alwaysMatch" from capabilities request.
    RefPtr<JSON::Value> requiredCapabilitiesValue;
    RefPtr<JSON::Object> requiredCapabilities;
    if (!capabilitiesObject->getValue(ASCIILiteral("alwaysMatch"), requiredCapabilitiesValue))
        // 2.1. If required capabilities is undefined, set the value to an empty JSON Object.
        requiredCapabilities = JSON::Object::create();
    else if (!requiredCapabilitiesValue->asObject(requiredCapabilities)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, String("alwaysMatch is invalid in capabilities")));
        return { };
    }

    // 2.2. Let required capabilities be the result of trying to validate capabilities with argument required capabilities.
    requiredCapabilities = validatedCapabilities(*requiredCapabilities);
    if (!requiredCapabilities) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, String("Invalid alwaysMatch capabilities")));
        return { };
    }

    // 3. Let all first match capabilities be the result of getting the property "firstMatch" from capabilities request.
    RefPtr<JSON::Value> firstMatchCapabilitiesValue;
    RefPtr<JSON::Array> firstMatchCapabilitiesList;
    if (!capabilitiesObject->getValue(ASCIILiteral("firstMatch"), firstMatchCapabilitiesValue)) {
        // 3.1. If all first match capabilities is undefined, set the value to a JSON List with a single entry of an empty JSON Object.
        firstMatchCapabilitiesList = JSON::Array::create();
        firstMatchCapabilitiesList->pushObject(JSON::Object::create());
    } else if (!firstMatchCapabilitiesValue->asArray(firstMatchCapabilitiesList)) {
        // 3.2. If all first match capabilities is not a JSON List, return error with error code invalid argument.
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, String("firstMatch is invalid in capabilities")));
        return { };
    }

    // 4. Let validated first match capabilities be an empty JSON List.
    Vector<RefPtr<JSON::Object>> validatedFirstMatchCapabilitiesList;
    auto firstMatchCapabilitiesListLength = firstMatchCapabilitiesList->length();
    validatedFirstMatchCapabilitiesList.reserveInitialCapacity(firstMatchCapabilitiesListLength);
    // 5. For each first match capabilities corresponding to an indexed property in all first match capabilities.
    for (unsigned i = 0; i < firstMatchCapabilitiesListLength; ++i) {
        RefPtr<JSON::Value> firstMatchCapabilitiesValue = firstMatchCapabilitiesList->get(i);
        RefPtr<JSON::Object> firstMatchCapabilities;
        if (!firstMatchCapabilitiesValue->asObject(firstMatchCapabilities)) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, String("Invalid capabilities found in firstMatch")));
            return { };
        }
        // 5.1. Let validated capabilities be the result of trying to validate capabilities with argument first match capabilities.
        firstMatchCapabilities = validatedCapabilities(*firstMatchCapabilities);
        if (!firstMatchCapabilities) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, String("Invalid firstMatch capabilities")));
            return { };
        }

        // Validate here that firstMatchCapabilities don't shadow alwaysMatchCapabilities.
        auto requiredEnd = requiredCapabilities->end();
        auto firstMatchEnd = firstMatchCapabilities->end();
        for (auto it = firstMatchCapabilities->begin(); it != firstMatchEnd; ++it) {
            if (requiredCapabilities->find(it->key) != requiredEnd) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument,
                    makeString("Invalid firstMatch capabilities: key ", it->key, " is present in alwaysMatch")));
                return { };
            }
        }

        // 5.2. Append validated capabilities to validated first match capabilities.
        validatedFirstMatchCapabilitiesList.uncheckedAppend(WTFMove(firstMatchCapabilities));
    }

    // 6. For each first match capabilities corresponding to an indexed property in validated first match capabilities.
    Vector<Capabilities> matchedCapabilitiesList;
    matchedCapabilitiesList.reserveInitialCapacity(validatedFirstMatchCapabilitiesList.size());
    for (auto& validatedFirstMatchCapabilies : validatedFirstMatchCapabilitiesList) {
        // 6.1. Let merged capabilities be the result of trying to merge capabilities with required capabilities and first match capabilities as arguments.
        auto mergedCapabilities = mergeCapabilities(*requiredCapabilities, *validatedFirstMatchCapabilies);

        // 6.2. Let matched capabilities be the result of trying to match capabilities with merged capabilities as an argument.
        if (auto matchedCapabilities = matchCapabilities(*mergedCapabilities)) {
            // 6.3. If matched capabilities is not null return matched capabilities.
            Capabilities capabilities;
            parseCapabilities(*matchedCapabilities, capabilities);
            matchedCapabilitiesList.uncheckedAppend(WTFMove(capabilities));
        }
    }

    if (matchedCapabilitiesList.isEmpty()) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, String("Failed to match capabilities")));
        return { };
    }

    return matchedCapabilitiesList;
}

void WebDriverService::newSession(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §8.1 New Session.
    // https://www.w3.org/TR/webdriver/#new-session
    if (m_session) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, String("Maximum number of active sessions")));
        return;
    }

    auto matchedCapabilitiesList = processCapabilities(*parameters, completionHandler);
    if (matchedCapabilitiesList.isEmpty())
        return;

    // Reverse the vector to always take last item.
    matchedCapabilitiesList.reverse();
    connectToBrowser(WTFMove(matchedCapabilitiesList), WTFMove(completionHandler));
}

void WebDriverService::connectToBrowser(Vector<Capabilities>&& capabilitiesList, Function<void (CommandResult&&)>&& completionHandler)
{
    if (capabilitiesList.isEmpty()) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, String("Failed to match capabilities")));
        return;
    }

    auto sessionHost = std::make_unique<SessionHost>(capabilitiesList.takeLast());
    auto* sessionHostPtr = sessionHost.get();
    sessionHostPtr->connectToBrowser([this, capabilitiesList = WTFMove(capabilitiesList), sessionHost = WTFMove(sessionHost), completionHandler = WTFMove(completionHandler)](std::optional<String> error) mutable {
        if (error) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, makeString("Failed to connect to browser: ", error.value())));
            return;
        }

        createSession(WTFMove(capabilitiesList), WTFMove(sessionHost), WTFMove(completionHandler));
    });
}

void WebDriverService::createSession(Vector<Capabilities>&& capabilitiesList, std::unique_ptr<SessionHost>&& sessionHost, Function<void (CommandResult&&)>&& completionHandler)
{
    auto* sessionHostPtr = sessionHost.get();
    sessionHostPtr->startAutomationSession([this, capabilitiesList = WTFMove(capabilitiesList), sessionHost = WTFMove(sessionHost), completionHandler = WTFMove(completionHandler)](bool capabilitiesDidMatch, std::optional<String> errorMessage) mutable {
        if (errorMessage) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, errorMessage.value()));
            return;
        }
        if (!capabilitiesDidMatch) {
            connectToBrowser(WTFMove(capabilitiesList), WTFMove(completionHandler));
            return;
        }

        RefPtr<Session> session = Session::create(WTFMove(sessionHost));
        session->createTopLevelBrowsingContext([this, session, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
            if (result.isError()) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, result.errorMessage()));
                return;
            }

            m_session = WTFMove(session);

            RefPtr<JSON::Object> resultObject = JSON::Object::create();
            resultObject->setString(ASCIILiteral("sessionId"), m_session->id());
            RefPtr<JSON::Object> capabilitiesObject = JSON::Object::create();
            const auto& capabilities = m_session->capabilities();
            if (capabilities.browserName)
                capabilitiesObject->setString(ASCIILiteral("browserName"), capabilities.browserName.value());
            if (capabilities.browserVersion)
                capabilitiesObject->setString(ASCIILiteral("browserVersion"), capabilities.browserVersion.value());
            if (capabilities.platformName)
                capabilitiesObject->setString(ASCIILiteral("platformName"), capabilities.platformName.value());
            if (capabilities.acceptInsecureCerts)
                capabilitiesObject->setBoolean(ASCIILiteral("acceptInsecureCerts"), capabilities.acceptInsecureCerts.value());
            if (capabilities.setWindowRect)
                capabilitiesObject->setBoolean(ASCIILiteral("setWindowRect"), capabilities.setWindowRect.value());
            if (capabilities.unhandledPromptBehavior) {
                switch (capabilities.unhandledPromptBehavior.value()) {
                case UnhandledPromptBehavior::Dismiss:
                    capabilitiesObject->setString(ASCIILiteral("unhandledPromptBehavior"), "dismiss");
                    break;
                case UnhandledPromptBehavior::Accept:
                    capabilitiesObject->setString(ASCIILiteral("unhandledPromptBehavior"), "accept");
                    break;
                case UnhandledPromptBehavior::DismissAndNotify:
                    capabilitiesObject->setString(ASCIILiteral("unhandledPromptBehavior"), "dismiss and notify");
                    break;
                case UnhandledPromptBehavior::AcceptAndNotify:
                    capabilitiesObject->setString(ASCIILiteral("unhandledPromptBehavior"), "accept and notify");
                    break;
                case UnhandledPromptBehavior::Ignore:
                    capabilitiesObject->setString(ASCIILiteral("unhandledPromptBehavior"), "ignore");
                    break;
                }
            }
            switch (capabilities.pageLoadStrategy.value_or(PageLoadStrategy::Normal)) {
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
            // FIXME: implement proxy support.
            capabilitiesObject->setObject(ASCIILiteral("proxy"), JSON::Object::create());
            RefPtr<JSON::Object> timeoutsObject = JSON::Object::create();
            timeoutsObject->setInteger(ASCIILiteral("script"), m_session->scriptTimeout().millisecondsAs<int>());
            timeoutsObject->setInteger(ASCIILiteral("pageLoad"), m_session->pageLoadTimeout().millisecondsAs<int>());
            timeoutsObject->setInteger(ASCIILiteral("implicit"), m_session->implicitWaitTimeout().millisecondsAs<int>());
            capabilitiesObject->setObject(ASCIILiteral("timeouts"), WTFMove(timeoutsObject));

            resultObject->setObject(ASCIILiteral("capabilities"), WTFMove(capabilitiesObject));
            completionHandler(CommandResult::success(WTFMove(resultObject)));
        });
    });
}

void WebDriverService::deleteSession(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §8.2 Delete Session.
    // https://www.w3.org/TR/webdriver/#delete-session
    String sessionID;
    if (!parameters->getString(ASCIILiteral("sessionId"), sessionID)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    if (!m_session || m_session->id() != sessionID) {
        completionHandler(CommandResult::success());
        return;
    }

    auto session = std::exchange(m_session, nullptr);
    session->close([this, session, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        // Ignore unknown errors when closing the session if the browser is closed.
        if (result.isError() && result.errorCode() == CommandResult::ErrorCode::UnknownError && !session->isConnected())
            completionHandler(CommandResult::success());
        else
            completionHandler(WTFMove(result));
    });
}

void WebDriverService::status(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&& completionHandler)
{
    // §8.3 Status
    // https://w3c.github.io/webdriver/webdriver-spec.html#status
    auto body = JSON::Object::create();
    body->setBoolean(ASCIILiteral("ready"), !m_session);
    body->setString(ASCIILiteral("message"), m_session ? ASCIILiteral("A session already exists") : ASCIILiteral("No sessions"));
    completionHandler(CommandResult::success(WTFMove(body)));
}

void WebDriverService::getTimeouts(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §8.4 Get Timeouts.
    // https://w3c.github.io/webdriver/webdriver-spec.html#get-timeouts
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    m_session->getTimeouts(WTFMove(completionHandler));
}

void WebDriverService::setTimeouts(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §8.5 Set Timeouts.
    // https://www.w3.org/TR/webdriver/#set-timeouts
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto timeouts = deserializeTimeouts(*parameters);
    if (!timeouts) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    m_session->setTimeouts(timeouts.value(), WTFMove(completionHandler));
}

void WebDriverService::go(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.1 Go.
    // https://www.w3.org/TR/webdriver/#go
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    String url;
    if (!parameters->getString(ASCIILiteral("url"), url)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    m_session->waitForNavigationToComplete([this, url = WTFMove(url), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->go(url, WTFMove(completionHandler));
    });
}

void WebDriverService::getCurrentURL(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.2 Get Current URL.
    // https://www.w3.org/TR/webdriver/#get-current-url
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    m_session->waitForNavigationToComplete([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->getCurrentURL(WTFMove(completionHandler));
    });
}

void WebDriverService::back(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.3 Back.
    // https://www.w3.org/TR/webdriver/#back
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    m_session->waitForNavigationToComplete([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->back(WTFMove(completionHandler));
    });
}

void WebDriverService::forward(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.4 Forward.
    // https://www.w3.org/TR/webdriver/#forward
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    m_session->waitForNavigationToComplete([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->forward(WTFMove(completionHandler));
    });
}

void WebDriverService::refresh(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.5 Refresh.
    // https://www.w3.org/TR/webdriver/#refresh
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    m_session->waitForNavigationToComplete([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->refresh(WTFMove(completionHandler));
    });
}

void WebDriverService::getTitle(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.6 Get Title.
    // https://www.w3.org/TR/webdriver/#get-title
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    m_session->waitForNavigationToComplete([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->getTitle(WTFMove(completionHandler));
    });
}

void WebDriverService::getWindowHandle(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.1 Get Window Handle.
    // https://www.w3.org/TR/webdriver/#get-window-handle
    if (findSessionOrCompleteWithError(*parameters, completionHandler))
        m_session->getWindowHandle(WTFMove(completionHandler));
}

void WebDriverService::getWindowRect(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.7.1 Get Window Rect.
    // https://w3c.github.io/webdriver/webdriver-spec.html#get-window-rect
    if (findSessionOrCompleteWithError(*parameters, completionHandler))
        m_session->getWindowRect(WTFMove(completionHandler));
}

void WebDriverService::setWindowRect(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.7.2 Set Window Rect.
    // https://w3c.github.io/webdriver/webdriver-spec.html#set-window-rect
    RefPtr<JSON::Value> value;
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

    if (findSessionOrCompleteWithError(*parameters, completionHandler))
        m_session->setWindowRect(x, y, width, height, WTFMove(completionHandler));
}

void WebDriverService::closeWindow(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.2 Close Window.
    // https://www.w3.org/TR/webdriver/#close-window
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    m_session->closeWindow([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }

        RefPtr<JSON::Array> handles;
        if (result.result()->asArray(handles) && !handles->length())
            m_session = nullptr;

        completionHandler(WTFMove(result));
    });
}

void WebDriverService::switchToWindow(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.3 Switch To Window.
    // https://www.w3.org/TR/webdriver/#switch-to-window
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    String handle;
    if (!parameters->getString(ASCIILiteral("handle"), handle)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    m_session->switchToWindow(handle, WTFMove(completionHandler));
}

void WebDriverService::getWindowHandles(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.4 Get Window Handles.
    // https://www.w3.org/TR/webdriver/#get-window-handles
    if (findSessionOrCompleteWithError(*parameters, completionHandler))
        m_session->getWindowHandles(WTFMove(completionHandler));
}

void WebDriverService::switchToFrame(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.5 Switch To Frame.
    // https://www.w3.org/TR/webdriver/#switch-to-frame
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    RefPtr<JSON::Value> frameID;
    if (!parameters->getValue(ASCIILiteral("id"), frameID)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    m_session->waitForNavigationToComplete([this, frameID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->switchToFrame(WTFMove(frameID), WTFMove(completionHandler));
    });
}

void WebDriverService::switchToParentFrame(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.6 Switch To Parent Frame.
    // https://www.w3.org/TR/webdriver/#switch-to-parent-frame
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    m_session->waitForNavigationToComplete([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->switchToParentFrame(WTFMove(completionHandler));
    });
}

static std::optional<String> findElementOrCompleteWithError(JSON::Object& parameters, Function<void (CommandResult&&)>& completionHandler)
{
    String elementID;
    if (!parameters.getString(ASCIILiteral("elementId"), elementID) || elementID.isEmpty()) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return std::nullopt;
    }
    return elementID;
}

static inline bool isValidStrategy(const String& strategy)
{
    // §12.1 Locator Strategies.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-table-of-location-strategies
    return strategy == "css selector"
        || strategy == "link text"
        || strategy == "partial link text"
        || strategy == "tag name"
        || strategy == "xpath";
}

static bool findStrategyAndSelectorOrCompleteWithError(JSON::Object& parameters, Function<void (CommandResult&&)>& completionHandler, String& strategy, String& selector)
{
    if (!parameters.getString(ASCIILiteral("using"), strategy) || !isValidStrategy(strategy)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return false;
    }
    if (!parameters.getString(ASCIILiteral("value"), selector)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return false;
    }
    return true;
}

void WebDriverService::findElement(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §12.2 Find Element.
    // https://www.w3.org/TR/webdriver/#find-element
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    String strategy, selector;
    if (!findStrategyAndSelectorOrCompleteWithError(*parameters, completionHandler, strategy, selector))
        return;

    m_session->waitForNavigationToComplete([this, strategy = WTFMove(strategy), selector = WTFMove(selector), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->findElements(strategy, selector, Session::FindElementsMode::Single, emptyString(), WTFMove(completionHandler));
    });
}

void WebDriverService::findElements(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §12.3 Find Elements.
    // https://www.w3.org/TR/webdriver/#find-elements
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    String strategy, selector;
    if (!findStrategyAndSelectorOrCompleteWithError(*parameters, completionHandler, strategy, selector))
        return;

    m_session->waitForNavigationToComplete([this, strategy = WTFMove(strategy), selector = WTFMove(selector), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->findElements(strategy, selector, Session::FindElementsMode::Multiple, emptyString(), WTFMove(completionHandler));
    });
}

void WebDriverService::findElementFromElement(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §12.4 Find Element From Element.
    // https://www.w3.org/TR/webdriver/#find-element-from-element
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    String strategy, selector;
    if (!findStrategyAndSelectorOrCompleteWithError(*parameters, completionHandler, strategy, selector))
        return;

    m_session->findElements(strategy, selector, Session::FindElementsMode::Single, elementID.value(), WTFMove(completionHandler));
}

void WebDriverService::findElementsFromElement(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §12.5 Find Elements From Element.
    // https://www.w3.org/TR/webdriver/#find-elements-from-element
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    String strategy, selector;
    if (!findStrategyAndSelectorOrCompleteWithError(*parameters, completionHandler, strategy, selector))
        return;

    m_session->findElements(strategy, selector, Session::FindElementsMode::Multiple, elementID.value(), WTFMove(completionHandler));
}

void WebDriverService::getActiveElement(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §12.6 Get Active Element.
    // https://w3c.github.io/webdriver/webdriver-spec.html#get-active-element
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    m_session->waitForNavigationToComplete([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->getActiveElement(WTFMove(completionHandler));
    });
}

void WebDriverService::isElementSelected(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §13.1 Is Element Selected.
    // https://www.w3.org/TR/webdriver/#is-element-selected
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    m_session->isElementSelected(elementID.value(), WTFMove(completionHandler));
}

void WebDriverService::getElementAttribute(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §13.2 Get Element Attribute.
    // https://www.w3.org/TR/webdriver/#get-element-attribute
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    String attribute;
    if (!parameters->getString(ASCIILiteral("name"), attribute)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    m_session->getElementAttribute(elementID.value(), attribute, WTFMove(completionHandler));
}

void WebDriverService::getElementProperty(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §13.3 Get Element Property
    // https://w3c.github.io/webdriver/webdriver-spec.html#get-element-property
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    String attribute;
    if (!parameters->getString(ASCIILiteral("name"), attribute)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    m_session->getElementProperty(elementID.value(), attribute, WTFMove(completionHandler));
}

void WebDriverService::getElementCSSValue(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §13.4 Get Element CSS Value
    // https://w3c.github.io/webdriver/webdriver-spec.html#get-element-css-value
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    String cssProperty;
    if (!parameters->getString(ASCIILiteral("name"), cssProperty)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    m_session->getElementCSSValue(elementID.value(), cssProperty, WTFMove(completionHandler));
}

void WebDriverService::getElementText(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §13.5 Get Element Text.
    // https://www.w3.org/TR/webdriver/#get-element-text
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    m_session->getElementText(elementID.value(), WTFMove(completionHandler));
}

void WebDriverService::getElementTagName(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §13.6 Get Element Tag Name.
    // https://www.w3.org/TR/webdriver/#get-element-tag-name
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    m_session->getElementTagName(elementID.value(), WTFMove(completionHandler));
}

void WebDriverService::getElementRect(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §13.7 Get Element Rect.
    // https://www.w3.org/TR/webdriver/#get-element-rect
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    m_session->getElementRect(elementID.value(), WTFMove(completionHandler));
}

void WebDriverService::isElementEnabled(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §13.8 Is Element Enabled.
    // https://www.w3.org/TR/webdriver/#is-element-enabled
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    m_session->isElementEnabled(elementID.value(), WTFMove(completionHandler));
}

void WebDriverService::isElementDisplayed(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §C. Element Displayedness.
    // https://www.w3.org/TR/webdriver/#element-displayedness
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    m_session->isElementDisplayed(elementID.value(), WTFMove(completionHandler));
}

void WebDriverService::elementClick(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §14.1 Element Click.
    // https://www.w3.org/TR/webdriver/#element-click
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    m_session->elementClick(elementID.value(), WTFMove(completionHandler));
}

void WebDriverService::elementClear(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §14.2 Element Clear.
    // https://www.w3.org/TR/webdriver/#element-clear
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    m_session->elementClear(elementID.value(), WTFMove(completionHandler));
}

void WebDriverService::elementSendKeys(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §14.3 Element Send Keys.
    // https://www.w3.org/TR/webdriver/#element-send-keys
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    RefPtr<JSON::Array> valueArray;
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

    m_session->elementSendKeys(elementID.value(), WTFMove(value), WTFMove(completionHandler));
}

static bool findScriptAndArgumentsOrCompleteWithError(JSON::Object& parameters, Function<void (CommandResult&&)>& completionHandler, String& script, RefPtr<JSON::Array>& arguments)
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

void WebDriverService::executeScript(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §15.2.1 Execute Script.
    // https://www.w3.org/TR/webdriver/#execute-script
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    String script;
    RefPtr<JSON::Array> arguments;
    if (!findScriptAndArgumentsOrCompleteWithError(*parameters, completionHandler, script, arguments))
        return;

    m_session->waitForNavigationToComplete([this, script = WTFMove(script), arguments = WTFMove(arguments), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->executeScript(script, WTFMove(arguments), Session::ExecuteScriptMode::Sync, WTFMove(completionHandler));
    });
}

void WebDriverService::executeAsyncScript(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §15.2.2 Execute Async Script.
    // https://www.w3.org/TR/webdriver/#execute-async-script
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    String script;
    RefPtr<JSON::Array> arguments;
    if (!findScriptAndArgumentsOrCompleteWithError(*parameters, completionHandler, script, arguments))
        return;

    m_session->waitForNavigationToComplete([this, script = WTFMove(script), arguments = WTFMove(arguments), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->executeScript(script, WTFMove(arguments), Session::ExecuteScriptMode::Async, WTFMove(completionHandler));
    });
}

void WebDriverService::getAllCookies(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §16.1 Get All Cookies.
    // https://w3c.github.io/webdriver/webdriver-spec.html#get-all-cookies
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    m_session->waitForNavigationToComplete([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->getAllCookies(WTFMove(completionHandler));
    });
}

void WebDriverService::getNamedCookie(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §16.2 Get Named Cookie.
    // https://w3c.github.io/webdriver/webdriver-spec.html#get-named-cookie
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    String name;
    if (!parameters->getString(ASCIILiteral("name"), name)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    m_session->waitForNavigationToComplete([this, name = WTFMove(name), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->getNamedCookie(name, WTFMove(completionHandler));
    });
}

static std::optional<Session::Cookie> deserializeCookie(JSON::Object& cookieObject)
{
    Session::Cookie cookie;
    if (!cookieObject.getString(ASCIILiteral("name"), cookie.name) || cookie.name.isEmpty())
        return std::nullopt;
    if (!cookieObject.getString(ASCIILiteral("value"), cookie.value) || cookie.value.isEmpty())
        return std::nullopt;

    RefPtr<JSON::Value> value;
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
        auto expiry = unsignedValue(*value);
        if (!expiry)
            return std::nullopt;
        cookie.expiry = expiry.value();
    }

    return cookie;
}

void WebDriverService::addCookie(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §16.3 Add Cookie.
    // https://w3c.github.io/webdriver/webdriver-spec.html#add-cookie
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    RefPtr<JSON::Object> cookieObject;
    if (!parameters->getObject(ASCIILiteral("cookie"), cookieObject)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    auto cookie = deserializeCookie(*cookieObject);
    if (!cookie) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    m_session->waitForNavigationToComplete([this, cookie = WTFMove(cookie), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->addCookie(cookie.value(), WTFMove(completionHandler));
    });
}

void WebDriverService::deleteCookie(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §16.4 Delete Cookie.
    // https://w3c.github.io/webdriver/webdriver-spec.html#delete-cookie
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    String name;
    if (!parameters->getString(ASCIILiteral("name"), name)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    m_session->waitForNavigationToComplete([this, name = WTFMove(name), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->deleteCookie(name, WTFMove(completionHandler));
    });
}

void WebDriverService::deleteAllCookies(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §16.5 Delete All Cookies.
    // https://w3c.github.io/webdriver/webdriver-spec.html#delete-all-cookies
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    m_session->waitForNavigationToComplete([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->deleteAllCookies(WTFMove(completionHandler));
    });
}

void WebDriverService::dismissAlert(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §18.1 Dismiss Alert.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dismiss-alert
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    m_session->waitForNavigationToComplete([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->dismissAlert(WTFMove(completionHandler));
    });
}

void WebDriverService::acceptAlert(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §18.2 Accept Alert.
    // https://w3c.github.io/webdriver/webdriver-spec.html#accept-alert
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    m_session->waitForNavigationToComplete([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->acceptAlert(WTFMove(completionHandler));
    });
}

void WebDriverService::getAlertText(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §18.3 Get Alert Text.
    // https://w3c.github.io/webdriver/webdriver-spec.html#get-alert-text
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    m_session->waitForNavigationToComplete([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->getAlertText(WTFMove(completionHandler));
    });
}

void WebDriverService::sendAlertText(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §18.4 Send Alert Text.
    // https://w3c.github.io/webdriver/webdriver-spec.html#send-alert-text
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    String text;
    if (!parameters->getString(ASCIILiteral("text"), text)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    m_session->waitForNavigationToComplete([this, text = WTFMove(text), completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->sendAlertText(text, WTFMove(completionHandler));
    });
}

void WebDriverService::takeScreenshot(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §19.1 Take Screenshot.
    // https://w3c.github.io/webdriver/webdriver-spec.html#take-screenshot
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    m_session->waitForNavigationToComplete([this, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->takeScreenshot(std::nullopt, std::nullopt, WTFMove(completionHandler));
    });
}

void WebDriverService::takeElementScreenshot(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §19.2 Take Element Screenshot.
    // https://w3c.github.io/webdriver/webdriver-spec.html#take-element-screenshot
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    bool scrollIntoView = true;
    parameters->getBoolean(ASCIILiteral("scroll"), scrollIntoView);

    m_session->waitForNavigationToComplete([this, elementID, scrollIntoView, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->takeScreenshot(elementID.value(), scrollIntoView, WTFMove(completionHandler));
    });
}

} // namespace WebDriver
