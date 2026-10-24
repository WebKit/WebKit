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
#include "WebDriverService.h"
#include <algorithm>
#include <array>
#include <crt_externs.h>
#include <fcntl.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wtf/FileSystem.h>
#include <wtf/RunLoop.h>
#include <wtf/SafeStrerror.h>
#include <wtf/UUID.h>
#include <wtf/darwin/DispatchExtras.h>
#include <wtf/posix/POSIXExtras.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringView.h>

// The browser is launched for each session with one end of a socketpair() at a fixed
// descriptor. Driver and browser speak the inspector socket protocol over it, the same
// protocol that SessionHostSocket uses over TCP: JSON events with an "event" field, each
// preceded by its length as a 4-byte big-endian integer.

namespace WebDriver {

static constexpr int browserConnectionFileDescriptor = 3;
static constexpr size_t messageHeaderSize = sizeof(uint32_t);
static constexpr auto startAutomationSessionTimeout = 30_s;
static constexpr auto browserTerminationGracePeriod = 2_s;

// minidriver drives the MiniBrowser that's built with it, in the same build directory.
static std::optional<String> buildDirectory()
{
    std::array<char, PATH_MAX> executablePath;
    uint32_t size = executablePath.size();
    if (_NSGetExecutablePath(executablePath.data(), &size))
        return std::nullopt;
    std::array<char, PATH_MAX> resolvedPath;
    if (!realpath(executablePath.data(), resolvedPath.data()))
        return std::nullopt;
    return FileSystem::parentPath(String::fromUTF8(unsafeSpan(resolvedPath.data())));
}

static void terminateProcess(pid_t processID)
{
    kill(processID, SIGTERM);

    // Reap the process, killing it if it doesn't exit within the grace period.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, browserTerminationGracePeriod.nanosecondsAs<int64_t>()), globalDispatchQueueSingleton(QOS_CLASS_UTILITY, 0), ^{
        if (!waitpid(processID, nullptr, WNOHANG)) {
            kill(processID, SIGKILL);
            waitpid(processID, nullptr, 0);
        }
    });
}

SessionHost::~SessionHost()
{
    if (m_channel)
        dispatch_io_close(m_channel.get(), DISPATCH_IO_STOP);
    terminateBrowser();
}

bool SessionHost::isConnected() const
{
    return !!m_channel;
}

void SessionHost::connectToBrowser(Function<void(std::optional<String> error)>&& completionHandler)
{
    if (m_channel) {
        completionHandler("Already connected to the browser."_s);
        return;
    }

    if (!m_targetIp.isEmpty() || m_capabilities.targetAddr) {
        completionHandler("minidriver can't connect to a browser that is already running."_s);
        return;
    }

    if (auto error = launchBrowser()) {
        completionHandler(WTF::move(error));
        return;
    }

    startReading();
    completionHandler(std::nullopt);
}

std::optional<String> SessionHost::launchBrowser()
{
    auto buildDirectory = WebDriver::buildDirectory();
    if (!buildDirectory)
        return "Failed to find the build directory that minidriver is in."_s;

    std::array<StringView, 4> browserPathComponents { "MiniBrowser.app"_s, "Contents"_s, "MacOS"_s, "MiniBrowser"_s };
    auto browserPathString = FileSystem::pathByAppendingComponents(*buildDirectory, browserPathComponents);
    auto browserPath = browserPathString.utf8();
    if (posixAccess(browserPath, X_OK))
        return makeString("There is no MiniBrowser next to minidriver, at "_s, browserPathString);

    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets))
        return makeString("Failed to create the browser connection: "_s, String::fromUTF8(safeStrerror(errno).span()));
    int driverSocket = sockets[0];
    int browserSocket = sockets[1];
    fcntl(driverSocket, F_SETFD, FD_CLOEXEC);
    fcntl(browserSocket, F_SETFD, FD_CLOEXEC);
    int noSigPipe = 1;
    setsockopt(driverSocket, SOL_SOCKET, SO_NOSIGPIPE, &noSigPipe, sizeof(noSigPipe));

    // dup2() onto the same descriptor wouldn't clear FD_CLOEXEC, so make sure the browser's
    // end isn't already at the descriptor it gets in the browser.
    if (browserSocket == browserConnectionFileDescriptor) {
        int duplicate = fcntl(browserSocket, F_DUPFD_CLOEXEC, browserConnectionFileDescriptor + 1);
        close(browserSocket);
        browserSocket = duplicate;
    }

    Vector<UTF8CString> arguments;
    arguments.append(browserPath);
    // AppKit opens arguments that aren't options as documents. Each option has to be followed
    // by its value as a separate argument, or AppKit pairs the next option with it.
    arguments.append(String("--automation-fd"_s).utf8());
    arguments.append(String::number(browserConnectionFileDescriptor).utf8());
    // AppKit window-state restoration can show a modal "reopen windows?" alert after an
    // abnormal exit, which automation can't dismiss. Turn it off for this launch only.
    for (auto argument : { "-NSQuitAlwaysKeepsWindows"_s, "NO"_s, "-ApplePersistenceIgnoreState"_s, "YES"_s })
        arguments.append(String(argument).utf8());
    if (m_capabilities.browserArguments) {
        for (auto& argument : *m_capabilities.browserArguments)
            arguments.append(argument.utf8());
    }

    auto argv = arguments.map([](auto& argument) {
        return const_cast<char*>(argument.legacyCStringPointer());
    });
    argv.append(nullptr);

    auto hasPrefix = [](const char* string, ASCIILiteral prefix) {
        return spanHasPrefix(unsafeSpan(string), prefix.span());
    };
    // minidriver doesn't depend on WebKit, and runs without any dyld path overrides.
    // We need to add them to reliably launch the browser.
    static constexpr std::array dyldPathAssignments { "DYLD_FRAMEWORK_PATH="_s, "__XPC_DYLD_FRAMEWORK_PATH="_s, "DYLD_LIBRARY_PATH="_s, "__XPC_DYLD_LIBRARY_PATH="_s };
    Vector<char*> envp;
    for (char** variable = *_NSGetEnviron(); *variable; ++variable) {
        if (std::ranges::none_of(dyldPathAssignments, [&](auto assignment) { return hasPrefix(*variable, assignment); }))
            envp.append(*variable);
    }
    auto dyldPaths = WTF::map(dyldPathAssignments, [&](auto assignment) {
        return makeString(assignment, *buildDirectory).utf8();
    });
    for (auto& variable : dyldPaths)
        envp.append(const_cast<char*>(variable.legacyCStringPointer()));
    envp.append(nullptr);

    // Only the standard descriptors and the browser's end of the connection are inherited.
    posix_spawn_file_actions_t fileActions;
    posix_spawn_file_actions_init(&fileActions);
    for (int descriptor : { STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO })
        posix_spawn_file_actions_addinherit_np(&fileActions, descriptor);
    posix_spawn_file_actions_adddup2(&fileActions, browserSocket, browserConnectionFileDescriptor);

    // minidriver ignores SIGTERM and SIGINT to handle them itself, and ignored signals stay
    // ignored in a launched process unless they're reset.
    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    sigset_t defaultSignals;
    sigemptyset(&defaultSignals);
    sigaddset(&defaultSignals, SIGTERM);
    sigaddset(&defaultSignals, SIGINT);
    posix_spawnattr_setsigdefault(&attributes, &defaultSignals);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_CLOEXEC_DEFAULT | POSIX_SPAWN_SETSIGDEF);

    pid_t processID = 0;
    int result = posix_spawn(&processID, browserPath.legacyCStringPointer(), &fileActions, &attributes, argv.mutableSpan().data(), envp.mutableSpan().data());

    posix_spawnattr_destroy(&attributes);
    posix_spawn_file_actions_destroy(&fileActions);
    close(browserSocket);

    if (result) {
        close(driverSocket);
        return makeString("Failed to launch the browser: "_s, String::fromUTF8(safeStrerror(result).span()));
    }

    m_browserProcessID = processID;
    m_capabilities.processID = processID;
    RELEASE_LOG(SessionHost, "Launched browser %s with process ID %d", browserPath, processID);

    WeakPtr weakThis { *this };
    m_browserExitSource = adoptOSObject(dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, processID, DISPATCH_PROC_EXIT, mainDispatchQueueSingleton()));
    dispatch_source_set_event_handler(m_browserExitSource.get(), ^{
        if (RefPtr protectedThis = weakThis)
            protectedThis->browserDidExit();
    });
    dispatch_resume(m_browserExitSource.get());

    m_channel = adoptOSObject(dispatch_io_create(DISPATCH_IO_STREAM, driverSocket, mainDispatchQueueSingleton(), ^(int) {
        close(driverSocket);
    }));
    dispatch_io_set_low_water(m_channel.get(), 1);

    return std::nullopt;
}

void SessionHost::terminateBrowser()
{
    if (m_browserExitSource) {
        dispatch_source_cancel(m_browserExitSource.get());
        m_browserExitSource = nullptr;
    }

    if (auto processID = std::exchange(m_browserProcessID, 0))
        terminateProcess(processID);
}

void SessionHost::browserDidExit()
{
    dispatch_source_cancel(m_browserExitSource.get());
    m_browserExitSource = nullptr;

    // The process has exited, so this doesn't block for long.
    int status = 0;
    if (waitpid(std::exchange(m_browserProcessID, 0), &status, 0) > 0)
        RELEASE_LOG(SessionHost, "Browser exited with status %d", status);

    connectionDidClose();
}

void SessionHost::startReading()
{
    WeakPtr weakThis { *this };
    dispatch_io_read(m_channel.get(), 0, SIZE_MAX, mainDispatchQueueSingleton(), ^(bool done, dispatch_data_t data, int error) {
        RefPtr protectedThis = weakThis;
        if (!protectedThis || !protectedThis->m_channel)
            return;

        if (data)
            protectedThis->didReceiveData(data);

        // The read is done at end of file or on error.
        if (done) {
            if (error)
                RELEASE_LOG_ERROR(SessionHost, "Reading from the browser failed: %s", safeStrerror(error));
            protectedThis->connectionDidClose();
        }
    });
}

void SessionHost::didReceiveData(dispatch_data_t data)
{
    dispatch_data_apply(data, ^bool(dispatch_data_t, size_t, const void* buffer, size_t size) {
        m_receiveBuffer.append(unsafeMakeSpan(static_cast<const uint8_t*>(buffer), size));
        return true;
    });

    size_t offset = 0;
    while (m_channel && m_receiveBuffer.size() - offset >= messageHeaderSize) {
        auto header = m_receiveBuffer.subspan(offset, messageHeaderSize);
        uint32_t messageSize = static_cast<uint32_t>(header[0]) << 24 | static_cast<uint32_t>(header[1]) << 16 | static_cast<uint32_t>(header[2]) << 8 | header[3];
        if (!messageSize) {
            RELEASE_LOG_ERROR(SessionHost, "Received an empty message from the browser");
            connectionDidClose();
            return;
        }
        if (m_receiveBuffer.size() - offset - messageHeaderSize < messageSize)
            break;

        auto message = String::fromUTF8(m_receiveBuffer.subspan(offset + messageHeaderSize, messageSize));
        offset += messageHeaderSize + messageSize;

        auto value = JSON::Value::parseJSON(message);
        auto event = value ? value->asObject() : nullptr;
        if (!event) {
            RELEASE_LOG_ERROR(SessionHost, "Received a message from the browser that isn't a JSON object");
            continue;
        }
        didReceiveEvent(*event);
    }

    // Handling an event can close the connection, which discards the buffer.
    if (m_channel)
        m_receiveBuffer.removeAt(0, offset);
}

void SessionHost::sendEvent(const JSON::Object& event)
{
    if (!m_channel)
        return;

    auto payload = event.toJSONString().utf8();
    auto size = static_cast<uint32_t>(payload.length());
    Vector<uint8_t> message;
    message.reserveInitialCapacity(messageHeaderSize + size);
    message.appendList({ static_cast<uint8_t>(size >> 24), static_cast<uint8_t>(size >> 16), static_cast<uint8_t>(size >> 8), static_cast<uint8_t>(size) });
    message.append(byteCast<uint8_t>(payload.span()));

    OSObjectPtr data = adoptOSObject(dispatch_data_create(message.span().data(), message.size(), nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT));
    dispatch_io_write(m_channel.get(), 0, data.get(), mainDispatchQueueSingleton(), ^(bool done, dispatch_data_t, int error) {
        if (done && error)
            RELEASE_LOG_ERROR(SessionHost, "Writing to the browser failed: %s", safeStrerror(error));
    });
}

void SessionHost::didReceiveEvent(const JSON::Object& event)
{
    auto name = event.getString("event"_s);
    auto message = event.getString("message"_s);
    auto connectionID = event.getInteger("connectionID"_s);

    if (name == "StartAutomationSession_Return"_s) {
        didReceiveStartAutomationSessionReturn(message);
        return;
    }

    if (name == "SetTargetList"_s) {
        if (connectionID && !message.isNull())
            didReceiveSetTargetList(*connectionID, message);
        return;
    }

    if (name == "SendMessageToFrontend"_s) {
        auto targetID = event.getInteger("targetID"_s);
        if (!connectionID || !targetID || message.isNull())
            return;
        if (static_cast<uint64_t>(*connectionID) != m_connectionID || static_cast<uint64_t>(*targetID) != m_target.id)
            return;
        dispatchMessage(message);
        return;
    }

    RELEASE_LOG_ERROR(SessionHost, "Received an unknown event from the browser: %s", name.utf8());
}

void SessionHost::didReceiveStartAutomationSessionReturn(const String& message)
{
    auto value = JSON::Value::parseJSON(message);
    auto browserInfo = value ? value->asObject() : nullptr;
    if (!browserInfo)
        return;

    // Match the requested browser name and version against the browser's, and report the browser's.
    auto browserName = browserInfo->getString("browserName"_s);
    if (!browserName.isEmpty()) {
        if (m_capabilities.browserName && !equalIgnoringASCIICase(*m_capabilities.browserName, browserName))
            m_browserMatchesCapabilities = false;
        m_capabilities.browserName = browserName;
    }

    auto browserVersion = browserInfo->getString("browserVersion"_s);
    if (!browserVersion.isEmpty()) {
        if (m_capabilities.browserVersion && !WebDriverService::platformCompareBrowserVersions(*m_capabilities.browserVersion, browserVersion))
            m_browserMatchesCapabilities = false;
        m_capabilities.browserVersion = browserVersion;
    }
}

void SessionHost::didReceiveSetTargetList(uint64_t connectionID, const String& message)
{
    auto value = JSON::Value::parseJSON(message);
    auto targetArray = value ? value->asArray() : nullptr;
    if (!targetArray)
        return;

    Vector<Target> targetList;
    for (auto& targetValue : *targetArray) {
        auto targetObject = targetValue->asObject();
        if (!targetObject || targetObject->getString("type"_s) != "automation"_s)
            continue;
        auto targetID = targetObject->getInteger("targetID"_s);
        if (!targetID)
            continue;
        targetList.append({ static_cast<uint64_t>(*targetID), targetObject->getString("name"_s).utf8(), targetObject->getBoolean("isPaired"_s).value_or(false) });
    }
    setTargetList(connectionID, WTF::move(targetList));
}

void SessionHost::setTargetList(uint64_t connectionID, Vector<Target>&& targetList)
{
    if (m_connectionID && m_connectionID != connectionID)
        return;

    // An empty target list means that the automation target went away.
    if (targetList.isEmpty()) {
        connectionDidClose();
        return;
    }

    m_target = targetList[0];
    if (m_connectionID || !m_startSessionCompletionHandler)
        return;

    m_connectionID = connectionID;

    auto setupEvent = JSON::Object::create();
    setupEvent->setString("event"_s, "Setup"_s);
    setupEvent->setInteger("connectionID"_s, m_connectionID);
    setupEvent->setInteger("targetID"_s, m_target.id);
    sendEvent(setupEvent);

    auto completionHandler = std::exchange(m_startSessionCompletionHandler, nullptr);
    completionHandler(m_browserMatchesCapabilities, std::nullopt);
}

void SessionHost::startAutomationSession(Function<void(bool, std::optional<String>)>&& completionHandler)
{
    ASSERT(!m_startSessionCompletionHandler);
    if (!m_channel) {
        completionHandler(false, "Not connected to the browser."_s);
        return;
    }

    m_startSessionCompletionHandler = WTF::move(completionHandler);
    m_sessionID = createVersion4UUIDString();

    auto capabilitiesObject = JSON::Object::create();
    capabilitiesObject->setBoolean("acceptInsecureCerts"_s, m_capabilities.acceptInsecureCerts.value_or(false));

    auto messageObject = JSON::Object::create();
    messageObject->setString("sessionID"_s, m_sessionID);
    messageObject->setObject("capabilities"_s, WTF::move(capabilitiesObject));

    auto startEvent = JSON::Object::create();
    startEvent->setString("event"_s, "StartAutomationSession"_s);
    startEvent->setString("message"_s, messageObject->toJSONString());
    sendEvent(startEvent);

    RunLoop::mainSingleton().dispatchAfter(startAutomationSessionTimeout, [weakThis = WeakPtr { *this }] {
        RefPtr protectedThis = weakThis;
        if (!protectedThis || !protectedThis->m_startSessionCompletionHandler)
            return;
        auto completionHandler = std::exchange(protectedThis->m_startSessionCompletionHandler, nullptr);
        completionHandler(false, "Timed out waiting for the browser to start the automation session."_s);
    });
}

void SessionHost::sendMessageToBackend(const String& message)
{
    auto messageEvent = JSON::Object::create();
    messageEvent->setString("event"_s, "SendMessageToBackend"_s);
    messageEvent->setInteger("connectionID"_s, m_connectionID);
    messageEvent->setInteger("targetID"_s, m_target.id);
    messageEvent->setString("message"_s, message);
    sendEvent(messageEvent);
}

void SessionHost::connectionDidClose()
{
    if (!m_channel)
        return;

    dispatch_io_close(m_channel.get(), DISPATCH_IO_STOP);
    m_channel = nullptr;
    m_receiveBuffer.clear();

    if (auto completionHandler = std::exchange(m_startSessionCompletionHandler, nullptr))
        completionHandler(false, "The browser closed the connection."_s);

    inspectorDisconnected();
}

} // namespace WebDriver

#endif // PLATFORM(COCOA)
