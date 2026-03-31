/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "SessionHost.h"

#if PLATFORM(COCOA)

#include "Logging.h"
#include <Foundation/Foundation.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wtf/JSONValues.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/Span.h>
#include <wtf/UUID.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

// SessionHostCocoa connects the WebDriver daemon to a WKWebView app (MiniBrowser)
// via a direct TCP socket. MiniBrowser runs a lightweight automation server that
// accepts session requests and relays automation protocol messages.
//
// This bypasses webinspectord entirely — no XPC, no entitlements needed.
// The driver launches MiniBrowser with --automation-port=<port>, then connects
// to that port over TCP using length-prefixed JSON messages.
//
// Message framing: [4 bytes big-endian length][JSON UTF-8 payload]

namespace WebDriver {

static constexpr size_t kHeaderSize = 4;
static constexpr int kMaxConnectAttempts = 40;
static constexpr auto kConnectRetryDelay = 500_ms;
static constexpr auto kSessionTimeout = 10_s;

SessionHost::~SessionHost()
{
    if (m_socketFd >= 0) {
        m_readSource = nullptr;
        ::close(m_socketFd);
        m_socketFd = -1;
    }

    if (m_browserTask && [m_browserTask isRunning])
        [m_browserTask terminate];
}

bool SessionHost::isConnected() const
{
    return m_socketFd >= 0;
}

static uint16_t findFreePort()
{
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return 0;

    struct sockaddr_in addr = { };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (::bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(sock);
        return 0;
    }

    socklen_t len = sizeof(addr);
    if (::getsockname(sock, reinterpret_cast<struct sockaddr*>(&addr), &len) < 0) {
        ::close(sock);
        return 0;
    }

    uint16_t port = ntohs(addr.sin_port);
    ::close(sock);
    return port;
}

void SessionHost::connectToBrowser(Function<void(std::optional<String> error)>&& completionHandler)
{
    if (!m_capabilities.browserBinary) {
        completionHandler("No browser binary specified. Use 'safari:browserOptions' { 'binary': '/path/to/MiniBrowser' } capability."_s);
        return;
    }

    m_automationPort = findFreePort();
    if (!m_automationPort) {
        completionHandler("Failed to find a free port for automation socket."_s);
        return;
    }

    launchBrowser([this, completionHandler = WTF::move(completionHandler)](std::optional<String> error) mutable {
        if (error) {
            completionHandler(WTF::move(error));
            return;
        }

        connectViaSocket(WTF::move(completionHandler));
    });
}

void SessionHost::launchBrowser(Function<void(std::optional<String> error)>&& completionHandler)
{
    NSString *browserPath = m_capabilities.browserBinary.value().createNSString().autorelease();

    if (![[NSFileManager defaultManager] isExecutableFileAtPath:browserPath]) {
        completionHandler(makeString("Browser binary not found or not executable: "_s, String(browserPath)));
        return;
    }

    // Resolve the build directory from the browser binary path for DYLD_FRAMEWORK_PATH.
    // The binary is at .app/Contents/MacOS/Executable — the build dir is 3 levels up.
    NSString *buildDir = [[[[browserPath stringByDeletingLastPathComponent]
        stringByDeletingLastPathComponent]
        stringByDeletingLastPathComponent]
        stringByDeletingLastPathComponent];

    m_browserTask = adoptNS([[NSTask alloc] init]);
    [m_browserTask setExecutableURL:[NSURL fileURLWithPath:browserPath]];

    NSMutableArray *arguments = [NSMutableArray array];
    [arguments addObject:@"--allow-remote-automation"];
    [arguments addObject:[NSString stringWithFormat:@"--automation-port=%u", m_automationPort]];

    if (m_capabilities.browserArguments) {
        for (auto& arg : *m_capabilities.browserArguments)
            [arguments addObject:arg.createNSString().autorelease()];
    }

    [m_browserTask setArguments:arguments];

    // Set DYLD_FRAMEWORK_PATH so MiniBrowser loads the local WebKit build.
    NSMutableDictionary *env = [[[NSProcessInfo processInfo] environment] mutableCopy];
    [env setObject:buildDir forKey:@"DYLD_FRAMEWORK_PATH"];
    [env setObject:buildDir forKey:@"__XPC_DYLD_FRAMEWORK_PATH"];
    [m_browserTask setEnvironment:env];

    NSError *launchError = nil;
    BOOL launched = [m_browserTask launchAndReturnError:&launchError];
    if (!launched) {
        completionHandler(makeString("Failed to launch browser: "_s, String([launchError localizedDescription])));
        return;
    }

    LOG(SessionHost, "SessionHostCocoa: Launched browser at %s with PID %d, automation port %u",
        browserPath.UTF8String, [m_browserTask processIdentifier], m_automationPort);

    completionHandler(std::nullopt);
}

void SessionHost::connectViaSocket(Function<void(std::optional<String> error)>&& completionHandler)
{
    // MiniBrowser needs time to start up and begin listening. Retry the connection
    // with a short delay between attempts.
    connectViaSocketWithRetry(0, WTF::move(completionHandler));
}

void SessionHost::connectViaSocketWithRetry(int attempt, Function<void(std::optional<String> error)>&& completionHandler)
{
    if (attempt >= kMaxConnectAttempts) {
        completionHandler(makeString("Failed to connect to browser automation socket after "_s,
            String::number(kMaxConnectAttempts), " attempts on port "_s,
            String::number(m_automationPort)));
        return;
    }

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        completionHandler(makeString("Failed to create socket: "_s, String::fromUTF8(strerror(errno))));
        return;
    }

    struct sockaddr_in addr = { };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(m_automationPort);

    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);

        // Check if the browser is still running.
        if (m_browserTask && ![m_browserTask isRunning]) {
            LOG(SessionHost, "SessionHostCocoa: Browser process exited with status %d", [m_browserTask terminationStatus]);
            completionHandler(makeString("Browser process exited prematurely with status "_s,
                String::number([m_browserTask terminationStatus])));
            return;
        }

        if (!attempt || attempt == 9 || attempt == 19 || attempt == 39)
            LOG(SessionHost, "SessionHostCocoa: Connect attempt %d to port %u failed: %s", attempt + 1, m_automationPort, strerror(errno));

        // Retry after a delay — browser may still be starting up.
        RunLoop::mainSingleton().dispatchAfter(kConnectRetryDelay,
            [this, attempt, completionHandler = WTF::move(completionHandler)]() mutable {
                connectViaSocketWithRetry(attempt + 1, WTF::move(completionHandler));
            });
        return;
    }

    m_socketFd = fd;
    setupSocketReadHandler();

    LOG(SessionHost, "SessionHostCocoa: Connected to browser automation socket on port %u (attempt %d)",
        m_automationPort, attempt + 1);

    completionHandler(std::nullopt);
}

void SessionHost::setupSocketReadHandler()
{
    m_readSource = adoptOSObject(dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, m_socketFd, 0, WTF::mainDispatchQueueSingleton()));

    dispatch_source_set_event_handler(m_readSource.get(), ^{
        readFromSocket();
    });

    dispatch_source_set_cancel_handler(m_readSource.get(), ^{
        // Socket closed.
    });

    dispatch_resume(m_readSource.get());
}

void SessionHost::readFromSocket()
{
    // Read available data into the buffer.
    uint8_t chunk[4096];
    ssize_t bytesRead = ::read(m_socketFd, chunk, sizeof(chunk));
    if (bytesRead <= 0) {
        LOG(SessionHost, "SessionHostCocoa: Socket read returned %zd, connection closed", bytesRead);
        socketConnectionClosed();
        return;
    }

    m_readBuffer.append(std::span(chunk, bytesRead));

    // Process complete messages from the buffer.
    while (m_readBuffer.size() >= kHeaderSize) {
        uint32_t messageLength = 0;
        memcpySpan(std::span(reinterpret_cast<uint8_t*>(&messageLength), kHeaderSize), m_readBuffer.span().subspan(0, kHeaderSize));
        messageLength = ntohl(messageLength);

        if (m_readBuffer.size() < kHeaderSize + messageLength)
            break; // Wait for more data.

        auto jsonSpan = m_readBuffer.span().subspan(kHeaderSize, messageLength);
        auto jsonString = String::fromUTF8(jsonSpan);

        size_t consumed = kHeaderSize + messageLength;
        size_t remaining = m_readBuffer.size() - consumed;
        if (remaining)
            memmoveSpan(m_readBuffer.mutableSpan().subspan(0, remaining), m_readBuffer.span().subspan(consumed, remaining));
        m_readBuffer.shrink(remaining);

        if (auto value = JSON::Value::parseJSON(jsonString)) {
            if (auto message = value->asObject())
                receivedMessage(message);
        }
    }
}

void SessionHost::sendSocketMessage(const JSON::Object& message)
{
    if (m_socketFd < 0)
        return;

    auto json = message.toJSONString().utf8();
    uint32_t length = htonl(json.length());

    // Write header + payload. For small messages, blocking write is fine.
    ::write(m_socketFd, &length, kHeaderSize);
    ::write(m_socketFd, json.data(), json.length());
}

void SessionHost::startAutomationSession(Function<void(bool, std::optional<String>)>&& completionHandler)
{
    m_sessionID = createVersion4UUIDString();
    m_startSessionCompletionHandler = WTF::move(completionHandler);

    if (m_socketFd < 0) {
        if (m_startSessionCompletionHandler)
            m_startSessionCompletionHandler(false, "Not connected to browser."_s);
        return;
    }

    // Send automation session request.
    auto request = JSON::Object::create();
    request->setString("type"_s, "automationSessionRequest"_s);
    request->setString("sessionIdentifier"_s, m_sessionID);

    auto capabilities = JSON::Object::create();
    capabilities->setBoolean("acceptInsecureCerts"_s, m_capabilities.acceptInsecureCerts.value_or(false));
    request->setObject("capabilities"_s, WTF::move(capabilities));

    LOG(SessionHost, "SessionHostCocoa: Sending automation session request for session %s", m_sessionID.utf8().data());
    sendSocketMessage(request);

    // Timeout: if we don't get a response within 10 seconds, fail.
    RunLoop::mainSingleton().dispatchAfter(kSessionTimeout, [this]() {
        if (m_startSessionCompletionHandler) {
            m_startSessionCompletionHandler(false, "Timed out waiting for automation session from browser."_s);
            m_startSessionCompletionHandler = nullptr;
        }
    });
}

void SessionHost::sendMessageToBackend(const String& message)
{
    if (m_socketFd < 0)
        return;

    auto envelope = JSON::Object::create();
    envelope->setString("type"_s, "sendMessageToBackend"_s);
    envelope->setString("message"_s, message);

    sendSocketMessage(envelope);
}

void SessionHost::receivedMessage(RefPtr<JSON::Object> message)
{
    auto type = message->getString("type"_s);
    if (!type) {
        LOG(SessionHost, "SessionHostCocoa: Received message without type field");
        return;
    }

    if (type == "automationSessionCreated"_s) {
        LOG(SessionHost, "SessionHostCocoa: Automation session created");

        if (m_startSessionCompletionHandler) {
            m_startSessionCompletionHandler(true, std::nullopt);
            m_startSessionCompletionHandler = nullptr;
        }
        return;
    }

    if (type == "sendMessageToFrontend"_s) {
        auto payload = message->getString("message"_s);
        if (!payload.isNull())
            dispatchMessage(payload);
        return;
    }

    if (type == "error"_s) {
        auto errorMessage = message->getString("message"_s);
        LOG(SessionHost, "SessionHostCocoa: Received error: %s", errorMessage.utf8().data());

        if (m_startSessionCompletionHandler) {
            m_startSessionCompletionHandler(false, errorMessage);
            m_startSessionCompletionHandler = nullptr;
        }
        return;
    }

    LOG(SessionHost, "SessionHostCocoa: Unhandled message type: %s", type.utf8().data());
}

void SessionHost::socketConnectionClosed()
{
    if (m_socketFd >= 0) {
        m_readSource = nullptr;
        ::close(m_socketFd);
        m_socketFd = -1;
    }

    inspectorDisconnected();
}

} // namespace WebDriver

#endif // PLATFORM(COCOA)
