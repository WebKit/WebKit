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
    printf("  -h,        --help         Prints this help message\n");
    printf("  -p <port>, --port=<port>  Port number the driver will use\n");
    printf("             --host=<host>  Host IP the driver will use, or either 'local' or 'all' (default: 'local')");
    printf("\n");
}

int WebDriverService::run(int argc, char** argv)
{
    String portString;
    Optional<String> host;
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

        static const unsigned hostStrLength = strlen("--host=");
        if (!strncmp(arg, "--host=", hostStrLength) && !host) {
            host = String(arg + hostStrLength);
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

    if (!m_server.listen(host, port))
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
    { HTTPMethod::Post, "/session/$sessionId/window/new", &WebDriverService::newWindow },
    { HTTPMethod::Post, "/session/$sessionId/frame", &WebDriverService::switchToFrame },
    { HTTPMethod::Post, "/session/$sessionId/frame/parent", &WebDriverService::switchToParentFrame },
    { HTTPMethod::Get, "/session/$sessionId/window/rect", &WebDriverService::getWindowRect },
    { HTTPMethod::Post, "/session/$sessionId/window/rect", &WebDriverService::setWindowRect },
    { HTTPMethod::Post, "/session/$sessionId/window/maximize", &WebDriverService::maximizeWindow },
    { HTTPMethod::Post, "/session/$sessionId/window/minimize", &WebDriverService::minimizeWindow },
    { HTTPMethod::Post, "/session/$sessionId/window/fullscreen", &WebDriverService::fullscreenWindow },

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

    { HTTPMethod::Get, "/session/$sessionId/source", &WebDriverService::getPageSource },
    { HTTPMethod::Post, "/session/$sessionId/execute/sync", &WebDriverService::executeScript },
    { HTTPMethod::Post, "/session/$sessionId/execute/async", &WebDriverService::executeAsyncScript },

    { HTTPMethod::Get, "/session/$sessionId/cookie", &WebDriverService::getAllCookies },
    { HTTPMethod::Get, "/session/$sessionId/cookie/$name", &WebDriverService::getNamedCookie },
    { HTTPMethod::Post, "/session/$sessionId/cookie", &WebDriverService::addCookie },
    { HTTPMethod::Delete, "/session/$sessionId/cookie/$name", &WebDriverService::deleteCookie },
    { HTTPMethod::Delete, "/session/$sessionId/cookie", &WebDriverService::deleteAllCookies },

    { HTTPMethod::Post, "/session/$sessionId/actions", &WebDriverService::performActions },
    { HTTPMethod::Delete, "/session/$sessionId/actions", &WebDriverService::releaseActions },

    { HTTPMethod::Post, "/session/$sessionId/alert/dismiss", &WebDriverService::dismissAlert },
    { HTTPMethod::Post, "/session/$sessionId/alert/accept", &WebDriverService::acceptAlert },
    { HTTPMethod::Get, "/session/$sessionId/alert/text", &WebDriverService::getAlertText },
    { HTTPMethod::Post, "/session/$sessionId/alert/text", &WebDriverService::sendAlertText },

    { HTTPMethod::Get, "/session/$sessionId/screenshot", &WebDriverService::takeScreenshot },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/screenshot", &WebDriverService::takeElementScreenshot },


    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/displayed", &WebDriverService::isElementDisplayed },
};

Optional<WebDriverService::HTTPMethod> WebDriverService::toCommandHTTPMethod(const String& method)
{
    auto lowerCaseMethod = method.convertToASCIILowercase();
    if (lowerCaseMethod == "get")
        return WebDriverService::HTTPMethod::Get;
    if (lowerCaseMethod == "post" || lowerCaseMethod == "put")
        return WebDriverService::HTTPMethod::Post;
    if (lowerCaseMethod == "delete")
        return WebDriverService::HTTPMethod::Delete;

    return WTF::nullopt;
}

bool WebDriverService::findCommand(HTTPMethod method, const String& path, CommandHandler* handler, HashMap<String, String>& parameters)
{
    size_t length = WTF_ARRAY_LENGTH(s_commands);
    for (size_t i = 0; i < length; ++i) {
        if (s_commands[i].method != method)
            continue;

        Vector<String> pathTokens = path.split('/');
        Vector<String> commandTokens = String::fromUTF8(s_commands[i].uriTemplate).split('/');
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
    if (method.value() == HTTPMethod::Post) {
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
        errorObject->setString("error"_s, result.errorString());
        errorObject->setString("message"_s, result.errorMessage().valueOr(emptyString()));
        errorObject->setString("stacktrace"_s, emptyString());
        // If the error data dictionary contains any entries, set the "data" field on body to a new JSON Object populated with the dictionary.
        if (auto& additionalData = result.additionalErrorData())
            errorObject->setObject("data"_s, RefPtr<JSON::Object> { additionalData });
        // Send a response with status and body as arguments.
        resultValue = WTFMove(errorObject);
    } else if (auto value = result.result())
        resultValue = WTFMove(value);
    else
        resultValue = JSON::Value::null();

    // When required to send a response.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-send-a-response
    RefPtr<JSON::Object> responseObject = JSON::Object::create();
    responseObject->setValue("value"_s, WTFMove(resultValue));
    replyHandler({ result.httpStatusCode(), responseObject->toJSONString().utf8(), "application/json; charset=utf-8"_s });
}

static Optional<double> valueAsNumberInRange(const JSON::Value& value, double minAllowed = 0, double maxAllowed = std::numeric_limits<int>::max())
{
    double number;
    if (!value.asDouble(number))
        return WTF::nullopt;

    if (std::isnan(number) || std::isinf(number))
        return WTF::nullopt;

    if (number < minAllowed || number > maxAllowed)
        return WTF::nullopt;

    return number;
}

static Optional<uint64_t> unsignedValue(JSON::Value& value)
{
    auto number = valueAsNumberInRange(value, 0, maxSafeInteger);
    if (!number)
        return WTF::nullopt;

    auto intValue = static_cast<uint64_t>(number.value());
    // If the contained value is a double, bail in case it doesn't match the integer
    // value, i.e. if the double value was not originally in integer form.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-integer
    if (number.value() != intValue)
        return WTF::nullopt;

    return intValue;
}

enum class IgnoreUnknownTimeout { No, Yes };

static Optional<Timeouts> deserializeTimeouts(JSON::Object& timeoutsObject, IgnoreUnknownTimeout ignoreUnknownTimeout)
{
    // §8.5 Set Timeouts.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-deserialize-as-a-timeout
    Timeouts timeouts;
    auto end = timeoutsObject.end();
    for (auto it = timeoutsObject.begin(); it != end; ++it) {
        if (it->key == "sessionId")
            continue;

        if (it->key == "script" && it->value->isNull()) {
            timeouts.script = std::numeric_limits<double>::infinity();
            continue;
        }

        // If value is not an integer, or it is less than 0 or greater than the maximum safe integer, return error with error code invalid argument.
        auto timeoutMS = unsignedValue(*it->value);
        if (!timeoutMS)
            return WTF::nullopt;

        if (it->key == "script")
            timeouts.script = timeoutMS.value();
        else if (it->key == "pageLoad")
            timeouts.pageLoad = timeoutMS.value();
        else if (it->key == "implicit")
            timeouts.implicit = timeoutMS.value();
        else if (ignoreUnknownTimeout == IgnoreUnknownTimeout::No)
            return WTF::nullopt;
    }
    return timeouts;
}

static Optional<Proxy> deserializeProxy(JSON::Object& proxyObject)
{
    // §7.1 Proxy.
    // https://w3c.github.io/webdriver/#proxy
    Proxy proxy;
    if (!proxyObject.getString("proxyType"_s, proxy.type))
        return WTF::nullopt;

    if (proxy.type == "direct" || proxy.type == "autodetect" || proxy.type == "system")
        return proxy;

    if (proxy.type == "pac") {
        String autoconfigURL;
        if (!proxyObject.getString("proxyAutoconfigUrl"_s, autoconfigURL))
            return WTF::nullopt;

        proxy.autoconfigURL = autoconfigURL;
        return proxy;
    }

    if (proxy.type == "manual") {
        RefPtr<JSON::Value> value;
        if (proxyObject.getValue("ftpProxy"_s, value)) {
            String ftpProxy;
            if (!value->asString(ftpProxy))
                return WTF::nullopt;

            proxy.ftpURL = URL({ }, makeString("ftp://", ftpProxy));
            if (!proxy.ftpURL->isValid())
                return WTF::nullopt;
        }
        if (proxyObject.getValue("httpProxy"_s, value)) {
            String httpProxy;
            if (!value->asString(httpProxy))
                return WTF::nullopt;

            proxy.httpURL = URL({ }, makeString("http://", httpProxy));
            if (!proxy.httpURL->isValid())
                return WTF::nullopt;
        }
        if (proxyObject.getValue("sslProxy"_s, value)) {
            String sslProxy;
            if (!value->asString(sslProxy))
                return WTF::nullopt;

            proxy.httpsURL = URL({ }, makeString("https://", sslProxy));
            if (!proxy.httpsURL->isValid())
                return WTF::nullopt;
        }
        if (proxyObject.getValue("socksProxy", value)) {
            String socksProxy;
            if (!value->asString(socksProxy))
                return WTF::nullopt;

            proxy.socksURL = URL({ }, makeString("socks://", socksProxy));
            if (!proxy.socksURL->isValid())
                return WTF::nullopt;

            RefPtr<JSON::Value> socksVersionValue;
            if (!proxyObject.getValue("socksVersion", socksVersionValue))
                return WTF::nullopt;

            auto socksVersion = unsignedValue(*socksVersionValue);
            if (!socksVersion || socksVersion.value() > 255)
                return WTF::nullopt;
            proxy.socksVersion = socksVersion.value();
        }
        if (proxyObject.getValue("noProxy"_s, value)) {
            RefPtr<JSON::Array> noProxy;
            if (!value->asArray(noProxy))
                return WTF::nullopt;

            auto noProxyLength = noProxy->length();
            for (unsigned i = 0; i < noProxyLength; ++i) {
                RefPtr<JSON::Value> addressValue = noProxy->get(i);
                String address;
                if (!addressValue->asString(address))
                    return WTF::nullopt;
                proxy.ignoreAddressList.append(WTFMove(address));
            }
        }

        return proxy;
    }

    return WTF::nullopt;
}

static Optional<PageLoadStrategy> deserializePageLoadStrategy(const String& pageLoadStrategy)
{
    if (pageLoadStrategy == "none")
        return PageLoadStrategy::None;
    if (pageLoadStrategy == "normal")
        return PageLoadStrategy::Normal;
    if (pageLoadStrategy == "eager")
        return PageLoadStrategy::Eager;
    return WTF::nullopt;
}

static Optional<UnhandledPromptBehavior> deserializeUnhandledPromptBehavior(const String& unhandledPromptBehavior)
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
    return WTF::nullopt;
}

void WebDriverService::parseCapabilities(const JSON::Object& matchedCapabilities, Capabilities& capabilities) const
{
    // Matched capabilities have already been validated.
    bool acceptInsecureCerts;
    if (matchedCapabilities.getBoolean("acceptInsecureCerts"_s, acceptInsecureCerts))
        capabilities.acceptInsecureCerts = acceptInsecureCerts;
    bool setWindowRect;
    if (matchedCapabilities.getBoolean("setWindowRect"_s, setWindowRect))
        capabilities.setWindowRect = setWindowRect;
    String browserName;
    if (matchedCapabilities.getString("browserName"_s, browserName))
        capabilities.browserName = browserName;
    String browserVersion;
    if (matchedCapabilities.getString("browserVersion"_s, browserVersion))
        capabilities.browserVersion = browserVersion;
    String platformName;
    if (matchedCapabilities.getString("platformName"_s, platformName))
        capabilities.platformName = platformName;
    RefPtr<JSON::Object> proxy;
    if (matchedCapabilities.getObject("proxy"_s, proxy))
        capabilities.proxy = deserializeProxy(*proxy);
    bool strictFileInteractability;
    if (matchedCapabilities.getBoolean("strictFileInteractability"_s, strictFileInteractability))
        capabilities.strictFileInteractability = strictFileInteractability;
    RefPtr<JSON::Object> timeouts;
    if (matchedCapabilities.getObject("timeouts"_s, timeouts))
        capabilities.timeouts = deserializeTimeouts(*timeouts, IgnoreUnknownTimeout::No);
    String pageLoadStrategy;
    if (matchedCapabilities.getString("pageLoadStrategy"_s, pageLoadStrategy))
        capabilities.pageLoadStrategy = deserializePageLoadStrategy(pageLoadStrategy);
    String unhandledPromptBehavior;
    if (matchedCapabilities.getString("unhandledPromptBehavior"_s, unhandledPromptBehavior))
        capabilities.unhandledPromptBehavior = deserializeUnhandledPromptBehavior(unhandledPromptBehavior);
    platformParseCapabilities(matchedCapabilities, capabilities);
}

bool WebDriverService::findSessionOrCompleteWithError(JSON::Object& parameters, Function<void (CommandResult&&)>& completionHandler)
{
    String sessionID;
    if (!parameters.getString("sessionId"_s, sessionID)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return false;
    }

    if (!m_session || m_session->id() != sessionID) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidSessionID));
        return false;
    }

    if (!m_session->isConnected()) {
        m_session = nullptr;
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidSessionID, String("session deleted because of page crash or hang.")));
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
            RefPtr<JSON::Object> proxy;
            if (!it->value->asObject(proxy) || !deserializeProxy(*proxy))
                return nullptr;
            result->setValue(it->key, RefPtr<JSON::Value>(it->value));
        } else if (it->key == "strictFileInteractability") {
            bool strictFileInteractability;
            if (!it->value->asBoolean(strictFileInteractability))
                return nullptr;
            result->setBoolean(it->key, strictFileInteractability);
        } else if (it->key == "timeouts") {
            RefPtr<JSON::Object> timeouts;
            if (!it->value->asObject(timeouts) || !deserializeTimeouts(*timeouts, IgnoreUnknownTimeout::No))
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
        matchedCapabilities->setString("browserName"_s, platformCapabilities.browserName.value());
    if (platformCapabilities.browserVersion)
        matchedCapabilities->setString("browserVersion"_s, platformCapabilities.browserVersion.value());
    if (platformCapabilities.platformName)
        matchedCapabilities->setString("platformName"_s, platformCapabilities.platformName.value());
    if (platformCapabilities.acceptInsecureCerts)
        matchedCapabilities->setBoolean("acceptInsecureCerts"_s, platformCapabilities.acceptInsecureCerts.value());
    if (platformCapabilities.strictFileInteractability)
        matchedCapabilities->setBoolean("strictFileInteractability"_s, platformCapabilities.strictFileInteractability.value());
    if (platformCapabilities.setWindowRect)
        matchedCapabilities->setBoolean("setWindowRect"_s, platformCapabilities.setWindowRect.value());

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
            RefPtr<JSON::Object> proxy;
            it->value->asObject(proxy);
            String proxyType;
            proxy->getString("proxyType"_s, proxyType);
            if (!platformSupportProxyType(proxyType))
                return nullptr;
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
    if (!parameters.getObject("capabilities"_s, capabilitiesObject)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return { };
    }

    // 2. Let required capabilities be the result of getting the property "alwaysMatch" from capabilities request.
    RefPtr<JSON::Value> requiredCapabilitiesValue;
    RefPtr<JSON::Object> requiredCapabilities;
    if (!capabilitiesObject->getValue("alwaysMatch"_s, requiredCapabilitiesValue))
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
    if (!capabilitiesObject->getValue("firstMatch"_s, firstMatchCapabilitiesValue)) {
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

    auto sessionHost = makeUnique<SessionHost>(capabilitiesList.takeLast());
    auto* sessionHostPtr = sessionHost.get();
    sessionHostPtr->connectToBrowser([this, capabilitiesList = WTFMove(capabilitiesList), sessionHost = WTFMove(sessionHost), completionHandler = WTFMove(completionHandler)](Optional<String> error) mutable {
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
    sessionHostPtr->startAutomationSession([this, capabilitiesList = WTFMove(capabilitiesList), sessionHost = WTFMove(sessionHost), completionHandler = WTFMove(completionHandler)](bool capabilitiesDidMatch, Optional<String> errorMessage) mutable {
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
            resultObject->setString("sessionId"_s, m_session->id());
            RefPtr<JSON::Object> capabilitiesObject = JSON::Object::create();
            const auto& capabilities = m_session->capabilities();
            capabilitiesObject->setString("browserName"_s, capabilities.browserName.valueOr(emptyString()));
            capabilitiesObject->setString("browserVersion"_s, capabilities.browserVersion.valueOr(emptyString()));
            capabilitiesObject->setString("platformName"_s, capabilities.platformName.valueOr(emptyString()));
            capabilitiesObject->setBoolean("acceptInsecureCerts"_s, capabilities.acceptInsecureCerts.valueOr(false));
            capabilitiesObject->setBoolean("strictFileInteractability"_s, capabilities.strictFileInteractability.valueOr(false));
            capabilitiesObject->setBoolean("setWindowRect"_s, capabilities.setWindowRect.valueOr(true));
            switch (capabilities.unhandledPromptBehavior.valueOr(UnhandledPromptBehavior::DismissAndNotify)) {
            case UnhandledPromptBehavior::Dismiss:
                capabilitiesObject->setString("unhandledPromptBehavior"_s, "dismiss");
                break;
            case UnhandledPromptBehavior::Accept:
                capabilitiesObject->setString("unhandledPromptBehavior"_s, "accept");
                break;
            case UnhandledPromptBehavior::DismissAndNotify:
                capabilitiesObject->setString("unhandledPromptBehavior"_s, "dismiss and notify");
                break;
            case UnhandledPromptBehavior::AcceptAndNotify:
                capabilitiesObject->setString("unhandledPromptBehavior"_s, "accept and notify");
                break;
            case UnhandledPromptBehavior::Ignore:
                capabilitiesObject->setString("unhandledPromptBehavior"_s, "ignore");
                break;
            }
            switch (capabilities.pageLoadStrategy.valueOr(PageLoadStrategy::Normal)) {
            case PageLoadStrategy::None:
                capabilitiesObject->setString("pageLoadStrategy"_s, "none");
                break;
            case PageLoadStrategy::Normal:
                capabilitiesObject->setString("pageLoadStrategy"_s, "normal");
                break;
            case PageLoadStrategy::Eager:
                capabilitiesObject->setString("pageLoadStrategy"_s, "eager");
                break;
            }
            if (!capabilities.proxy)
                capabilitiesObject->setObject("proxy"_s, JSON::Object::create());
            RefPtr<JSON::Object> timeoutsObject = JSON::Object::create();
            if (m_session->scriptTimeout() == std::numeric_limits<double>::infinity())
                timeoutsObject->setValue("script"_s, JSON::Value::null());
            else
                timeoutsObject->setDouble("script"_s, m_session->scriptTimeout());
            timeoutsObject->setDouble("pageLoad"_s, m_session->pageLoadTimeout());
            timeoutsObject->setDouble("implicit"_s, m_session->implicitWaitTimeout());
            capabilitiesObject->setObject("timeouts"_s, WTFMove(timeoutsObject));

            resultObject->setObject("capabilities"_s, WTFMove(capabilitiesObject));
            completionHandler(CommandResult::success(WTFMove(resultObject)));
        });
    });
}

void WebDriverService::deleteSession(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §8.2 Delete Session.
    // https://www.w3.org/TR/webdriver/#delete-session
    String sessionID;
    if (!parameters->getString("sessionId"_s, sessionID)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    if (!m_session || m_session->id() != sessionID) {
        completionHandler(CommandResult::success());
        return;
    }

    auto session = std::exchange(m_session, nullptr);
    session->close([session, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
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
    body->setBoolean("ready"_s, !m_session);
    body->setString("message"_s, m_session ? "A session already exists"_s : "No sessions"_s);
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

    auto timeouts = deserializeTimeouts(*parameters, IgnoreUnknownTimeout::Yes);
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
    if (!parameters->getString("url"_s, url)) {
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
    Optional<double> width;
    if (parameters->getValue("width"_s, value)) {
        if (auto number = valueAsNumberInRange(*value))
            width = number;
        else if (!value->isNull()) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
            return;
        }
    }
    Optional<double> height;
    if (parameters->getValue("height"_s, value)) {
        if (auto number = valueAsNumberInRange(*value))
            height = number;
        else if (!value->isNull()) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
            return;
        }
    }
    Optional<double> x;
    if (parameters->getValue("x"_s, value)) {
        if (auto number = valueAsNumberInRange(*value, INT_MIN))
            x = number;
        else if (!value->isNull()) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
            return;
        }
    }
    Optional<double> y;
    if (parameters->getValue("y"_s, value)) {
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

void WebDriverService::maximizeWindow(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.7.3 Maximize Window
    // https://w3c.github.io/webdriver/#maximize-window
    if (findSessionOrCompleteWithError(*parameters, completionHandler))
        m_session->maximizeWindow(WTFMove(completionHandler));
}

void WebDriverService::minimizeWindow(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.7.4 Minimize Window
    // https://w3c.github.io/webdriver/#minimize-window
    if (findSessionOrCompleteWithError(*parameters, completionHandler))
        m_session->minimizeWindow(WTFMove(completionHandler));
}

void WebDriverService::fullscreenWindow(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.7.5 Fullscreen Window
    // https://w3c.github.io/webdriver/#fullscreen-window
    if (findSessionOrCompleteWithError(*parameters, completionHandler))
        m_session->fullscreenWindow(WTFMove(completionHandler));
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
    if (!parameters->getString("handle"_s, handle)) {
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

void WebDriverService::newWindow(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §11.5 New Window
    // https://w3c.github.io/webdriver/#new-window
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    Optional<String> typeHint;
    RefPtr<JSON::Value> value;
    if (parameters->getValue("type"_s, value)) {
        String valueString;
        if (value->asString(valueString)) {
            if (valueString == "window" || valueString == "tab")
                typeHint = valueString;
        } else if (!value->isNull()) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
            return;
        }
    }

    m_session->newWindow(typeHint, WTFMove(completionHandler));
}

void WebDriverService::switchToFrame(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.5 Switch To Frame.
    // https://www.w3.org/TR/webdriver/#switch-to-frame
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    RefPtr<JSON::Value> frameID;
    if (!parameters->getValue("id"_s, frameID)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    switch (frameID->type()) {
    case JSON::Value::Type::Null:
        break;
    case JSON::Value::Type::Double:
    case JSON::Value::Type::Integer:
        if (!valueAsNumberInRange(*frameID, 0, std::numeric_limits<unsigned short>::max())) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
            return;
        }
        break;
    case JSON::Value::Type::Object: {
        RefPtr<JSON::Object> frameIDObject;
        frameID->asObject(frameIDObject);
        if (frameIDObject->find(Session::webElementIdentifier()) == frameIDObject->end()) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
            return;
        }
        break;
    }
    case JSON::Value::Type::Boolean:
    case JSON::Value::Type::String:
    case JSON::Value::Type::Array:
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

static Optional<String> findElementOrCompleteWithError(JSON::Object& parameters, Function<void (CommandResult&&)>& completionHandler)
{
    String elementID;
    if (!parameters.getString("elementId"_s, elementID) || elementID.isEmpty()) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return WTF::nullopt;
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
    if (!parameters.getString("using"_s, strategy) || !isValidStrategy(strategy)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return false;
    }
    if (!parameters.getString("value"_s, selector)) {
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
    if (!parameters->getString("name"_s, attribute)) {
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
    if (!parameters->getString("name"_s, attribute)) {
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
    if (!parameters->getString("name"_s, cssProperty)) {
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

    String text;
    if (!parameters->getString("text"_s, text) || text.isEmpty()) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return;
    }

    m_session->elementSendKeys(elementID.value(), text, WTFMove(completionHandler));
}

void WebDriverService::getPageSource(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §15.1 Getting Page Source.
    // https://w3c.github.io/webdriver/webdriver-spec.html#getting-page-source
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    m_session->getPageSource(WTFMove(completionHandler));
}

static bool findScriptAndArgumentsOrCompleteWithError(JSON::Object& parameters, Function<void (CommandResult&&)>& completionHandler, String& script, RefPtr<JSON::Array>& arguments)
{
    if (!parameters.getString("script"_s, script)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument));
        return false;
    }
    if (!parameters.getArray("args"_s, arguments)) {
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
    if (!parameters->getString("name"_s, name)) {
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

static Optional<Session::Cookie> deserializeCookie(JSON::Object& cookieObject)
{
    Session::Cookie cookie;
    if (!cookieObject.getString("name"_s, cookie.name) || cookie.name.isEmpty())
        return WTF::nullopt;
    if (!cookieObject.getString("value"_s, cookie.value) || cookie.value.isEmpty())
        return WTF::nullopt;

    RefPtr<JSON::Value> value;
    if (cookieObject.getValue("path"_s, value)) {
        String path;
        if (!value->asString(path))
            return WTF::nullopt;
        cookie.path = path;
    }
    if (cookieObject.getValue("domain"_s, value)) {
        String domain;
        if (!value->asString(domain))
            return WTF::nullopt;
        cookie.domain = domain;
    }
    if (cookieObject.getValue("secure"_s, value)) {
        bool secure;
        if (!value->asBoolean(secure))
            return WTF::nullopt;
        cookie.secure = secure;
    }
    if (cookieObject.getValue("httpOnly"_s, value)) {
        bool httpOnly;
        if (!value->asBoolean(httpOnly))
            return WTF::nullopt;
        cookie.httpOnly = httpOnly;
    }
    if (cookieObject.getValue("expiry"_s, value)) {
        auto expiry = unsignedValue(*value);
        if (!expiry)
            return WTF::nullopt;
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
    if (!parameters->getObject("cookie"_s, cookieObject)) {
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
    if (!parameters->getString("name"_s, name)) {
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

static bool processPauseAction(JSON::Object& actionItem, Action& action, Optional<String>& errorMessage)
{
    RefPtr<JSON::Value> durationValue;
    if (!actionItem.getValue("duration"_s, durationValue))
        return true;

    auto duration = unsignedValue(*durationValue);
    if (!duration) {
        errorMessage = String("The parameter 'duration' is invalid in pause action");
        return false;
    }

    action.duration = duration.value();
    return true;
}

static Optional<Action> processNullAction(const String& id, JSON::Object& actionItem, Optional<String>& errorMessage)
{
    String subtype;
    actionItem.getString("type"_s, subtype);
    if (subtype != "pause") {
        errorMessage = String("The parameter 'type' in null action is invalid or missing");
        return WTF::nullopt;
    }

    Action action(id, Action::Type::None, Action::Subtype::Pause);
    if (!processPauseAction(actionItem, action, errorMessage))
        return WTF::nullopt;

    return action;
}

static Optional<Action> processKeyAction(const String& id, JSON::Object& actionItem, Optional<String>& errorMessage)
{
    Action::Subtype actionSubtype;
    String subtype;
    actionItem.getString("type"_s, subtype);
    if (subtype == "pause")
        actionSubtype = Action::Subtype::Pause;
    else if (subtype == "keyUp")
        actionSubtype = Action::Subtype::KeyUp;
    else if (subtype == "keyDown")
        actionSubtype = Action::Subtype::KeyDown;
    else {
        errorMessage = String("The parameter 'type' of key action is invalid");
        return WTF::nullopt;
    }

    Action action(id, Action::Type::Key, actionSubtype);

    switch (actionSubtype) {
    case Action::Subtype::Pause:
        if (!processPauseAction(actionItem, action, errorMessage))
            return WTF::nullopt;
        break;
    case Action::Subtype::KeyUp:
    case Action::Subtype::KeyDown: {
        RefPtr<JSON::Value> keyValue;
        if (!actionItem.getValue("value"_s, keyValue)) {
            errorMessage = String("The paramater 'value' is missing for key up/down action");
            return WTF::nullopt;
        }
        String key;
        if (!keyValue->asString(key) || key.isEmpty()) {
            errorMessage = String("The paramater 'value' is invalid for key up/down action");
            return WTF::nullopt;
        }
        // FIXME: check single unicode code point.
        action.key = key;
        break;
    }
    case Action::Subtype::PointerUp:
    case Action::Subtype::PointerDown:
    case Action::Subtype::PointerMove:
    case Action::Subtype::PointerCancel:
        ASSERT_NOT_REACHED();
    }

    return action;
}

static MouseButton actionMouseButton(unsigned button)
{
    // MouseEvent.button
    // https://www.w3.org/TR/uievents/#ref-for-dom-mouseevent-button-1
    switch (button) {
    case 0:
        return MouseButton::Left;
    case 1:
        return MouseButton::Middle;
    case 2:
        return MouseButton::Right;
    }

    return MouseButton::None;
}

static Optional<Action> processPointerAction(const String& id, PointerParameters& parameters, JSON::Object& actionItem, Optional<String>& errorMessage)
{
    Action::Subtype actionSubtype;
    String subtype;
    actionItem.getString("type"_s, subtype);
    if (subtype == "pause")
        actionSubtype = Action::Subtype::Pause;
    else if (subtype == "pointerUp")
        actionSubtype = Action::Subtype::PointerUp;
    else if (subtype == "pointerDown")
        actionSubtype = Action::Subtype::PointerDown;
    else if (subtype == "pointerMove")
        actionSubtype = Action::Subtype::PointerMove;
    else if (subtype == "pointerCancel")
        actionSubtype = Action::Subtype::PointerCancel;
    else {
        errorMessage = String("The parameter 'type' of pointer action is invalid");
        return WTF::nullopt;
    }

    Action action(id, Action::Type::Pointer, actionSubtype);
    action.pointerType = parameters.pointerType;

    switch (actionSubtype) {
    case Action::Subtype::Pause:
        if (!processPauseAction(actionItem, action, errorMessage))
            return WTF::nullopt;
        break;
    case Action::Subtype::PointerUp:
    case Action::Subtype::PointerDown: {
        RefPtr<JSON::Value> buttonValue;
        if (!actionItem.getValue("button"_s, buttonValue)) {
            errorMessage = String("The paramater 'button' is missing for pointer up/down action");
            return WTF::nullopt;
        }
        auto button = unsignedValue(*buttonValue);
        if (!button) {
            errorMessage = String("The paramater 'button' is invalid for pointer up/down action");
            return WTF::nullopt;
        }
        action.button = actionMouseButton(button.value());
        break;
    }
    case Action::Subtype::PointerMove: {
        RefPtr<JSON::Value> durationValue;
        if (actionItem.getValue("duration"_s, durationValue)) {
            auto duration = unsignedValue(*durationValue);
            if (!duration) {
                errorMessage = String("The parameter 'duration' is invalid in pointer move action");
                return WTF::nullopt;
            }
            action.duration = duration.value();
        }

        RefPtr<JSON::Value> originValue;
        if (actionItem.getValue("origin"_s, originValue)) {
            if (originValue->type() == JSON::Value::Type::Object) {
                RefPtr<JSON::Object> originObject;
                originValue->asObject(originObject);
                String elementID;
                if (!originObject->getString(Session::webElementIdentifier(), elementID)) {
                    errorMessage = String("The parameter 'origin' is not a valid web element object in pointer move action");
                    return WTF::nullopt;
                }
                action.origin = PointerOrigin { PointerOrigin::Type::Element, elementID };
            } else {
                String origin;
                originValue->asString(origin);
                if (origin == "viewport")
                    action.origin = PointerOrigin { PointerOrigin::Type::Viewport, WTF::nullopt };
                else if (origin == "pointer")
                    action.origin = PointerOrigin { PointerOrigin::Type::Pointer, WTF::nullopt };
                else {
                    errorMessage = String("The parameter 'origin' is invalid in pointer move action");
                    return WTF::nullopt;
                }
            }
        } else
            action.origin = PointerOrigin { PointerOrigin::Type::Viewport, WTF::nullopt };

        RefPtr<JSON::Value> xValue;
        if (actionItem.getValue("x"_s, xValue)) {
            auto x = valueAsNumberInRange(*xValue, INT_MIN);
            if (!x) {
                errorMessage = String("The paramater 'x' is invalid for pointer move action");
                return WTF::nullopt;
            }
            action.x = x.value();
        }

        RefPtr<JSON::Value> yValue;
        if (actionItem.getValue("y"_s, yValue)) {
            auto y = valueAsNumberInRange(*yValue, INT_MIN);
            if (!y) {
                errorMessage = String("The paramater 'y' is invalid for pointer move action");
                return WTF::nullopt;
            }
            action.y = y.value();
        }
        break;
    }
    case Action::Subtype::PointerCancel:
        break;
    case Action::Subtype::KeyUp:
    case Action::Subtype::KeyDown:
        ASSERT_NOT_REACHED();
    }

    return action;
}

static Optional<PointerParameters> processPointerParameters(JSON::Object& actionSequence, Optional<String>& errorMessage)
{
    PointerParameters parameters;
    RefPtr<JSON::Value> parametersDataValue;
    if (!actionSequence.getValue("parameters"_s, parametersDataValue))
        return parameters;

    RefPtr<JSON::Object> parametersData;
    if (!parametersDataValue->asObject(parametersData)) {
        errorMessage = String("Action sequence pointer parameters is not an object");
        return WTF::nullopt;
    }

    String pointerType;
    if (!parametersData->getString("pointerType"_s, pointerType))
        return parameters;

    if (pointerType == "mouse")
        parameters.pointerType = PointerType::Mouse;
    else if (pointerType == "pen")
        parameters.pointerType = PointerType::Pen;
    else if (pointerType == "touch")
        parameters.pointerType = PointerType::Touch;
    else {
        errorMessage = String("The parameter 'pointerType' in action sequence pointer parameters is invalid");
        return WTF::nullopt;
    }

    return parameters;
}

static Optional<Vector<Action>> processInputActionSequence(Session& session, JSON::Value& actionSequenceValue, Optional<String>& errorMessage)
{
    RefPtr<JSON::Object> actionSequence;
    if (!actionSequenceValue.asObject(actionSequence)) {
        errorMessage = String("The action sequence is not an object");
        return WTF::nullopt;
    }

    String type;
    actionSequence->getString("type"_s, type);
    InputSource::Type inputSourceType;
    if (type == "key")
        inputSourceType = InputSource::Type::Key;
    else if (type == "pointer")
        inputSourceType = InputSource::Type::Pointer;
    else if (type == "none")
        inputSourceType = InputSource::Type::None;
    else {
        errorMessage = String("The parameter 'type' is invalid or missing in action sequence");
        return WTF::nullopt;
    }

    String id;
    if (!actionSequence->getString("id"_s, id)) {
        errorMessage = String("The parameter 'id' is invalid or missing in action sequence");
        return WTF::nullopt;
    }

    Optional<PointerParameters> parameters;
    Optional<PointerType> pointerType;
    if (inputSourceType == InputSource::Type::Pointer) {
        parameters = processPointerParameters(*actionSequence, errorMessage);
        if (!parameters)
            return WTF::nullopt;

        pointerType = parameters->pointerType;
    }

    auto& inputSource = session.getOrCreateInputSource(id, inputSourceType, pointerType);
    if (inputSource.type != inputSourceType) {
        errorMessage = String("Action sequence type doesn't match input source type");
        return WTF::nullopt;
    }

    if (inputSource.type ==  InputSource::Type::Pointer && inputSource.pointerType != pointerType) {
        errorMessage = String("Action sequence pointer type doesn't match input source pointer type");
        return WTF::nullopt;
    }

    RefPtr<JSON::Array> actionItems;
    if (!actionSequence->getArray("actions"_s, actionItems)) {
        errorMessage = String("The parameter 'actions' is invalid or not present in action sequence");
        return WTF::nullopt;
    }

    Vector<Action> actions;
    unsigned actionItemsLength = actionItems->length();
    for (unsigned i = 0; i < actionItemsLength; ++i) {
        auto actionItemValue = actionItems->get(i);
        RefPtr<JSON::Object> actionItem;
        if (!actionItemValue->asObject(actionItem)) {
            errorMessage = String("An action in action sequence is not an object");
            return WTF::nullopt;
        }

        Optional<Action> action;
        if (inputSourceType == InputSource::Type::None)
            action = processNullAction(id, *actionItem, errorMessage);
        else if (inputSourceType == InputSource::Type::Key)
            action = processKeyAction(id, *actionItem, errorMessage);
        else if (inputSourceType == InputSource::Type::Pointer)
            action = processPointerAction(id, parameters.value(), *actionItem, errorMessage);
        if (!action)
            return WTF::nullopt;

        actions.append(action.value());
    }

    return actions;
}

void WebDriverService::performActions(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §17.5 Perform Actions.
    // https://w3c.github.io/webdriver/webdriver-spec.html#perform-actions
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    RefPtr<JSON::Array> actionsArray;
    if (!parameters->getArray("actions"_s, actionsArray)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, String("The paramater 'actions' is invalid or not present")));
        return;
    }

    Optional<String> errorMessage;
    Vector<Vector<Action>> actionsByTick;
    unsigned actionsArrayLength = actionsArray->length();
    for (unsigned i = 0; i < actionsArrayLength; ++i) {
        auto actionSequence = actionsArray->get(i);
        auto inputSourceActions = processInputActionSequence(*m_session, *actionSequence, errorMessage);
        if (!inputSourceActions) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, errorMessage.value()));
            return;
        }
        for (unsigned i = 0; i < inputSourceActions->size(); ++i) {
            if (actionsByTick.size() < i + 1)
                actionsByTick.append({ });
            actionsByTick[i].append(inputSourceActions.value()[i]);
        }
    }

    m_session->performActions(WTFMove(actionsByTick), WTFMove(completionHandler));
}

void WebDriverService::releaseActions(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §17.5 Release Actions.
    // https://w3c.github.io/webdriver/webdriver-spec.html#release-actions
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    m_session->releaseActions(WTFMove(completionHandler));
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
    if (!parameters->getString("text"_s, text)) {
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
        m_session->takeScreenshot(WTF::nullopt, WTF::nullopt, WTFMove(completionHandler));
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

    m_session->waitForNavigationToComplete([this, elementID, completionHandler = WTFMove(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTFMove(result));
            return;
        }
        m_session->takeScreenshot(elementID.value(), true, WTFMove(completionHandler));
    });
}

} // namespace WebDriver
