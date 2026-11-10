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
#include "Logging.h"
#include "SessionHost.h"
#include <cmath>
#include <ranges>
#include <wtf/Compiler.h>
#include <wtf/FileSystem.h>
#include <wtf/LoggerHelper.h>
#include <wtf/MonotonicTime.h>
#include <wtf/RunLoop.h>
#include <wtf/SortedArrayMap.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/WTFString.h>

#if ENABLE(WEBDRIVER_BIDI)
#include "HTTPServer.h"
#include "WebSocketServer.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <optional>
#include <wtf/JSONValues.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UUID.h>
#endif

namespace WebDriver {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebDriverService);

// https://w3c.github.io/webdriver/webdriver-spec.html#dfn-maximum-safe-integer
static const double maxSafeInteger = 9007199254740991.0; // 2 ^ 53 - 1

WebDriverService::WebDriverService()
    : m_server(*this)
#if ENABLE(WEBDRIVER_BIDI)
    , m_bidiServer(WebSocketServer::create(*this))
    , m_browserTerminatedObserver(SessionHost::BrowserTerminatedObserver::create([weakThis = WeakPtr { *this }](StringImpl* sessionID) {
        if (CheckedPtr checkedThis = weakThis.get())
            checkedThis->onBrowserTerminated(sessionID);
    }))
#endif
{
#if ENABLE(WEBDRIVER_BIDI)
    SessionHost::addBrowserTerminatedObserver(m_browserTerminatedObserver);
#endif
}

WebDriverService::~WebDriverService()
{
#if ENABLE(WEBDRIVER_BIDI)
    SessionHost::removeBrowserTerminatedObserver(m_browserTerminatedObserver);
#endif

    // As the runloop is already gone, just explicitly cancel the pending
    // CompletionHandlers to avoid ASSERTs when they're destructed without
    // being called.
    for (auto& request : m_pendingRequests)
        request = nullptr;
}

static void printUsageStatement(const char* programName)
{
    SAFE_PRINTF("Usage: %s options\n", utf8(StringImpl::fromLatin1(programName)));
    SAFE_PRINTF("  -h,          --help             Prints this help message\n");
    SAFE_PRINTF("  -p <port>,   --port=<port>      Port number the driver will use\n");
    SAFE_PRINTF("               --host=<host>      Host IP the driver will use, or either 'local' or 'all' (default: 'local')\n");
#if !PLATFORM(COCOA)
    SAFE_PRINTF("               --allow-hosts=<host>[,<host>...]       Additional hostnames, without port, to accept in the Host header (localhost and IP addresses are always accepted)\n");
    SAFE_PRINTF("               --allow-origins=<origin>[,<origin>...] Origins, as <scheme>://<host>[:<port>], to accept in the Origin header (requests with any other Origin are rejected)\n");
    SAFE_PRINTF("  -t <ip:port> --target=<ip:port> Target IP and port\n");
#endif
#if ENABLE(WEBDRIVER_BIDI)
    SAFE_PRINTF("               --bidi-port=<port>        Port number to use for BiDi's WebSocket connections\n");
#endif
    SAFE_PRINTF("               --replace-on-new-session  Replace the existing session on new session request\n");
    SAFE_PRINTF("\nEnvironment variables:\n");
    SAFE_PRINTF("  WEBKIT_DEBUG=<channel>[,<channel>...]    Enable verbose logging for the given channels.\n");
    SAFE_PRINTF("                                           Useful channels include: SessionHost, WebDriverClassic, and WebDriverBiDi (when enabled).\n");
}

int WebDriverService::run(int argc, char** argv)
{
    RefPtr<StringImpl> portString;
    std::optional<RefPtr<StringImpl>> host;
#if !PLATFORM(COCOA)
    // minidriver has its own loopback-only checks of the Host and Origin headers.
    Vector<RefPtr<StringImpl>> allowedHosts;
    Vector<URL> allowedOrigins;
#endif
#if ENABLE(WEBDRIVER_BIDI)
    RefPtr<StringImpl> bidiPortString;
#endif
    RefPtr<StringImpl> targetString;
    if (const char* targetEnvVar = getenv("WEBDRIVER_TARGET_ADDR"))
        targetString = StringImpl::fromLatin1(targetEnvVar);
    for (int i = 1 ; i < argc; ++i) {
        auto arg = unsafeSpan(argv[i]);
        if (equalSpans(arg, "-h"_span) || equalSpans(arg, "--help"_span)) {
            printUsageStatement(argv[0]);
            return EXIT_SUCCESS;
        }

        if (equalSpans(arg, "-p"_span) && !portString) {
            if (++i == argc) {
                printUsageStatement(argv[0]);
                return EXIT_FAILURE;
            }
            portString = StringImpl::fromLatin1(argv[i]);
            continue;
        }

        static constexpr auto portArgument = "--port="_span;
        if (spanHasPrefix(arg, portArgument) && !portString) {
            portString = StringImpl::fromLatin1(arg.subspan(portArgument.size()));
            continue;
        }

        static constexpr auto hostArgument = "--host="_span;
        if (spanHasPrefix(arg, hostArgument) && !host) {
            host = StringImpl::fromLatin1(arg.subspan(hostArgument.size()));
            continue;
        }

#if !PLATFORM(COCOA)
        static constexpr auto allowHostsArgument = "--allow-hosts="_span;
        if (spanHasPrefix(arg, allowHostsArgument)) {
            allowedHosts.appendVector(split(StringImpl::fromLatin1(arg.subspan(allowHostsArgument.size())), ','));
            continue;
        }

        static constexpr auto allowOriginsArgument = "--allow-origins="_span;
        if (spanHasPrefix(arg, allowOriginsArgument)) {
            for (auto& originString : split(StringImpl::fromLatin1(arg.subspan(allowOriginsArgument.size())), ',')) {
                URL origin { originString };
                bool isOrigin = origin.isValid() && origin.protocolIsInHTTPFamily() && origin.path() == "/"_s
                    && !origin.hasQuery() && !origin.hasFragmentIdentifier() && !origin.hasCredentials();
                if (!isOrigin) {
                    SAFE_FPRINTF(stderr, "Invalid origin %s in --allow-origins, expected <scheme>://<host>[:<port>]\n", utf8(originString));
                    return EXIT_FAILURE;
                }
                allowedOrigins.append(WTF::move(origin));
            }
            continue;
        }
#endif

#if ENABLE(WEBDRIVER_BIDI)
        static constexpr auto bidiPortArgument = "--bidi-port="_span;
        if (spanHasPrefix(arg, bidiPortArgument) && !bidiPortString) {
            bidiPortString = StringImpl::fromLatin1(arg.subspan(bidiPortArgument.size()));
            continue;
        }
#endif

        if (equalSpans(arg, "-t"_span) && !targetString) {
            if (++i == argc) {
                printUsageStatement(argv[0]);
                return EXIT_FAILURE;
            }
            targetString = StringImpl::fromLatin1(argv[i]);
            continue;
        }

        static constexpr auto targetArgument = "--target="_span;
        if (spanHasPrefix(arg, targetArgument) && !targetString) {
            targetString = StringImpl::fromLatin1(arg.subspan(targetArgument.size()));
            continue;
        }

        if (equalSpans(arg, "--replace-on-new-session"_span)) {
            m_replaceOnNewSession = true;
            continue;
        }
    }

    if (!portString) {
        printUsageStatement(argv[0]);
        return EXIT_FAILURE;
    }

    if (!isEmpty(targetString)) {
        auto position = reverseFind(targetString, ':');
        if (position != notFound) {
            m_targetAddress = left(targetString, position);
            m_targetPort = parseIntegerAllowingTrailingJunk<uint16_t>(StringView { targetString }.substring(position + 1)).value_or(0);
        }
    }

    auto port = parseInteger<uint16_t>(portString);
    if (!port) {
        RELEASE_LOG_ERROR(WebDriverClassic, "Invalid port %s provided", utf8(portString));
        SAFE_FPRINTF(stderr, "Invalid port %s provided\n", utf8(portString));
        return EXIT_FAILURE;
    }

#if ENABLE(WEBDRIVER_BIDI)
    std::optional<uint16_t> bidiPort;
    if (bidiPortString) {
        bidiPort = parseInteger<uint16_t>(bidiPortString);
        if (!bidiPort) {
            RELEASE_LOG_ERROR(WebDriverBiDi, "Invalid WebSocket BiDi port %s explicitly provided. Aborting.", utf8(bidiPortString));
            SAFE_FPRINTF(stderr, "Invalid WebSocket BiDi port %s explicitly provided. Aborting.\n", utf8(bidiPortString));
            return EXIT_FAILURE;
        }
    } else {
        RELEASE_LOG_INFO(WebDriverBiDi, "No explicit WebSocket BiDi port requested. Defaulting to an OS-provided one.");
        bidiPort = 0;
    }
#endif

    WTF::initializeMainThread();

    auto hostStr = host && !!*host ? utf8(*host) : "local"_s;
    auto programName = FileSystem::lastComponentOfPathIgnoringTrailingSlash(StringImpl::fromLatin1(argv[0]));
    if (isEmpty(programName))
        programName = "WebDriver"_si;
    auto programNameStr = utf8(programName);

#if ENABLE(WEBDRIVER_BIDI)
    if (isEmpty(m_targetAddress))
        RELEASE_LOG_INFO(WebDriverClassic, "%s starting: http=%s:%d bidi=%d", programNameStr, hostStr, *port, *bidiPort);
    else
        RELEASE_LOG_INFO(WebDriverClassic, "%s starting: http=%s:%d bidi=%d target=%s:%d", programNameStr, hostStr, *port, *bidiPort, utf8(m_targetAddress), m_targetPort);
#else
    if (isEmpty(m_targetAddress))
        RELEASE_LOG_INFO(WebDriverClassic, "%s starting: http=%s:%d", programNameStr, hostStr, *port);
    else
        RELEASE_LOG_INFO(WebDriverClassic, "%s starting: http=%s:%d target=%s:%d", programNameStr, hostStr, *port, utf8(m_targetAddress), m_targetPort);
#endif

#if !PLATFORM(COCOA)
    setAllowedHosts(WTF::move(allowedHosts));
    setAllowedOrigins(WTF::move(allowedOrigins));
#endif

    if (!m_server.listen(host, *port)) {
        RELEASE_LOG_ERROR(WebDriverClassic, "Unable to listen for HTTP server at host %s and port %d", hostStr, *port);
        SAFE_FPRINTF(stderr, "FATAL: Unable to listen for HTTP server at host %s and port %d.\n", hostStr, *port);
        return EXIT_FAILURE;
    }
    RELEASE_LOG_INFO(WebDriverClassic, "Started HTTP server with host %s and port %d", hostStr, *port);
#if ENABLE(WEBDRIVER_BIDI)
    auto bidiServerURL = m_bidiServer->listen(host ? *host : nullString(), *bidiPort);
    if (!bidiServerURL) {
        RELEASE_LOG_ERROR(WebDriverBiDi, "Unable to listen for WebSocket BiDi server at host %s and port %d", hostStr, *bidiPort);
        SAFE_FPRINTF(stderr, "FATAL: Unable to listen for WebSocket BiDi server at host %s and port %d.\n", hostStr, *bidiPort);
        return EXIT_FAILURE;
    }
    RELEASE_LOG_INFO(WebDriverBiDi, "Started WebSocket BiDi server at %s", utf8(*bidiServerURL));
#endif // ENABLE(WEBDRIVER_BIDI)

    RunLoop::run();

#if ENABLE(WEBDRIVER_BIDI)
    m_bidiServer->disconnect();
#endif
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
    { HTTPMethod::Post, "/session/$sessionId/window/consume-user-activation", &WebDriverService::consumeUserActivation },

    { HTTPMethod::Post, "/session/$sessionId/element", &WebDriverService::findElement },
    { HTTPMethod::Post, "/session/$sessionId/elements", &WebDriverService::findElements },
    { HTTPMethod::Post, "/session/$sessionId/element/$elementId/element", &WebDriverService::findElementFromElement },
    { HTTPMethod::Post, "/session/$sessionId/element/$elementId/elements", &WebDriverService::findElementsFromElement },
    { HTTPMethod::Post, "/session/$sessionId/shadow/$shadowId/element", &WebDriverService::findElementFromShadowRoot },
    { HTTPMethod::Post, "/session/$sessionId/shadow/$shadowId/elements", &WebDriverService::findElementsFromShadowRoot },
    { HTTPMethod::Get, "/session/$sessionId/element/active", &WebDriverService::getActiveElement },

    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/shadow", &WebDriverService::getElementShadowRoot },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/selected", &WebDriverService::isElementSelected },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/attribute/$name", &WebDriverService::getElementAttribute },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/property/$name", &WebDriverService::getElementProperty },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/css/$name", &WebDriverService::getElementCSSValue },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/text", &WebDriverService::getElementText },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/name", &WebDriverService::getElementTagName },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/rect", &WebDriverService::getElementRect },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/enabled", &WebDriverService::isElementEnabled },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/computedrole", &WebDriverService::getComputedRole },
    { HTTPMethod::Get, "/session/$sessionId/element/$elementId/computedlabel", &WebDriverService::getComputedLabel },

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

#if ENABLE(WEBDRIVER_BIDI)
const WebDriverService::BidiCommand WebDriverService::s_bidiCommands[] = {
    { "session.status"_s, &WebDriverService::bidiSessionStatus },
};
#endif

std::optional<WebDriverService::HTTPMethod> WebDriverService::toCommandHTTPMethod(StringImpl* method)
{
    static constexpr SortedArrayMap httpMethods { WTF::toArray<std::pair<ComparableLettersLiteral, WebDriverService::HTTPMethod>>({
        { "delete"_s, WebDriverService::HTTPMethod::Delete },
        { "get"_s, WebDriverService::HTTPMethod::Get },
        { "post"_s, WebDriverService::HTTPMethod::Post },
        { "put"_s, WebDriverService::HTTPMethod::Post },
    }) };

    if (auto* methodValue = httpMethods.tryGet(method))
        return *methodValue;
    return std::nullopt;
}

bool WebDriverService::findCommand(HTTPMethod method, StringImpl* path, CommandHandler* handler, HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>>& parameters)
{
    size_t length = std::size(s_commands);
    for (size_t i = 0; i < length; ++i) {
        if (s_commands[i].method != method)
            continue;

        Vector<RefPtr<StringImpl>> pathTokens = split(path, '/');
        Vector<RefPtr<StringImpl>> commandTokens = split(StringImpl::fromUTF8(s_commands[i].uriTemplate), '/');
        if (pathTokens.size() != commandTokens.size())
            continue;

        bool allMatched = true;
        for (size_t j = 0; j < pathTokens.size() && allMatched; ++j) {
            if (codeUnitAt(commandTokens[j], 0) == '$')
                parameters.set(substring(commandTokens[j], 1), pathTokens[j]);
            else if (!equal(commandTokens[j], pathTokens[j]))
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
    Function<void (HTTPRequestHandler::Response&&)> actualReplyHandler = WTF::move(replyHandler);

#if !RELEASE_LOG_DISABLED
    if (LOG_CHANNEL(WebDriverClassic).state != WTFLogChannelState::Off) {
        RELEASE_LOG_INFO(WebDriverClassic, "HTTP request %s %s (body=%zu bytes)", utf8(request.method), utf8(request.path), request.dataLength);
        actualReplyHandler = [startTime = MonotonicTime::now(), replyHandler = WTF::move(actualReplyHandler)](HTTPRequestHandler::Response&& response) mutable {
            RELEASE_LOG_INFO(WebDriverClassic, "HTTP response %u in %.0fms", response.statusCode, (MonotonicTime::now() - startTime).milliseconds());
            replyHandler(WTF::move(response));
        };
    }
#endif

    auto method = toCommandHTTPMethod(request.method);
    if (!method) {
        sendResponse(WTF::move(actualReplyHandler), CommandResult::fail(CommandResult::ErrorCode::UnknownCommand, makeString("Unknown method: "_s, request.method)));
        return;
    }
    CommandHandler handler;
    HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> parameters;
    if (!findCommand(method.value(), request.path, &handler, parameters)) {
        sendResponse(WTF::move(actualReplyHandler), CommandResult::fail(CommandResult::ErrorCode::UnknownCommand, makeString("Unknown command: "_s, request.path)));
        return;
    }

    RefPtr<JSON::Object> parametersObject;
    if (method.value() == HTTPMethod::Post) {
        auto messageValue = JSON::Value::parseJSON(StringImpl::fromUTF8({ request.data, request.dataLength }));
        if (!messageValue) {
            sendResponse(WTF::move(actualReplyHandler), CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Invalid JSON in request body"_s));
            return;
        }

        parametersObject = messageValue->asObject();
        if (!parametersObject) {
            sendResponse(WTF::move(actualReplyHandler), CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Expected JSON object in request body"_s));
            return;
        }
    } else
        parametersObject = JSON::Object::create();
    for (const auto& parameter : parameters)
        parametersObject->setString(parameter.key, parameter.value);

    // Session-less commands should not be enqueued
    // https://www.w3.org/TR/webdriver2/#processing-model
    if (!parametersObject->getString("sessionId"_si)) {
        // When replacing an existing session, though, we should wait as
        // a normal command in order to avoid leaving the driver/browser
        // in an inconsistent state.
        if (handler != &WebDriverService::newSession || !m_replaceOnNewSession) {
            ((*this).*handler)(WTF::move(parametersObject), [this, actualReplyHandler = WTF::move(actualReplyHandler)](CommandResult&& result) mutable {
                sendResponse(WTF::move(actualReplyHandler), WTF::move(result));
            });
            return;
        }

        flushRequestQueueWithResult(CommandResult::fail(CommandResult::ErrorCode::InvalidSessionID, "Current session will be replaced."_s));
    }

    enqueueOrDispatchRequest([this, protectedThis = protect(*this), parametersObject = WTF::move(parametersObject), handler, actualReplyHandler = WTF::move(actualReplyHandler)](std::optional<CommandResult> earlyResult) mutable {
        if (earlyResult) {
            sendResponse(WTF::move(actualReplyHandler), WTF::move(earlyResult.value()));
            // No need to drive the queue forward as earlyResult is used when flushing the queue.
            return;
        }

        ((*this).*handler)(WTF::move(parametersObject), [this, protectedThis = protect(*this), actualReplyHandler = WTF::move(actualReplyHandler)](CommandResult&& result) mutable {
            sendResponse(WTF::move(actualReplyHandler), WTF::move(result));
            dispatchNextPendingRequest();
        });
    });
}

void WebDriverService::enqueueOrDispatchRequest(CompletionHandler<void(std::optional<CommandResult>)>&& request)
{
    if (!m_hasRunningRequest) {
        m_hasRunningRequest = true;
        request(std::nullopt);
        return;
    }

    // Rely on the running request to eventually pump the next event
    RELEASE_LOG(WebDriverClassic, "WebDriverService::enqueueOrDispatchRequest Already running a command, enqueuing the incoming one");
    m_pendingRequests.append(WTF::move(request));
}

void WebDriverService::dispatchNextPendingRequest()
{
    // Run on the next iteration to avoid deep recursion in the case of a
    // sequence of synchronous commands
    RunLoop::mainSingleton().dispatch([this, protectedThis = protect(*this)] {
        ASSERT(m_hasRunningRequest);
        if (m_pendingRequests.isEmpty()) {
            m_hasRunningRequest = false;
            return;
        }

        RELEASE_LOG(WebDriverClassic, "WebDriverService::dispatchNextPendingRequest dispatching next pending request");
        auto request = m_pendingRequests.takeFirst();
        m_hasRunningRequest = true;
        request(std::nullopt);
    });
}

void WebDriverService::flushRequestQueueWithResult(CommandResult&& flushResult)
{
    auto requestsToFlush = std::exchange(m_pendingRequests, { });
    for (auto& request : requestsToFlush)
        request({ flushResult });
    // Do not clear m_hasRunningRequest as a command might be in flight and we can't interrupt it.
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
        errorObject->setString("error"_si, result.errorString());
        errorObject->setString("message"_si, result.errorMessage().value_or(emptyString()));
        errorObject->setString("stacktrace"_si, emptyString());
        // If the error data dictionary contains any entries, set the "data" field on body to a new JSON Object populated with the dictionary.
        if (auto& additionalData = result.additionalErrorData())
            errorObject->setObject("data"_si, *additionalData);
        // Send a response with status and body as arguments.
        resultValue = WTF::move(errorObject);
    } else if (auto value = result.result())
        resultValue = WTF::move(value);
    else
        resultValue = JSON::Value::null();

    // When required to send a response.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-send-a-response
    auto responseObject = JSON::Object::create();
    responseObject->setValue("value"_si, resultValue.releaseNonNull());
    replyHandler({ result.httpStatusCode(), utf8(responseObject->toJSONString()), "application/json; charset=utf-8"_s });
}

#if ENABLE(WEBDRIVER_BIDI)

bool WebDriverService::acceptHandshake(HTTPRequestHandler::Request&& request)
{
    // https://w3c.github.io/webdriver-bidi/#transport
    auto& resourceName = request.path;

    auto& resources = m_bidiServer->listener()->resources;
    auto foundResource = std::ranges::find_if(resources, [&](auto& resource) {
        return equal(resource, resourceName);
    });
    if (foundResource == resources.end()) {
        RELEASE_LOG(WebDriverBiDi, "Resource name %s not found in listener's list of WebSocket resources. Rejecting handshake.", utf8(resourceName));
        return false;
    }

    if (equal(*foundResource, "/session"_s)) {
        // FIXME Add support for bidi-only sessions
        RELEASE_LOG(WebDriverBiDi, "BiDi-only sessions are not supported yet. Rejecting handshake.");
        return false;
    }

    auto sessionID = m_bidiServer->getSessionID(resourceName);
    if (!sessionID) {
        RELEASE_LOG(WebDriverBiDi, "No session ID found for resource name %s. Rejecting handshake.", utf8(resourceName));
        return false;
    }

    // FIXME Properly support multiple sessions in the future
    if (!equal(sessionID, m_session->id())) {
        RELEASE_LOG(WebDriverBiDi, "No active session found for session ID %s. Rejecting handshake.", utf8(sessionID));
        return false;
    }

    return true;
}

void WebDriverService::handleMessage(WebSocketMessageHandler::Message&& message, Function<void(WebSocketMessageHandler::Message&&)>&& completionHandler)
{
    // https://w3c.github.io/webdriver-bidi/#handle-an-incoming-message

    if (!message.connection) {
        RELEASE_LOG(WebDriverBiDi, "Incoming message without attached connection. Ignoring message.");
        completionHandler(WebSocketMessageHandler::Message::fail(CommandResult::ErrorCode::UnknownError, std::nullopt));
        return;
    }

    auto connection = message.connection;
    if (!equal(m_bidiServer->sessionID(connection), m_session->id())) {
        // FIXME Remove once we support checking static vs non-static methods https://bugs.webkit.org/show_bug.cgi?id=281721
        completionHandler(WebSocketMessageHandler::Message::fail(CommandResult::ErrorCode::InvalidSessionID, connection));
        return;
    }

    auto parsedMessageValue = JSON::Value::parseJSON(StringImpl::create((message.payload).span()).get());
    if (!parsedMessageValue) {
        RELEASE_LOG(WebDriverBiDi, "WebDriverService::handleMessage() Failed to parse incoming message");
        completionHandler(WebSocketMessageHandler::Message::fail(CommandResult::ErrorCode::InvalidArgument, message.connection));
        return;
    }

    const auto& messageObject = parsedMessageValue->asObject();
    if (!messageObject) {
        RELEASE_LOG_ERROR(WebDriverBiDi, "WebDriver handle BiDi message: Expected object.");
        completionHandler(WebSocketMessageHandler::Message::fail(CommandResult::ErrorCode::InvalidArgument, connection));
        return;
    }

    std::optional<int> commandId = messageObject->getInteger("id"_si);
    if (!commandId) {
        RELEASE_LOG_ERROR(WebDriverBiDi, "Missing command ID.");
        completionHandler(WebSocketMessageHandler::Message::fail(CommandResult::ErrorCode::InvalidArgument, connection, "Missing command id"_s));
        return;
    }

    BidiCommandHandler handler;
    RefPtr<JSON::Object> parameters;
    // FIXME Maybe replace manual method dispatch with generated dispatchers for static methods, like we do in WebDriverBidiProcessor-related classes in the UIProcess
    // https://bugs.webkit.org/show_bug.cgi?id=281721
    if (!findBidiCommand(messageObject, &handler, parameters)) {
        RELEASE_LOG(WebDriverBiDi, "Failed to find appropriate BiDi command on WebDriver service. Relaying to the browser.");
        auto sessionID = m_session->id();
        m_session->relayBidiCommand(makeString(message.payload), *commandId, [completionHandler = WTF::move(completionHandler), sessionID, this](WebSocketMessageHandler::Message&& resultMessage) {
            auto connection = m_bidiServer->connection(sessionID);
            if (!connection) {
                RELEASE_LOG(WebDriverBiDi, "Failed to find connection for session ID %s. Ignoring message.", utf8(sessionID));
                return;
            }
            resultMessage.connection = *connection;
            completionHandler(WTF::move(resultMessage));
        });
        return;
    }

    ((*this).*handler)(*commandId, WTF::move(parameters), [completionHandler = WTF::move(completionHandler), message](WebSocketMessageHandler::Message&& resultMessage) {
        // 6.7.5 If method is "session.new", let session be the entry in the list of active sessions whose session ID is equal to the "sessionId" property of value, append connection to session’s session WebSocket connections, and remove connection from the WebSocket connections not associated with a session.
        // FIXME https://bugs.webkit.org/show_bug.cgi?id=281722
        resultMessage.connection = message.connection;
        completionHandler(WTF::move(resultMessage));
    });
}

bool WebDriverService::findBidiCommand(const RefPtr<JSON::Object>& parameters, BidiCommandHandler* handler, RefPtr<JSON::Object>& parsedParams)
{
    const RefPtr<StringImpl>& method = parameters->getString("method"_si);
    if (!method)
        return false;

    auto candidate = std::find_if(std::begin(s_bidiCommands), std::end(s_bidiCommands),
        [method](const BidiCommand& command) {
            return equal(method, command.method);
    });

    if (candidate == std::end(s_bidiCommands))
        return false;

    parsedParams = parameters->getObject("params"_si);
    *handler = candidate->handler;
    return true;
}

#endif // ENABLE(WEBDRIVER_BIDI)

static std::optional<double> valueAsNumberInRange(const JSON::Value& value, double minAllowed = 0, double maxAllowed = std::numeric_limits<int>::max())
{
    auto number = value.asDouble();
    if (!number)
        return std::nullopt;

    if (std::isnan(*number) || std::isinf(*number))
        return std::nullopt;

    if (*number < minAllowed || *number > maxAllowed)
        return std::nullopt;

    return *number;
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

enum class IgnoreUnknownTimeout : bool { No, Yes };

static std::optional<Timeouts> deserializeTimeouts(JSON::Object& timeoutsObject, IgnoreUnknownTimeout ignoreUnknownTimeout)
{
    // §8.5 Set Timeouts.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-deserialize-as-a-timeout
    Timeouts timeouts;
    auto end = timeoutsObject.end();
    for (auto it = timeoutsObject.begin(); it != end; ++it) {
        if (equal(it->key, "sessionId"_s))
            continue;

        if (equal(it->key, "script"_s) && it->value->isNull()) {
            timeouts.script = std::numeric_limits<double>::infinity();
            continue;
        }

        // If value is not an integer, or it is less than 0 or greater than the maximum safe integer, return error with error code invalid argument.
        auto timeoutMS = unsignedValue(it->value);
        if (!timeoutMS)
            return std::nullopt;

        if (equal(it->key, "script"_s))
            timeouts.script = timeoutMS.value();
        else if (equal(it->key, "pageLoad"_s))
            timeouts.pageLoad = timeoutMS.value();
        else if (equal(it->key, "implicit"_s))
            timeouts.implicit = timeoutMS.value();
        else if (ignoreUnknownTimeout == IgnoreUnknownTimeout::No)
            return std::nullopt;
    }
    return timeouts;
}

static std::optional<Proxy> deserializeProxy(JSON::Object& proxyObject)
{
    // §7.1 Proxy.
    // https://w3c.github.io/webdriver/#proxy
    Proxy proxy;

    proxy.type = proxyObject.getString("proxyType"_si);
    if (!proxy.type)
        return std::nullopt;

    if (equal(proxy.type, "direct"_s) || equal(proxy.type, "autodetect"_s) || equal(proxy.type, "system"_s))
        return proxy;

    if (equal(proxy.type, "pac"_s)) {
        auto autoconfigURL = proxyObject.getString("proxyAutoconfigUrl"_si);
        if (!autoconfigURL)
            return std::nullopt;

        proxy.autoconfigURL = URL({ }, autoconfigURL);
        if (!proxy.autoconfigURL->isValid())
            return std::nullopt;

        return proxy;
    }

    if (equal(proxy.type, "manual"_s)) {
        if (auto value = proxyObject.getValue("ftpProxy"_si)) {
            auto ftpProxy = value->asString();
            if (!ftpProxy)
                return std::nullopt;

            proxy.ftpURL = URL({ }, makeString("ftp://"_s, ftpProxy));
            if (!proxy.ftpURL->isValid())
                return std::nullopt;
        }
        if (auto value = proxyObject.getValue("httpProxy"_si)) {
            auto httpProxy = value->asString();
            if (!httpProxy)
                return std::nullopt;

            proxy.httpURL = URL({ }, makeString("http://"_s, httpProxy));
            if (!proxy.httpURL->isValid())
                return std::nullopt;
        }
        if (auto value = proxyObject.getValue("sslProxy"_si)) {
            auto sslProxy = value->asString();
            if (!sslProxy)
                return std::nullopt;

            proxy.httpsURL = URL({ }, makeString("https://"_s, sslProxy));
            if (!proxy.httpsURL->isValid())
                return std::nullopt;
        }
        if (auto value = proxyObject.getValue("socksProxy"_si)) {
            auto socksProxy = value->asString();
            if (!socksProxy)
                return std::nullopt;

            proxy.socksURL = URL({ }, makeString("socks://"_s, socksProxy));
            if (!proxy.socksURL->isValid())
                return std::nullopt;

            auto socksVersionValue = proxyObject.getValue("socksVersion"_si);
            if (!socksVersionValue)
                return std::nullopt;

            auto socksVersion = unsignedValue(*socksVersionValue);
            if (!socksVersion || socksVersion.value() > 255)
                return std::nullopt;
            proxy.socksVersion = socksVersion.value();
        }
        if (auto value = proxyObject.getValue("noProxy"_si)) {
            auto noProxy = value->asArray();
            if (!noProxy)
                return std::nullopt;

            auto noProxyLength = noProxy->length();
            for (unsigned i = 0; i < noProxyLength; ++i) {
                auto address = noProxy->get(i)->asString();
                if (!address)
                    return std::nullopt;
                proxy.ignoreAddressList.append(address);
            }
        }

        return proxy;
    }

    return std::nullopt;
}

static std::optional<PageLoadStrategy> deserializePageLoadStrategy(StringImpl* pageLoadStrategy)
{
    if (equal(pageLoadStrategy, "none"_s))
        return PageLoadStrategy::None;
    if (equal(pageLoadStrategy, "normal"_s))
        return PageLoadStrategy::Normal;
    if (equal(pageLoadStrategy, "eager"_s))
        return PageLoadStrategy::Eager;
    return std::nullopt;
}

static std::optional<PromptHandlerConfiguration> deserializePromptHandlerConfiguration(StringImpl* handler)
{
    // https://w3c.github.io/webdriver/#dfn-known-prompt-handlers
    if (equal(handler, "dismiss"_s))
        return PromptHandlerConfiguration { PromptHandlerType::Dismiss, false };
    if (equal(handler, "accept"_s))
        return PromptHandlerConfiguration { PromptHandlerType::Accept, false };
    if (equal(handler, "dismiss and notify"_s))
        return PromptHandlerConfiguration { PromptHandlerType::Dismiss, true };
    if (equal(handler, "accept and notify"_s))
        return PromptHandlerConfiguration { PromptHandlerType::Accept, true };
    if (equal(handler, "ignore"_s))
        return PromptHandlerConfiguration { PromptHandlerType::Ignore, true };
    return std::nullopt;
}

static std::optional<UserPromptHandler> deserializeUnhandledPromptBehavior(const JSON::Value& value)
{
    // https://w3c.github.io/webdriver/#dfn-deserialize-as-an-unhandled-prompt-behavior
    UserPromptHandler userPromptHandler;

    if (auto handler = value.asString(); !!handler) {
        userPromptHandler.fallbackDefault = deserializePromptHandlerConfiguration(handler);
        if (!userPromptHandler.fallbackDefault)
            return std::nullopt;
        return userPromptHandler;
    }

    auto handlers = value.asObject();
    if (!handlers)
        return std::nullopt;

    for (auto& [promptType, handlerValue] : *handlers) {
        auto handler = handlerValue->asString();
        if (!handler)
            return std::nullopt;
        auto configuration = deserializePromptHandlerConfiguration(handler);
        if (!configuration)
            return std::nullopt;

        if (promptType == "alert"_s)
            userPromptHandler.alert = configuration;
        else if (promptType == "beforeUnload"_s)
            userPromptHandler.beforeUnload = configuration;
        else if (promptType == "confirm"_s)
            userPromptHandler.confirm = configuration;
        else if (promptType == "default"_s)
            userPromptHandler.defaultHandler = configuration;
        else if (promptType == "file"_s)
            userPromptHandler.file = configuration;
        else if (promptType == "prompt"_s)
            userPromptHandler.prompt = configuration;
        else
            return std::nullopt;
    }

    return userPromptHandler;
}

static RefPtr<StringImpl> serializePromptHandlerConfiguration(const PromptHandlerConfiguration& configuration)
{
    // https://w3c.github.io/webdriver/#dfn-serialize-a-prompt-handler-configuration
    switch (configuration.handler) {
    case PromptHandlerType::Accept:
        return configuration.notify ? "accept and notify"_si : "accept"_si;
    case PromptHandlerType::Dismiss:
        return configuration.notify ? "dismiss and notify"_si : "dismiss"_si;
    case PromptHandlerType::Ignore:
        return "ignore"_si;
    }

    ASSERT_NOT_REACHED();
    return "ignore"_si;
}

static Ref<JSON::Value> serializeUserPromptHandler(const std::optional<UserPromptHandler>& userPromptHandler)
{
    // https://w3c.github.io/webdriver/#dfn-serialize-the-user-prompt-handler
    if (!userPromptHandler)
        return JSON::Value::create("dismiss and notify"_si);

    if (userPromptHandler->fallbackDefault)
        return JSON::Value::create(serializePromptHandlerConfiguration(*userPromptHandler->fallbackDefault));

    auto serialized = JSON::Object::create();
    auto serializeIfPresent = [&](ASCIILiteral promptType, const std::optional<PromptHandlerConfiguration>& configuration) {
        if (configuration)
            serialized->setString(StringImpl::create(promptType).ptr(), serializePromptHandlerConfiguration(*configuration).get());
    };
    serializeIfPresent("alert"_s, userPromptHandler->alert);
    serializeIfPresent("beforeUnload"_s, userPromptHandler->beforeUnload);
    serializeIfPresent("confirm"_s, userPromptHandler->confirm);
    serializeIfPresent("default"_s, userPromptHandler->defaultHandler);
    serializeIfPresent("file"_s, userPromptHandler->file);
    serializeIfPresent("prompt"_s, userPromptHandler->prompt);
    return serialized;
}

void WebDriverService::parseCapabilities(const JSON::Object& matchedCapabilities, Capabilities& capabilities) const
{
    // Matched capabilities have already been validated.
    auto acceptInsecureCerts = matchedCapabilities.getBoolean("acceptInsecureCerts"_si);
    if (acceptInsecureCerts)
        capabilities.acceptInsecureCerts = *acceptInsecureCerts;

    auto setWindowRect = matchedCapabilities.getBoolean("setWindowRect"_si);
    if (setWindowRect)
        capabilities.setWindowRect = *setWindowRect;

    auto browserName = matchedCapabilities.getString("browserName"_si);
    if (!!browserName)
        capabilities.browserName = browserName;

    auto browserVersion = matchedCapabilities.getString("browserVersion"_si);
    if (!!browserVersion)
        capabilities.browserVersion = browserVersion;

    auto platformName = matchedCapabilities.getString("platformName"_si);
    if (!!platformName)
        capabilities.platformName = platformName;

    auto proxy = matchedCapabilities.getObject("proxy"_si);
    if (proxy)
        capabilities.proxy = deserializeProxy(*proxy);

    auto strictFileInteractability = matchedCapabilities.getBoolean("strictFileInteractability"_si);
    if (strictFileInteractability)
        capabilities.strictFileInteractability = *strictFileInteractability;

    auto timeouts = matchedCapabilities.getObject("timeouts"_si);
    if (timeouts)
        capabilities.timeouts = deserializeTimeouts(*timeouts, IgnoreUnknownTimeout::No);

    auto pageLoadStrategy = matchedCapabilities.getString("pageLoadStrategy"_si);
    if (!!pageLoadStrategy)
        capabilities.pageLoadStrategy = deserializePageLoadStrategy(pageLoadStrategy);

    if (auto unhandledPromptBehavior = matchedCapabilities.getValue("unhandledPromptBehavior"_si))
        capabilities.unhandledPromptBehavior = deserializeUnhandledPromptBehavior(*unhandledPromptBehavior);

    if (auto webSocketURL = matchedCapabilities.getBoolean("webSocketUrl"_si))
        capabilities.webSocketURL = *webSocketURL;

    platformParseCapabilities(matchedCapabilities, capabilities);
}

bool WebDriverService::findSessionOrCompleteWithError(JSON::Object& parameters, Function<void (CommandResult&&)>& completionHandler)
{
    auto sessionID = parameters.getString("sessionId"_si);
    if (!sessionID) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Missing session ID parameter"_s));
        return false;
    }

    if (!m_session || !equal(m_session->id(), sessionID)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidSessionID));
        return false;
    }

    if (!m_session->isConnected()) {
        discardSessionIfDisconnected();
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidSessionID, "session deleted because of page crash or hang."_si));
        return false;
    }

    return true;
}

void WebDriverService::discardSessionIfDisconnected()
{
    // The browser can go away without the session being deleted, for example when it crashes or
    // when the user quits it. Such a session must not prevent creating a new one.
    if (!m_session || m_session->isConnected())
        return;

#if ENABLE(WEBDRIVER_BIDI)
    // Close the session's BiDi connection first, as when the browser terminates. The handlers of
    // BiDi messages and of closed connections expect every connection to belong to the session.
    onBrowserTerminated(protect(m_session->id()).get());
#endif
    m_session = nullptr;
}

RefPtr<JSON::Object> WebDriverService::validatedCapabilities(const JSON::Object& capabilities) const
{
    // §7.2 Processing Capabilities.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-validate-capabilities
    auto result = JSON::Object::create();
    auto end = capabilities.end();
    for (auto it = capabilities.begin(); it != end; ++it) {
        if (it->value->isNull())
            continue;
        if (equal(it->key, "acceptInsecureCerts"_s)) {
            auto acceptInsecureCerts = it->value->asBoolean();
            if (!acceptInsecureCerts)
                return nullptr;
            result->setBoolean(it->key, *acceptInsecureCerts);
        } else if (equal(it->key, "browserName"_s) || equal(it->key, "browserVersion"_s) || equal(it->key, "platformName"_s)) {
            auto stringValue = it->value->asString();
            if (!stringValue)
                return nullptr;
            result->setString(it->key, stringValue);
        } else if (equal(it->key, "pageLoadStrategy"_s)) {
            auto pageLoadStrategy = it->value->asString();
            if (!pageLoadStrategy || !deserializePageLoadStrategy(pageLoadStrategy))
                return nullptr;
            result->setString(it->key, pageLoadStrategy);
        } else if (equal(it->key, "proxy"_s)) {
            auto proxy = it->value->asObject();
            if (!proxy || !deserializeProxy(*proxy))
                return nullptr;
            result->setValue(it->key, *proxy);
        } else if (equal(it->key, "strictFileInteractability"_s)) {
            auto strictFileInteractability = it->value->asBoolean();
            if (!strictFileInteractability)
                return nullptr;
            result->setBoolean(it->key, *strictFileInteractability);
        } else if (equal(it->key, "timeouts"_s)) {
            auto timeouts = it->value->asObject();
            if (!timeouts || !deserializeTimeouts(*timeouts, IgnoreUnknownTimeout::No))
                return nullptr;
            result->setValue(it->key, *timeouts);
        } else if (equal(it->key, "unhandledPromptBehavior"_s)) {
            if (!deserializeUnhandledPromptBehavior(it->value))
                return nullptr;
            result->setValue(it->key, it->value.copyRef());
        } else if (find(it->key, ':') != notFound) {
            if (!platformValidateCapability(it->key, it->value))
                return nullptr;
            result->setValue(it->key, it->value.copyRef());
        } else if (equal(it->key, "webSocketUrl"_s)) {
            auto webSocketURL = it->value->asBoolean();
            if (!webSocketURL)
                return nullptr;
            result->setBoolean(it->key, *webSocketURL);
        } else
            return nullptr;
    }
    return result;
}

RefPtr<JSON::Object> WebDriverService::mergeCapabilities(const JSON::Object& requiredCapabilities, const JSON::Object& firstMatchCapabilities) const
{
    // §7.2 Processing Capabilities.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-merging-capabilities
    auto result = JSON::Object::create();
    auto requiredEnd = requiredCapabilities.end();
    for (auto it = requiredCapabilities.begin(); it != requiredEnd; ++it)
        result->setValue(it->key, it->value.copyRef());

    auto firstMatchEnd = firstMatchCapabilities.end();
    for (auto it = firstMatchCapabilities.begin(); it != firstMatchEnd; ++it)
        result->setValue(it->key, it->value.copyRef());

    return result;
}

RefPtr<JSON::Object> WebDriverService::matchCapabilities(const JSON::Object& mergedCapabilities) const
{
    // §7.2 Processing Capabilities.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-matching-capabilities
    Capabilities platformCapabilities = this->platformCapabilities();

    // Some capabilities like browser name and version might need to launch the browser,
    // so we only reject the known capabilities that don't match.
    auto matchedCapabilities = JSON::Object::create();
    if (platformCapabilities.browserName)
        matchedCapabilities->setString("browserName"_si, platformCapabilities.browserName.value());
    if (platformCapabilities.browserVersion)
        matchedCapabilities->setString("browserVersion"_si, platformCapabilities.browserVersion.value());
    if (platformCapabilities.platformName)
        matchedCapabilities->setString("platformName"_si, platformCapabilities.platformName.value());
    if (platformCapabilities.acceptInsecureCerts)
        matchedCapabilities->setBoolean("acceptInsecureCerts"_si, platformCapabilities.acceptInsecureCerts.value());
    if (platformCapabilities.strictFileInteractability)
        matchedCapabilities->setBoolean("strictFileInteractability"_si, platformCapabilities.strictFileInteractability.value());
    if (platformCapabilities.setWindowRect)
        matchedCapabilities->setBoolean("setWindowRect"_si, platformCapabilities.setWindowRect.value());

    auto end = mergedCapabilities.end();
    for (auto it = mergedCapabilities.begin(); it != end; ++it) {
        if (equal(it->key, "browserName"_s) && platformCapabilities.browserName) {
            auto browserName = it->value->asString();
            if (!equalIgnoringASCIICase(platformCapabilities.browserName.value(), browserName))
                return nullptr;
        } else if (equal(it->key, "browserVersion"_s) && platformCapabilities.browserVersion) {
            auto browserVersion = it->value->asString();
            if (!platformCompareBrowserVersions(browserVersion, platformCapabilities.browserVersion.value()))
                return nullptr;
        } else if (equal(it->key, "platformName"_s) && platformCapabilities.platformName) {
            auto platformName = it->value->asString();
            if (!equalLettersIgnoringASCIICase(platformName, "any"_s) && !equal(platformCapabilities.platformName.value(), platformName))
                return nullptr;
        } else if (equal(it->key, "acceptInsecureCerts"_s) && platformCapabilities.acceptInsecureCerts) {
            auto acceptInsecureCerts = it->value->asBoolean();
            if (acceptInsecureCerts && !platformCapabilities.acceptInsecureCerts.value())
                return nullptr;
        } else if (equal(it->key, "proxy"_s)) {
            auto proxyType = it->value->asObject()->getString("proxyType"_si);
            if (!platformSupportProxyType(proxyType))
                return nullptr;
        } else if (equal(it->key, "webSocketUrl"_s)) {
            auto webSocketURL = it->value->asBoolean();
            if (webSocketURL && !platformSupportBidi())
                return nullptr;
        } else if (!platformMatchCapability(it->key, it->value))
            return nullptr;
        matchedCapabilities->setValue(it->key, it->value.copyRef());
    }

    return matchedCapabilities;
}

Vector<Capabilities> WebDriverService::processCapabilities(const JSON::Object& parameters, Function<void (CommandResult&&)>& completionHandler) const
{
    // §7.2 Processing Capabilities.
    // https://w3c.github.io/webdriver/webdriver-spec.html#processing-capabilities

    // 1. Let capabilities request be the result of getting the property "capabilities" from parameters.
    auto capabilitiesObject = parameters.getObject("capabilities"_si);
    if (!capabilitiesObject) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "capabilities parameter is missing in request"_s));
        return { };
    }

    // 2. Let required capabilities be the result of getting the property "alwaysMatch" from capabilities request.
    RefPtr<JSON::Object> requiredCapabilities;
    auto requiredCapabilitiesValue = capabilitiesObject->getValue("alwaysMatch"_si);
    if (!requiredCapabilitiesValue) {
        // 2.1. If required capabilities is undefined, set the value to an empty JSON Object.
        requiredCapabilities = JSON::Object::create();
    } else if (!(requiredCapabilities = requiredCapabilitiesValue->asObject())) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "alwaysMatch is invalid in capabilities"_si));
        return { };
    }

    // 2.2. Let required capabilities be the result of trying to validate capabilities with argument required capabilities.
    requiredCapabilities = validatedCapabilities(*requiredCapabilities);
    if (!requiredCapabilities) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Invalid alwaysMatch capabilities"_si));
        return { };
    }

    // 3. Let all first match capabilities be the result of getting the property "firstMatch" from capabilities request.
    RefPtr<JSON::Array> firstMatchCapabilitiesList;
    auto firstMatchCapabilitiesValue = capabilitiesObject->getValue("firstMatch"_si);
    if (!firstMatchCapabilitiesValue) {
        // 3.1. If all first match capabilities is undefined, set the value to a JSON List with a single entry of an empty JSON Object.
        firstMatchCapabilitiesList = JSON::Array::create();
        firstMatchCapabilitiesList->pushObject(JSON::Object::create());
    } else {
        firstMatchCapabilitiesList = firstMatchCapabilitiesValue->asArray();
        if (!firstMatchCapabilitiesList) {
            // 3.2. If all first match capabilities is not a JSON List, return error with error code invalid argument.
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "firstMatch is invalid in capabilities"_si));
            return { };
        }
    }

    // 4. Let validated first match capabilities be an empty JSON List.
    auto firstMatchCapabilitiesListLength = firstMatchCapabilitiesList->length();
    Vector<RefPtr<JSON::Object>> validatedFirstMatchCapabilitiesList(ReserveCapacityTag { }, firstMatchCapabilitiesListLength);
    // 5. For each first match capabilities corresponding to an indexed property in all first match capabilities.
    for (unsigned i = 0; i < firstMatchCapabilitiesListLength; ++i) {
        auto firstMatchCapabilities = firstMatchCapabilitiesList->get(i)->asObject();
        if (!firstMatchCapabilities) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Invalid capabilities found in firstMatch"_si));
            return { };
        }
        // 5.1. Let validated capabilities be the result of trying to validate capabilities with argument first match capabilities.
        firstMatchCapabilities = validatedCapabilities(*firstMatchCapabilities);
        if (!firstMatchCapabilities) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Invalid firstMatch capabilities"_si));
            return { };
        }

        // Validate here that firstMatchCapabilities don't shadow alwaysMatchCapabilities.
        auto requiredEnd = requiredCapabilities->end();
        auto firstMatchEnd = firstMatchCapabilities->end();
        for (auto it = firstMatchCapabilities->begin(); it != firstMatchEnd; ++it) {
            if (requiredCapabilities->find(it->key) != requiredEnd) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument,
                    makeString("Invalid firstMatch capabilities: key "_s, it->key, " is present in alwaysMatch"_s)));
                return { };
            }
        }

        // 5.2. Append validated capabilities to validated first match capabilities.
        validatedFirstMatchCapabilitiesList.append(WTF::move(firstMatchCapabilities));
    }

    // 6. For each first match capabilities corresponding to an indexed property in validated first match capabilities.
    Vector<Capabilities> matchedCapabilitiesList(ReserveCapacityTag { }, validatedFirstMatchCapabilitiesList.size());
    for (auto& validatedFirstMatchCapabilies : validatedFirstMatchCapabilitiesList) {
        // 6.1. Let merged capabilities be the result of trying to merge capabilities with required capabilities and first match capabilities as arguments.
        auto mergedCapabilities = mergeCapabilities(*requiredCapabilities, *validatedFirstMatchCapabilies);

        // 6.2. Let matched capabilities be the result of trying to match capabilities with merged capabilities as an argument.
        if (auto matchedCapabilities = matchCapabilities(*mergedCapabilities)) {
            // 6.3. If matched capabilities is not null return matched capabilities.
            Capabilities capabilities;
            parseCapabilities(*matchedCapabilities, capabilities);
            matchedCapabilitiesList.append(WTF::move(capabilities));
        }
    }

    if (matchedCapabilitiesList.isEmpty()) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, "Failed to match capabilities"_si));
        return { };
    }

    return matchedCapabilitiesList;
}

void WebDriverService::newSession(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §8.1 New Session.
    // https://www.w3.org/TR/webdriver/#new-session
    auto matchedCapabilitiesList = processCapabilities(*parameters, completionHandler);
    if (matchedCapabilitiesList.isEmpty())
        return;

    discardSessionIfDisconnected();
    if (!m_session) {
        // Reverse the vector to always take last item.
        matchedCapabilitiesList.reverse();
        connectToBrowser(WTF::move(matchedCapabilitiesList), WTF::move(completionHandler));
        return;
    }

    if (m_replaceOnNewSession) {
        RELEASE_LOG(WebDriverClassic, "WebDriverService::newSession: Replacing existing session.");
        auto session = std::exchange(m_session, nullptr);
        session->close([this, session, matchedCapabilitiesList, completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
#if ENABLE(WEBDRIVER_BIDI)
            m_bidiServer->disconnectSession(session->id());
#endif
            // Ignore unknown errors when closing the session if the session has abeen actually closed.
            if ((!result.isError()) || (result.errorCode() == CommandResult::ErrorCode::UnknownError && !session->isConnected())) {
                matchedCapabilitiesList.reverse();
                connectToBrowser(WTF::move(matchedCapabilitiesList), WTF::move(completionHandler));
            } else
                completionHandler(WTF::move(result));
        });
        return;
    }
    RELEASE_LOG(WebDriverClassic, "WebDriverService::newSession: Maximum number of active sessions reached. Returning error.");
    completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, "Maximum number of active sessions"_si));
}

void WebDriverService::connectToBrowser(Vector<Capabilities>&& capabilitiesList, Function<void (CommandResult&&)>&& completionHandler)
{
    if (capabilitiesList.isEmpty()) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, "Failed to match capabilities"_si));
        return;
    }

    auto sessionHost = SessionHost::create(capabilitiesList.takeLast());
    sessionHost->setHostAddress(m_targetAddress, m_targetPort);
    auto protectedSessionHost = Ref<SessionHost>(sessionHost);
    protectedSessionHost->connectToBrowser([this, capabilitiesList = WTF::move(capabilitiesList), sessionHost = WTF::move(sessionHost), completionHandler = WTF::move(completionHandler)](std::optional<RefPtr<StringImpl>> error) mutable {
        if (error) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, makeString("Failed to connect to browser: "_s, error.value())));
            return;
        }

        createSession(WTF::move(capabilitiesList), WTF::move(sessionHost), WTF::move(completionHandler));
    });
}

void WebDriverService::createSession(Vector<Capabilities>&& capabilitiesList, Ref<SessionHost>&& sessionHost, Function<void (CommandResult&&)>&& completionHandler)
{
    auto protectedSessionHost = Ref<SessionHost>(sessionHost);
    protectedSessionHost->startAutomationSession([this, capabilitiesList = WTF::move(capabilitiesList), sessionHost = WTF::move(sessionHost), completionHandler = WTF::move(completionHandler)](bool capabilitiesDidMatch, std::optional<RefPtr<StringImpl>> errorMessage) mutable {
        if (errorMessage) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::UnknownError, errorMessage.value()));
            return;
        }
        if (!capabilitiesDidMatch) {
            connectToBrowser(WTF::move(capabilitiesList), WTF::move(completionHandler));
            return;
        }
#if ENABLE(WEBDRIVER_BIDI)
        RefPtr<Session> session = Session::create(WTF::move(sessionHost), m_bidiServer);
#else
        RefPtr<Session> session = Session::create(WTF::move(sessionHost));
#endif
        session->createTopLevelBrowsingContext([this, session, completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
            if (result.isError()) {
                completionHandler(CommandResult::fail(CommandResult::ErrorCode::SessionNotCreated, result.errorMessage().value_or("Unknown error creating top level browsing context."_s)));
                return;
            }

            m_session = WTF::move(session);

            auto resultObject = JSON::Object::create();
            resultObject->setString("sessionId"_si, m_session->id());
            auto capabilitiesObject = JSON::Object::create();
            const auto& capabilities = m_session->capabilities();
            capabilitiesObject->setString("browserName"_si, capabilities.browserName.value_or(emptyString()));
            capabilitiesObject->setString("browserVersion"_si, capabilities.browserVersion.value_or(emptyString()));
            capabilitiesObject->setString("platformName"_si, capabilities.platformName.value_or(emptyString()));
            capabilitiesObject->setBoolean("acceptInsecureCerts"_si, capabilities.acceptInsecureCerts.value_or(false));
            capabilitiesObject->setBoolean("strictFileInteractability"_si, capabilities.strictFileInteractability.value_or(false));
            capabilitiesObject->setBoolean("setWindowRect"_si, capabilities.setWindowRect.value_or(true));
            capabilitiesObject->setInteger("webkit:processID"_si, capabilities.processID.value_or(0));
            capabilitiesObject->setValue("unhandledPromptBehavior"_si, serializeUserPromptHandler(capabilities.unhandledPromptBehavior));
            switch (capabilities.pageLoadStrategy.value_or(PageLoadStrategy::Normal)) {
            case PageLoadStrategy::None:
                capabilitiesObject->setString("pageLoadStrategy"_si, "none"_si);
                break;
            case PageLoadStrategy::Normal:
                capabilitiesObject->setString("pageLoadStrategy"_si, "normal"_si);
                break;
            case PageLoadStrategy::Eager:
                capabilitiesObject->setString("pageLoadStrategy"_si, "eager"_si);
                break;
            }
            if (!capabilities.proxy)
                capabilitiesObject->setObject("proxy"_si, JSON::Object::create());
            auto timeoutsObject = JSON::Object::create();
            if (m_session->scriptTimeout() == std::numeric_limits<double>::infinity())
                timeoutsObject->setValue("script"_si, JSON::Value::null());
            else
                timeoutsObject->setDouble("script"_si, m_session->scriptTimeout());
            timeoutsObject->setDouble("pageLoad"_si, m_session->pageLoadTimeout());
            timeoutsObject->setDouble("implicit"_si, m_session->implicitWaitTimeout());
            capabilitiesObject->setObject("timeouts"_si, WTF::move(timeoutsObject));

#if ENABLE(WEBDRIVER_BIDI)
            // Extension steps defined by BiDi spec: https://w3c.github.io/webdriver-bidi/#establishing
            if (!m_session->hasBiDiEnabled() && capabilities.webSocketURL && *capabilities.webSocketURL) {
                auto listener = m_bidiServer->startListening(m_session->id());
                // We need to update the listener host to a visible one so remote clients can connect to it.
                listener->host = m_server.visibleHost();

                auto webSocketURL = m_bidiServer->getWebSocketURL(listener, m_session->id());
                capabilitiesObject->setString("webSocketUrl"_si, webSocketURL);
                m_session->setHasBiDiEnabled(true);
            } else {
                RELEASE_LOG(WebDriverBiDi, "BiDi support not enabled for session %s", utf8(m_session->id()));
                if (!m_session->hasBiDiEnabled())
                    RELEASE_LOG(WebDriverBiDi, "BiDi flag not set for session %s", utf8(m_session->id()));
                if (!capabilities.webSocketURL || !*capabilities.webSocketURL)
                    RELEASE_LOG(WebDriverBiDi, "webSocketURL not set for session %s", utf8(m_session->id()));
            }
#endif

            resultObject->setObject("capabilities"_si, WTF::move(capabilitiesObject));
            completionHandler(CommandResult::success(WTF::move(resultObject)));
        });
    });
}

void WebDriverService::deleteSession(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §8.2 Delete Session.
    // https://www.w3.org/TR/webdriver/#delete-session
    auto sessionID = parameters->getString("sessionId"_si);
    if (!sessionID) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Missing session ID parameter"_s));
        return;
    }

    if (!m_session || !equal(m_session->id(), sessionID)) {
        completionHandler(CommandResult::success());
        return;
    }

    auto session = std::exchange(m_session, nullptr);
    session->close([this, session, completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        UNUSED_VARIABLE(this); // Conditionally used in ENABLE(WEBDRIVER_BIDI) block.
#if ENABLE(WEBDRIVER_BIDI)
        m_bidiServer->disconnectSession(session->id());
#endif
        // Ignore unknown errors when closing the session if the session has abeen actually closed.
        if (result.isError() && result.errorCode() == CommandResult::ErrorCode::UnknownError && !session->isConnected())
            completionHandler(CommandResult::success());
        else
            completionHandler(WTF::move(result));
    });
}

void WebDriverService::status(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&& completionHandler)
{
    // §8.3 Status
    // https://w3c.github.io/webdriver/webdriver-spec.html#status
    discardSessionIfDisconnected();
    auto body = JSON::Object::create();
    body->setBoolean("ready"_si, !m_session);
    body->setString("message"_si, StringImpl::create(m_session ? "A session already exists"_s : "No sessions"_s).ptr());
    completionHandler(CommandResult::success(WTF::move(body)));
}

void WebDriverService::getTimeouts(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §8.4 Get Timeouts.
    // https://w3c.github.io/webdriver/webdriver-spec.html#get-timeouts
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    m_session->getTimeouts(WTF::move(completionHandler));
}

void WebDriverService::setTimeouts(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §8.5 Set Timeouts.
    // https://www.w3.org/TR/webdriver/#set-timeouts
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto timeouts = deserializeTimeouts(*parameters, IgnoreUnknownTimeout::Yes);
    if (!timeouts) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Invalid timeouts"_s));
        return;
    }

    m_session->setTimeouts(timeouts.value(), WTF::move(completionHandler));
}

void WebDriverService::go(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.1 Go.
    // https://www.w3.org/TR/webdriver/#go
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto url = parameters->getString("url"_si);
    if (!url) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Missing URL parameter"_s));
        return;
    }

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, url = WTF::move(url), completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->go(url, WTF::move(completionHandler));
    });
}

void WebDriverService::getCurrentURL(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.2 Get Current URL.
    // https://www.w3.org/TR/webdriver/#get-current-url
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->getCurrentURL(WTF::move(completionHandler));
    });
}

void WebDriverService::back(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.3 Back.
    // https://www.w3.org/TR/webdriver/#back
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->back(WTF::move(completionHandler));
    });
}

void WebDriverService::forward(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.4 Forward.
    // https://www.w3.org/TR/webdriver/#forward
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->forward(WTF::move(completionHandler));
    });
}

void WebDriverService::refresh(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.5 Refresh.
    // https://www.w3.org/TR/webdriver/#refresh
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->refresh(WTF::move(completionHandler));
    });
}

void WebDriverService::getTitle(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §9.6 Get Title.
    // https://www.w3.org/TR/webdriver/#get-title
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->getTitle(WTF::move(completionHandler));
    });
}

void WebDriverService::getWindowHandle(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.1 Get Window Handle.
    // https://www.w3.org/TR/webdriver/#get-window-handle
    if (findSessionOrCompleteWithError(*parameters, completionHandler))
        m_session->getWindowHandle(WTF::move(completionHandler));
}

void WebDriverService::getWindowRect(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.7.1 Get Window Rect.
    // https://w3c.github.io/webdriver/webdriver-spec.html#get-window-rect
    if (findSessionOrCompleteWithError(*parameters, completionHandler))
        m_session->getWindowRect(WTF::move(completionHandler));
}

void WebDriverService::setWindowRect(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.7.2 Set Window Rect.
    // https://w3c.github.io/webdriver/webdriver-spec.html#set-window-rect
    std::optional<double> width;
    if (auto value = parameters->getValue("width"_si)) {
        if (auto number = valueAsNumberInRange(*value))
            width = number;
        else if (!value->isNull()) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Invalid width value"_s));
            return;
        }
    }
    std::optional<double> height;
    if (auto value = parameters->getValue("height"_si)) {
        if (auto number = valueAsNumberInRange(*value))
            height = number;
        else if (!value->isNull()) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Invalid height value"_s));
            return;
        }
    }
    std::optional<double> x;
    if (auto value = parameters->getValue("x"_si)) {
        if (auto number = valueAsNumberInRange(*value, INT_MIN))
            x = number;
        else if (!value->isNull()) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Invalid x value"_s));
            return;
        }
    }
    std::optional<double> y;
    if (auto value = parameters->getValue("y"_si)) {
        if (auto number = valueAsNumberInRange(*value, INT_MIN))
            y = number;
        else if (!value->isNull()) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Invalid y value"_s));
            return;
        }
    }

    // FIXME: If the remote end does not support the Set Window Rect command for the current
    // top-level browsing context for any reason, return error with error code unsupported operation.

    if (findSessionOrCompleteWithError(*parameters, completionHandler))
        m_session->setWindowRect(x, y, width, height, WTF::move(completionHandler));
}

void WebDriverService::maximizeWindow(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.7.3 Maximize Window
    // https://w3c.github.io/webdriver/#maximize-window
    if (findSessionOrCompleteWithError(*parameters, completionHandler))
        m_session->maximizeWindow(WTF::move(completionHandler));
}

void WebDriverService::minimizeWindow(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.7.4 Minimize Window
    // https://w3c.github.io/webdriver/#minimize-window
    if (findSessionOrCompleteWithError(*parameters, completionHandler))
        m_session->minimizeWindow(WTF::move(completionHandler));
}

void WebDriverService::fullscreenWindow(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.7.5 Fullscreen Window
    // https://w3c.github.io/webdriver/#fullscreen-window
    if (findSessionOrCompleteWithError(*parameters, completionHandler))
        m_session->fullscreenWindow(WTF::move(completionHandler));
}

void WebDriverService::closeWindow(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.2 Close Window.
    // https://www.w3.org/TR/webdriver/#close-window
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto protectedSession = Ref { *m_session };
    m_session->closeWindow([this, protectedSession, completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }

        auto handles = result.result()->asArray();
        if (handles && !handles->length() && m_session && equal(protectedSession->id(), m_session->id()))
            m_session = nullptr;

        completionHandler(WTF::move(result));
    });
}

void WebDriverService::switchToWindow(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.3 Switch To Window.
    // https://www.w3.org/TR/webdriver/#switch-to-window
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto handle = parameters->getString("handle"_si);
    if (!handle) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Missing handle parameter"_s));
        return;
    }

    m_session->switchToWindow(handle, WTF::move(completionHandler));
}

void WebDriverService::getWindowHandles(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.4 Get Window Handles.
    // https://www.w3.org/TR/webdriver/#get-window-handles
    if (findSessionOrCompleteWithError(*parameters, completionHandler))
        m_session->getWindowHandles(WTF::move(completionHandler));
}

void WebDriverService::newWindow(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §11.5 New Window
    // https://w3c.github.io/webdriver/#new-window
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    std::optional<RefPtr<StringImpl>> typeHint;
    if (auto value = parameters->getValue("type"_si)) {
        auto valueString = value->asString();
        if (!!valueString) {
            if (equal(valueString, "window"_s) || equal(valueString, "tab"_s))
                typeHint = valueString;
        } else if (!value->isNull()) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Invalid type value"_s));
            return;
        }
    }

    m_session->newWindow(typeHint, WTF::move(completionHandler));
}

void WebDriverService::switchToFrame(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.5 Switch To Frame.
    // https://www.w3.org/TR/webdriver/#switch-to-frame
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto frameID = parameters->getValue("id"_si);
    if (!frameID) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Missing frame ID parameter"_s));
        return;
    }

    switch (frameID->type()) {
    case JSON::Value::Type::Null:
        break;
    case JSON::Value::Type::Double:
    case JSON::Value::Type::Integer:
        if (!valueAsNumberInRange(*frameID, 0, std::numeric_limits<unsigned short>::max())) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Invalid frame ID value"_s));
            return;
        }
        break;
    case JSON::Value::Type::Object: {
        auto frameIDObject = frameID->asObject();
        if (frameIDObject->find(Session::webElementIdentifier()) == frameIDObject->end()) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Invalid frame ID object"_s));
            return;
        }
        break;
    }
    case JSON::Value::Type::Boolean:
    case JSON::Value::Type::String:
    case JSON::Value::Type::Array:
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Invalid frame ID type"_s));
        return;
    }

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, frameID, completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->switchToFrame(WTF::move(frameID), WTF::move(completionHandler));
    });
}

void WebDriverService::switchToParentFrame(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §10.6 Switch To Parent Frame.
    // https://www.w3.org/TR/webdriver/#switch-to-parent-frame
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->switchToParentFrame(WTF::move(completionHandler));
    });
}

static std::optional<RefPtr<StringImpl>> findElementOrCompleteWithError(JSON::Object& parameters, Function<void (CommandResult&&)>& completionHandler, Session::ElementIsShadowRoot isShadowRoot = Session::ElementIsShadowRoot::No)
{
    auto elementID = parameters.getString(StringImpl::create(isShadowRoot == Session::ElementIsShadowRoot::Yes ? "shadowId"_s : "elementId"_s).ptr());
    if (isEmpty(elementID)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Missing element ID or shadow ID parameter"_s));
        return std::nullopt;
    }
    return elementID;
}

static inline bool isValidStrategy(StringImpl* strategy)
{
    // §12.1 Locator Strategies.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dfn-table-of-location-strategies
    return equal(strategy, "css selector"_s)
        || equal(strategy, "link text"_s)
        || equal(strategy, "partial link text"_s)
        || equal(strategy, "tag name"_s)
        || equal(strategy, "xpath"_s);
}

static bool findStrategyAndSelectorOrCompleteWithError(JSON::Object& parameters, Function<void (CommandResult&&)>& completionHandler, Session::ElementIsShadowRoot isShadowRoot, RefPtr<StringImpl>& strategy, RefPtr<StringImpl>& selector)
{
    strategy = parameters.getString("using"_si);
    if (!isValidStrategy(strategy)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, makeString("Invalid strategy: "_s, strategy)));
        return false;
    }
    selector = parameters.getString("value"_si);
    if (!selector) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Missing selector value"_s));
        return false;
    }

    if (isShadowRoot == Session::ElementIsShadowRoot::Yes) {
        // Currently there is an opened discussion about if the following values has to be supported for a Shadow Root
        // because the current implementation doesn't support them. We have them disabled for now.
        // https://github.com/w3c/webdriver/issues/1610
        if (equal(strategy, "tag name"_s) || equal(strategy, "xpath"_s)) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidSelector, makeString("Unsupported strategy for shadow root: "_s, strategy)));
            return false;
        }
    }

    return true;
}

void WebDriverService::findElement(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §12.2 Find Element.
    // https://www.w3.org/TR/webdriver/#find-element
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    RefPtr<StringImpl> strategy, selector;
    if (!findStrategyAndSelectorOrCompleteWithError(*parameters, completionHandler, Session::ElementIsShadowRoot::No, strategy, selector))
        return;

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, strategy = WTF::move(strategy), selector = WTF::move(selector), completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->findElements(strategy, selector, Session::FindElementsMode::Single, emptyString(), Session::ElementIsShadowRoot::No, WTF::move(completionHandler));
    });
}

void WebDriverService::findElements(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §12.3 Find Elements.
    // https://www.w3.org/TR/webdriver/#find-elements
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    RefPtr<StringImpl> strategy, selector;
    if (!findStrategyAndSelectorOrCompleteWithError(*parameters, completionHandler, Session::ElementIsShadowRoot::No, strategy, selector))
        return;

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, strategy = WTF::move(strategy), selector = WTF::move(selector), completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->findElements(strategy, selector, Session::FindElementsMode::Multiple, emptyString(), Session::ElementIsShadowRoot::No, WTF::move(completionHandler));
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

    RefPtr<StringImpl> strategy, selector;
    if (!findStrategyAndSelectorOrCompleteWithError(*parameters, completionHandler, Session::ElementIsShadowRoot::No, strategy, selector))
        return;
    m_session->findElements(strategy, selector, Session::FindElementsMode::Single, elementID.value(), Session::ElementIsShadowRoot::No, WTF::move(completionHandler));
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

    RefPtr<StringImpl> strategy, selector;
    if (!findStrategyAndSelectorOrCompleteWithError(*parameters, completionHandler, Session::ElementIsShadowRoot::No, strategy, selector))
        return;

    m_session->findElements(strategy, selector, Session::FindElementsMode::Multiple, elementID.value(), Session::ElementIsShadowRoot::No, WTF::move(completionHandler));
}

void WebDriverService::findElementFromShadowRoot(RefPtr<JSON::Object>&& parameters, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto shadowID = findElementOrCompleteWithError(*parameters, completionHandler, Session::ElementIsShadowRoot::Yes);
    if (!shadowID)
        return;

    RefPtr<StringImpl> strategy, selector;
    if (!findStrategyAndSelectorOrCompleteWithError(*parameters, completionHandler, Session::ElementIsShadowRoot::Yes, strategy, selector))
        return;

    m_session->findElements(strategy, selector, Session::FindElementsMode::Single, shadowID.value(), Session::ElementIsShadowRoot::Yes, WTF::move(completionHandler));
}

void WebDriverService::findElementsFromShadowRoot(RefPtr<JSON::Object>&& parameters, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto shadowID = findElementOrCompleteWithError(*parameters, completionHandler, Session::ElementIsShadowRoot::Yes);
    if (!shadowID)
        return;

    RefPtr<StringImpl> strategy, selector;
    if (!findStrategyAndSelectorOrCompleteWithError(*parameters, completionHandler, Session::ElementIsShadowRoot::Yes, strategy, selector))
        return;

    m_session->findElements(strategy, selector, Session::FindElementsMode::Multiple, shadowID.value(), Session::ElementIsShadowRoot::Yes, WTF::move(completionHandler));
}

void WebDriverService::getActiveElement(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §12.6 Get Active Element.
    // https://w3c.github.io/webdriver/webdriver-spec.html#get-active-element
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->getActiveElement(WTF::move(completionHandler));
    });
}

void WebDriverService::getElementShadowRoot(RefPtr<JSON::Object>&& parameters, Function<void(CommandResult&&)>&& completionHandler)
{
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    m_session->getElementShadowRoot(elementID.value(), WTF::move(completionHandler));
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

    m_session->isElementSelected(elementID.value(), WTF::move(completionHandler));
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

    auto attribute = parameters->getString("name"_si);
    if (!attribute) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Missing attribute name parameter"_s));
        return;
    }

    m_session->getElementAttribute(elementID.value(), attribute, WTF::move(completionHandler));
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

    auto attribute = parameters->getString("name"_si);
    if (!attribute) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Missing property name parameter"_s));
        return;
    }

    m_session->getElementProperty(elementID.value(), attribute, WTF::move(completionHandler));
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

    auto cssProperty = parameters->getString("name"_si);
    if (!cssProperty) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Missing CSS property name parameter"_s));
        return;
    }

    m_session->getElementCSSValue(elementID.value(), cssProperty, WTF::move(completionHandler));
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

    m_session->getElementText(elementID.value(), WTF::move(completionHandler));
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

    m_session->getElementTagName(elementID.value(), WTF::move(completionHandler));
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

    m_session->getElementRect(elementID.value(), WTF::move(completionHandler));
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

    m_session->isElementEnabled(elementID.value(), WTF::move(completionHandler));
}

void WebDriverService::getComputedRole(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §12.4.9 Get Computed Role
    // https://www.w3.org/TR/webdriver/#get-computed-role
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    m_session->getComputedRole(elementID.value(), WTF::move(completionHandler));
}

void WebDriverService::consumeUserActivation(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // consume-user-activation extension command.
    // https://html.spec.whatwg.org/#user-activation-user-agent-automation
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    m_session->consumeUserActivation(WTF::move(completionHandler));
}

void WebDriverService::getComputedLabel(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §12.4.10 Get Computed Role
    // https://www.w3.org/TR/webdriver/#get-computed-label
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto elementID = findElementOrCompleteWithError(*parameters, completionHandler);
    if (!elementID)
        return;

    m_session->getComputedLabel(elementID.value(), WTF::move(completionHandler));
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

    m_session->isElementDisplayed(elementID.value(), WTF::move(completionHandler));
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

    m_session->elementClick(elementID.value(), WTF::move(completionHandler));
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

    m_session->elementClear(elementID.value(), WTF::move(completionHandler));
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

    auto text = parameters->getString("text"_si);
    if (isEmpty(text)) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Missing text parameter"_s));
        return;
    }

    m_session->elementSendKeys(elementID.value(), text, WTF::move(completionHandler));
}

void WebDriverService::getPageSource(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §15.1 Getting Page Source.
    // https://w3c.github.io/webdriver/webdriver-spec.html#getting-page-source
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    m_session->getPageSource(WTF::move(completionHandler));
}

static bool findScriptAndArgumentsOrCompleteWithError(JSON::Object& parameters, Function<void (CommandResult&&)>& completionHandler, RefPtr<StringImpl>& script, RefPtr<JSON::Array>& arguments)
{
    script = parameters.getString("script"_si);
    if (!script) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Missing script parameter"_s));
        return false;
    }
    arguments = parameters.getArray("args"_si);
    if (!arguments) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Missing args parameter"_s));
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

    RefPtr<StringImpl> script;
    RefPtr<JSON::Array> arguments;
    if (!findScriptAndArgumentsOrCompleteWithError(*parameters, completionHandler, script, arguments))
        return;

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, script = WTF::move(script), arguments = WTF::move(arguments), completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->executeScript(script, WTF::move(arguments), Session::ExecuteScriptMode::Sync, WTF::move(completionHandler));
    });
}

void WebDriverService::executeAsyncScript(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §15.2.2 Execute Async Script.
    // https://www.w3.org/TR/webdriver/#execute-async-script
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    RefPtr<StringImpl> script;
    RefPtr<JSON::Array> arguments;
    if (!findScriptAndArgumentsOrCompleteWithError(*parameters, completionHandler, script, arguments))
        return;

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, script = WTF::move(script), arguments = WTF::move(arguments), completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->executeScript(script, WTF::move(arguments), Session::ExecuteScriptMode::Async, WTF::move(completionHandler));
    });
}

void WebDriverService::getAllCookies(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §16.1 Get All Cookies.
    // https://w3c.github.io/webdriver/webdriver-spec.html#get-all-cookies
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->getAllCookies(WTF::move(completionHandler));
    });
}

void WebDriverService::getNamedCookie(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §16.2 Get Named Cookie.
    // https://w3c.github.io/webdriver/webdriver-spec.html#get-named-cookie
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto name = parameters->getString("name"_si);
    if (!name) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Missing cookie name parameter"_s));
        return;
    }

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, name = WTF::move(name), completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->getNamedCookie(name, WTF::move(completionHandler));
    });
}

static std::optional<Session::Cookie> deserializeCookie(JSON::Object& cookieObject)
{
    Session::Cookie cookie;

    cookie.name = cookieObject.getString("name"_si);
    if (isEmpty(cookie.name))
        return std::nullopt;

    cookie.value = cookieObject.getString("value"_si);
    if (isEmpty(cookie.value))
        return std::nullopt;

    if (auto value = cookieObject.getValue("path"_si)) {
        auto path = value->asString();
        if (!path)
            return std::nullopt;
        cookie.path = path;
    }
    if (auto value = cookieObject.getValue("domain"_si)) {
        auto domain = value->asString();
        if (!domain)
            return std::nullopt;
        cookie.domain = domain;
    }
    if (auto value = cookieObject.getValue("secure"_si)) {
        auto secure = value->asBoolean();
        if (!secure)
            return std::nullopt;
        cookie.secure = secure;
    }
    if (auto value = cookieObject.getValue("httpOnly"_si)) {
        auto httpOnly = value->asBoolean();
        if (!httpOnly)
            return std::nullopt;
        cookie.httpOnly = httpOnly;
    }
    if (auto value = cookieObject.getValue("expiry"_si)) {
        auto expiry = unsignedValue(*value);
        if (!expiry)
            return std::nullopt;
        cookie.expiry = expiry.value();
    }
    if (auto value = cookieObject.getValue("sameSite"_si)) {
        auto sameSite = value->asString();
        if (!equal(sameSite, "None"_s) && !equal(sameSite, "Lax"_s) && !equal(sameSite, "Strict"_s))
            return std::nullopt;
        cookie.sameSite = sameSite;
    }

    return cookie;
}

void WebDriverService::addCookie(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §16.3 Add Cookie.
    // https://w3c.github.io/webdriver/webdriver-spec.html#add-cookie
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto cookieObject = parameters->getObject("cookie"_si);
    if (!cookieObject) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Missing cookie parameter"_s));
        return;
    }

    auto cookie = deserializeCookie(*cookieObject);
    if (!cookie) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Invalid cookie parameter"_s));
        return;
    }

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, cookie = WTF::move(cookie), completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->addCookie(cookie.value(), WTF::move(completionHandler));
    });
}

void WebDriverService::deleteCookie(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §16.4 Delete Cookie.
    // https://w3c.github.io/webdriver/webdriver-spec.html#delete-cookie
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto name = parameters->getString("name"_si);
    if (!name) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Missing cookie name parameter"_s));
        return;
    }

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, name = WTF::move(name), completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->deleteCookie(name, WTF::move(completionHandler));
    });
}

void WebDriverService::deleteAllCookies(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §16.5 Delete All Cookies.
    // https://w3c.github.io/webdriver/webdriver-spec.html#delete-all-cookies
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->deleteAllCookies(WTF::move(completionHandler));
    });
}

static bool processPauseAction(JSON::Object& actionItem, Action& action, std::optional<RefPtr<StringImpl>>& errorMessage)
{
    auto durationValue = actionItem.getValue("duration"_si);
    if (!durationValue)
        return true;

    auto duration = unsignedValue(*durationValue);
    if (!duration) {
        errorMessage = "The 'duration' parameter for the pause action is invalid"_si;
        return false;
    }

    action.duration = duration.value();
    return true;
}

static std::optional<Action> processNullAction(StringImpl* id, JSON::Object& actionItem, std::optional<RefPtr<StringImpl>>& errorMessage)
{
    auto subtype = actionItem.getString("type"_si);
    if (!equal(subtype, "pause"_s)) {
        errorMessage = "The 'type' parameter for the null action is invalid or missing"_si;
        return std::nullopt;
    }

    Action action(id, Action::Type::None, Action::Subtype::Pause);
    if (!processPauseAction(actionItem, action, errorMessage))
        return std::nullopt;

    return action;
}

static std::optional<Action> processKeyAction(StringImpl* id, JSON::Object& actionItem, std::optional<RefPtr<StringImpl>>& errorMessage)
{
    Action::Subtype actionSubtype;
    auto subtype = actionItem.getString("type"_si);
    if (equal(subtype, "pause"_s))
        actionSubtype = Action::Subtype::Pause;
    else if (equal(subtype, "keyUp"_s))
        actionSubtype = Action::Subtype::KeyUp;
    else if (equal(subtype, "keyDown"_s))
        actionSubtype = Action::Subtype::KeyDown;
    else {
        errorMessage = "The 'type' parameter for the key action is invalid"_si;
        return std::nullopt;
    }

    Action action(id, Action::Type::Key, actionSubtype);

    switch (actionSubtype) {
    case Action::Subtype::Pause:
        if (!processPauseAction(actionItem, action, errorMessage))
            return std::nullopt;
        break;
    case Action::Subtype::KeyUp:
    case Action::Subtype::KeyDown: {
        auto keyValue = actionItem.getValue("value"_si);
        if (!keyValue) {
            errorMessage = "The 'value' parameter for the key up/down action is missing "_si;
            return std::nullopt;
        }
        auto key = keyValue->asString();
        if (isEmpty(key)) {
            errorMessage = "The 'value' parameter for the key up/down action is invalid"_si;
            return std::nullopt;
        }
        // FIXME: check single unicode code point.
        action.key = key;
        break;
    }
    case Action::Subtype::PointerUp:
    case Action::Subtype::PointerDown:
    case Action::Subtype::PointerMove:
    case Action::Subtype::PointerCancel:
    case Action::Subtype::Scroll:
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

enum class CoordinateType { Fractional, Integral };

static bool processPointerMoveAction(JSON::Object& actionItem, Action& action, std::optional<RefPtr<StringImpl>>& errorMessage, CoordinateType coordinateType = CoordinateType::Fractional)
{
    if (auto durationValue = actionItem.getValue("duration"_si)) {
        auto duration = unsignedValue(*durationValue);
        if (!duration) {
            errorMessage = "The 'duration' parameter for the action is invalid"_si;
            return false;
        }
        action.duration = duration.value();
    }

    if (auto originValue = actionItem.getValue("origin"_si)) {
        if (auto originObject = originValue->asObject()) {
            auto elementID = originObject->getString(Session::webElementIdentifier());
            if (!elementID) {
                errorMessage = "The 'origin' parameter for the action is not a valid web element object"_si;
                return false;
            }
            action.origin = PointerOrigin { PointerOrigin::Type::Element, elementID };
        } else {
            auto origin = originValue->asString();
            if (equal(origin, "viewport"_s))
                action.origin = PointerOrigin { PointerOrigin::Type::Viewport, std::nullopt };
            else if (equal(origin, "pointer"_s))
                action.origin = PointerOrigin { PointerOrigin::Type::Pointer, std::nullopt };
            else {
                errorMessage = "The 'origin' parameter for the action is invalid"_si;
                return false;
            }
        }
    } else
        action.origin = PointerOrigin { PointerOrigin::Type::Viewport, std::nullopt };

    if (auto xValue = actionItem.getValue("x"_si)) {
        auto x = valueAsNumberInRange(*xValue, INT_MIN);
        if (!x || (coordinateType == CoordinateType::Integral && x.value() != std::floor(x.value()))) {
            errorMessage = "The 'x' parameter for the action is invalid"_si;
            return false;
        }
        action.x = x.value();
    } else {
        errorMessage = "The 'x' parameter for the action is missing"_si;
        return false;
    }

    if (auto yValue = actionItem.getValue("y"_si)) {
        auto y = valueAsNumberInRange(*yValue, INT_MIN);
        if (!y || (coordinateType == CoordinateType::Integral && y.value() != std::floor(y.value()))) {
            errorMessage = "The 'y' parameter for the action is invalid"_si;
            return false;
        }
        action.y = y.value();
    } else {
        errorMessage = "The 'y' parameter for the action is missing"_si;
        return false;
    }

    return true;
}

static std::optional<Action> processPointerAction(StringImpl* id, PointerParameters& parameters, JSON::Object& actionItem, std::optional<RefPtr<StringImpl>>& errorMessage)
{
    Action::Subtype actionSubtype;
    auto subtype = actionItem.getString("type"_si);
    if (equal(subtype, "pause"_s))
        actionSubtype = Action::Subtype::Pause;
    else if (equal(subtype, "pointerUp"_s))
        actionSubtype = Action::Subtype::PointerUp;
    else if (equal(subtype, "pointerDown"_s))
        actionSubtype = Action::Subtype::PointerDown;
    else if (equal(subtype, "pointerMove"_s))
        actionSubtype = Action::Subtype::PointerMove;
    else if (equal(subtype, "pointerCancel"_s))
        actionSubtype = Action::Subtype::PointerCancel;
    else {
        errorMessage = "The 'type' parameter for the pointer action is invalid"_si;
        return std::nullopt;
    }

    Action action(id, Action::Type::Pointer, actionSubtype);
    action.pointerType = parameters.pointerType;

    switch (actionSubtype) {
    case Action::Subtype::Pause:
        if (!processPauseAction(actionItem, action, errorMessage))
            return std::nullopt;
        break;
    case Action::Subtype::PointerUp:
    case Action::Subtype::PointerDown: {
        auto buttonValue = actionItem.getValue("button"_si);
        if (!buttonValue) {
            errorMessage = "The 'button' parameter for the pointer up/down action is missing"_si;
            return std::nullopt;
        }
        auto button = unsignedValue(*buttonValue);
        if (!button) {
            errorMessage = "The 'button' parameter for the pointer up/down action is invalid"_si;
            return std::nullopt;
        }
        action.button = actionMouseButton(button.value());
        break;
    }
    case Action::Subtype::PointerMove:
        if (!processPointerMoveAction(actionItem, action, errorMessage))
            return std::nullopt;
        break;
    case Action::Subtype::PointerCancel:
        break;
    case Action::Subtype::KeyUp:
    case Action::Subtype::KeyDown:
    case Action::Subtype::Scroll:
        ASSERT_NOT_REACHED();
    }

    return action;
}

static std::optional<Action> processWheelAction(StringImpl* id, JSON::Object& actionItem, std::optional<RefPtr<StringImpl>>& errorMessage)
{
    Action::Subtype actionSubtype;
    auto subtype = actionItem.getString("type"_si);
    if (equal(subtype, "pause"_s))
        actionSubtype = Action::Subtype::Pause;
    else if (equal(subtype, "scroll"_s))
        actionSubtype = Action::Subtype::Scroll;
    else {
        errorMessage = "The 'type' parameter for the wheel action is invalid"_si;
        return std::nullopt;
    }

    Action action(id, Action::Type::Wheel, actionSubtype);

    switch (actionSubtype) {
    case Action::Subtype::Pause:
        if (!processPauseAction(actionItem, action, errorMessage))
            return std::nullopt;
        break;
    case Action::Subtype::Scroll:
        if (!processPointerMoveAction(actionItem, action, errorMessage, CoordinateType::Integral))
            return std::nullopt;

        if (auto deltaXValue = actionItem.getValue("deltaX"_si)) {
            auto deltaX = valueAsNumberInRange(*deltaXValue, INT_MIN);
            if (!deltaX || deltaX.value() != std::floor(deltaX.value())) {
                errorMessage = "The 'deltaX' parameter for the action is invalid"_si;
                return std::nullopt;
            }
            action.deltaX = deltaX.value();
        } else {
            errorMessage = "The 'deltaX' parameter for the action is missing"_si;
            return std::nullopt;
        }

        if (auto deltaYValue = actionItem.getValue("deltaY"_si)) {
            auto deltaY = valueAsNumberInRange(*deltaYValue, INT_MIN);
            if (!deltaY || deltaY.value() != std::floor(deltaY.value())) {
                errorMessage = "The 'deltaY' parameter for the action is invalid"_si;
                return std::nullopt;
            }
            action.deltaY = deltaY.value();
        } else {
            errorMessage = "The 'deltaY' parameter for the action is missing"_si;
            return std::nullopt;
        }
        break;
    case Action::Subtype::KeyUp:
    case Action::Subtype::KeyDown:
    case Action::Subtype::PointerUp:
    case Action::Subtype::PointerDown:
    case Action::Subtype::PointerMove:
    case Action::Subtype::PointerCancel:
        ASSERT_NOT_REACHED();
    }

    return action;
}

static std::optional<PointerParameters> processPointerParameters(JSON::Object& actionSequence, std::optional<RefPtr<StringImpl>>& errorMessage)
{
    PointerParameters parameters;

    auto parametersDataValue = actionSequence.getValue("parameters"_si);
    if (!parametersDataValue)
        return parameters;

    auto parametersData = parametersDataValue->asObject();
    if (!parametersData) {
        errorMessage = "Action sequence pointer parameters is not an object"_si;
        return std::nullopt;
    }

    auto pointerType = parametersData->getString("pointerType"_si);
    if (!pointerType)
        return parameters;

    if (equal(pointerType, "mouse"_s))
        parameters.pointerType = PointerType::Mouse;
    else if (equal(pointerType, "pen"_s))
        parameters.pointerType = PointerType::Pen;
    else if (equal(pointerType, "touch"_s))
        parameters.pointerType = PointerType::Touch;
    else {
        errorMessage = "The 'pointerType' parameter in the pointer parameters of the action sequence is invalid"_si;
        return std::nullopt;
    }

    return parameters;
}

static std::optional<Vector<Action>> processInputActionSequence(Session& session, JSON::Value& actionSequenceValue, std::optional<RefPtr<StringImpl>>& errorMessage)
{
    auto actionSequence = actionSequenceValue.asObject();
    if (!actionSequence) {
        errorMessage = "The action sequence is not an object"_si;
        return std::nullopt;
    }

    auto type = actionSequence->getString("type"_si);
    InputSource::Type inputSourceType;
    if (equal(type, "key"_s))
        inputSourceType = InputSource::Type::Key;
    else if (equal(type, "pointer"_s))
        inputSourceType = InputSource::Type::Pointer;
    else if (equal(type, "wheel"_s))
        inputSourceType = InputSource::Type::Wheel;
    else if (equal(type, "none"_s))
        inputSourceType = InputSource::Type::None;
    else {
        errorMessage = "The 'type' parameter in the action sequence is invalid or missing"_si;
        return std::nullopt;
    }

    auto id = actionSequence->getString("id"_si);
    if (!id) {
        errorMessage = "The 'id' parameter in the action sequence is invalid or missing"_si;
        return std::nullopt;
    }

    std::optional<PointerParameters> parameters;
    std::optional<PointerType> pointerType;
    if (inputSourceType == InputSource::Type::Pointer) {
        parameters = processPointerParameters(*actionSequence, errorMessage);
        if (!parameters)
            return std::nullopt;

        pointerType = parameters->pointerType;
    }

    auto& inputSource = session.getOrCreateInputSource(id, inputSourceType, pointerType);
    if (inputSource.type != inputSourceType) {
        errorMessage = "Action sequence type doesn't match input source type"_si;
        return std::nullopt;
    }

    if (inputSource.type ==  InputSource::Type::Pointer && inputSource.pointerType != pointerType) {
        errorMessage = "Action sequence pointer type doesn't match input source pointer type"_si;
        return std::nullopt;
    }

    auto actionItems = actionSequence->getArray("actions"_si);
    if (!actionItems) {
        errorMessage = "The 'actions' parameter in the action sequence is invalid or not present"_si;
        return std::nullopt;
    }

    Vector<Action> actions;
    unsigned actionItemsLength = actionItems->length();
    for (unsigned i = 0; i < actionItemsLength; ++i) {
        auto actionItem = actionItems->get(i)->asObject();
        if (!actionItem) {
            errorMessage = "An action in the action sequence is not an object"_si;
            return std::nullopt;
        }

        std::optional<Action> action;
        if (inputSourceType == InputSource::Type::None)
            action = processNullAction(id, *actionItem, errorMessage);
        else if (inputSourceType == InputSource::Type::Key)
            action = processKeyAction(id, *actionItem, errorMessage);
        else if (inputSourceType == InputSource::Type::Pointer)
            action = processPointerAction(id, parameters.value(), *actionItem, errorMessage);
        else if (inputSourceType == InputSource::Type::Wheel)
            action = processWheelAction(id, *actionItem, errorMessage);
        if (!action)
            return std::nullopt;

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

    auto actionsArray = parameters->getArray("actions"_si);
    if (!actionsArray) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "The 'actions' parameter is invalid or not present"_si));
        return;
    }

    std::optional<RefPtr<StringImpl>> errorMessage;
    Vector<Vector<Action>> actionsByTick;
    unsigned actionsArrayLength = actionsArray->length();
    for (unsigned i = 0; i < actionsArrayLength; ++i) {
        auto actionSequence = actionsArray->get(i);
        auto inputSourceActions = processInputActionSequence(*m_session, actionSequence, errorMessage);
        if (!inputSourceActions) {
            completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, errorMessage.value_or("Could not process input action sequence"_s)));
            return;
        }
        for (unsigned i = 0; i < inputSourceActions->size(); ++i) {
            if (actionsByTick.size() < i + 1)
                actionsByTick.append({ });
            actionsByTick[i].append(inputSourceActions.value()[i]);
        }
    }

    m_session->performActions(WTF::move(actionsByTick), WTF::move(completionHandler));
}

void WebDriverService::releaseActions(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §17.5 Release Actions.
    // https://w3c.github.io/webdriver/webdriver-spec.html#release-actions
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    m_session->releaseActions(WTF::move(completionHandler));
}

void WebDriverService::dismissAlert(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §18.1 Dismiss Alert.
    // https://w3c.github.io/webdriver/webdriver-spec.html#dismiss-alert
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->dismissAlert(WTF::move(completionHandler));
    });
}

void WebDriverService::acceptAlert(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §18.2 Accept Alert.
    // https://w3c.github.io/webdriver/webdriver-spec.html#accept-alert
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->acceptAlert(WTF::move(completionHandler));
    });
}

void WebDriverService::getAlertText(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §18.3 Get Alert Text.
    // https://w3c.github.io/webdriver/webdriver-spec.html#get-alert-text
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->getAlertText(WTF::move(completionHandler));
    });
}

void WebDriverService::sendAlertText(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §18.4 Send Alert Text.
    // https://w3c.github.io/webdriver/webdriver-spec.html#send-alert-text
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto text = parameters->getString("text"_si);
    if (!text) {
        completionHandler(CommandResult::fail(CommandResult::ErrorCode::InvalidArgument, "Missing text parameter"_s));
        return;
    }

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, text = WTF::move(text), completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->sendAlertText(text, WTF::move(completionHandler));
    });
}

void WebDriverService::takeScreenshot(RefPtr<JSON::Object>&& parameters, Function<void (CommandResult&&)>&& completionHandler)
{
    // §19.1 Take Screenshot.
    // https://w3c.github.io/webdriver/webdriver-spec.html#take-screenshot
    if (!findSessionOrCompleteWithError(*parameters, completionHandler))
        return;

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->takeScreenshot(std::nullopt, std::nullopt, WTF::move(completionHandler));
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

    auto protectedSession = Ref { *m_session };
    protectedSession->waitForNavigationToComplete([protectedSession, elementID, completionHandler = WTF::move(completionHandler)](CommandResult&& result) mutable {
        if (result.isError()) {
            completionHandler(WTF::move(result));
            return;
        }
        protectedSession->takeScreenshot(elementID.value(), true, WTF::move(completionHandler));
    });
}

#if ENABLE(WEBDRIVER_BIDI)
void WebDriverService::bidiSessionStatus(unsigned id, RefPtr<JSON::Object>&&, Function<void(WebSocketMessageHandler::Message&&)>&& completionHandler)
{
    discardSessionIfDisconnected();
    auto result = JSON::Object::create();
    bool ready = !m_session;
    result->setBoolean("ready"_si, ready);
    if (ready)
        result->setString("message"_si, "Ready for new sessions"_si);
    else
        result->setString("message"_si, "Maximum number of sessions created"_si);

    completionHandler(WebSocketMessageHandler::Message::reply("success"_si, id, WTF::move(result)));
}

void WebDriverService::clientDisconnected(const WebSocketMessageHandler::Connection& connection)
{
    // https://w3c.github.io/webdriver-bidi/#handle-a-connection-closing
    if (equal(m_bidiServer->sessionID(connection), m_session->id()))
        m_bidiServer->removeConnection(connection);
    else if (m_bidiServer->isStaticConnection(connection))
        m_bidiServer->removeStaticConnection(connection);
    // Note from spec: This does not end any session.
}

void WebDriverService::onBrowserTerminated(StringImpl* sessionID)
{
    if (m_session && equal(m_session->id(), sessionID)) {
        auto connection = m_bidiServer->connection(sessionID);
        m_bidiServer->disconnectSession(sessionID);
        if (connection)
            clientDisconnected(*connection);
    }
}

#endif // ENABLE(WEBDRIVER_BIDI)

} // namespace WebDriver
