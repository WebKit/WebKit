/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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

#include <wtf/NeverDestroyed.h>
#include <wtf/UUID.h>
#include <wtf/text/MakeString.h>

#if PLATFORM(WIN)
#include <shlwapi.h>
#include <ws2tcpip.h>
#endif

namespace WebDriver {

SessionHost::~SessionHost()
{
#if PLATFORM(WIN)
    if (m_browserHandle) {
        ::TerminateProcess(m_browserHandle.get(), 0);
        m_browserHandle = { };
    }
#endif
}

HashMap<String, Inspector::RemoteInspectorConnectionClient::CallHandler>& SessionHost::dispatchMap()
{
    static NeverDestroyed<HashMap<String, CallHandler>> methods = HashMap<String, CallHandler>({
        {"SetTargetList"_s, static_cast<CallHandler>(&SessionHost::receivedSetTargetList)},
        {"SendMessageToFrontend"_s, static_cast<CallHandler>(&SessionHost::receivedSendMessageToFrontend)},
        {"StartAutomationSession_Return"_s, static_cast<CallHandler>(&SessionHost::receivedStartAutomationSessionReturn)},
    });

    return methods;
}

void SessionHost::sendWebInspectorEvent(const String& event)
{
    if (!m_clientID)
        return;

    send(m_clientID.value(), event.utf8().span());
}

#if PLATFORM(WIN)
static std::optional<unsigned> getFreePort()
{
    // This function binds port 0 and immediately closes it,
    // and returns the port number actually bound as a free port.

    // FIXME:
    // There is no guarantee that the port number returned by this function will always be available.
    // Another application may use it immediately.

    auto sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
        return std::nullopt;

    sockaddr_in address = { };
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(0);
    int ret = bind(sock, reinterpret_cast<struct sockaddr*>(&address), sizeof(address));
    if (ret == SOCKET_ERROR) {
        closesocket(sock);
        return std::nullopt;
    }

    address = { };
    socklen_t len = sizeof(address);
    ret = getsockname(sock, reinterpret_cast<struct sockaddr*>(&address), &len);
    if (ret == SOCKET_ERROR) {
        closesocket(sock);
        return std::nullopt;
    }

    auto freePort = ntohs(address.sin_port);
    ::closesocket(sock);
    return freePort;
}

static WTF::Win32Handle launchBrowser()
{
    WCHAR pathStr[MAX_PATH];
    if (!::GetModuleFileName(nullptr, pathStr, std::size(pathStr)))
        return { };

    // Launch MiniBrowser.exe in the same path as WebDriver.exe.
    // FIXME: It would be even better if the WebKit browsers elsewhere instead of MiniBrowser could be specified.
    ::PathRemoveFileSpec(pathStr);
    if (!::PathAppend(pathStr, L"MiniBrowser.exe"))
        return { };

    auto commandLine = makeString('"', String(pathStr), '"');

    STARTUPINFO startupInfo { };
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION processInformation { };
    if (::CreateProcess(0, commandLine.wideCharacters().data(), 0, 0, true, 0, 0, 0, &startupInfo, &processInformation))
        return Win32Handle::adopt(processInformation.hProcess);

    return { };
}
#endif

void SessionHost::connectToBrowser(Function<void (std::optional<String> error)>&& completionHandler)
{
    if (m_clientID) {
        completionHandler(makeString("Already connected to the browser."_s));
        return;
    }

    String targetIp;
    uint16_t targetPort = 0;

    if (!m_targetIp.isEmpty() && m_targetPort) {
        targetIp = m_targetIp;
        targetPort = m_targetPort;
    } else if (m_capabilities.targetAddr && m_capabilities.targetPort) {
        targetIp = m_capabilities.targetAddr.value();
        targetPort = m_capabilities.targetPort.value();
    }

    if (targetIp.isEmpty() || !targetPort) {
#if !PLATFORM(WIN)
        completionHandler(makeString("Target IP/port is invalid, or not specified."_s));
        return;
#else
        // WebDriver will launch MiniBrowser for automation if both "-t" and "--target" options are not specified.
        auto freePort = getFreePort();
        if (!freePort) {
            completionHandler(makeString("Failed to launch MiniBrowser."_s));
            return;
        }

        targetIp = "127.0.0.1"_s;
        targetPort = *freePort;

        auto envVar = makeString(targetIp, ':', targetPort);
        ::SetEnvironmentVariable(L"WEBKIT_INSPECTOR_SERVER", envVar.wideCharacters().data());

        m_browserHandle = launchBrowser();
        if (!m_browserHandle) {
            completionHandler(makeString("Failed to launch MiniBrowser."_s));
            return;
        }
        m_isRemoteBrowser = false;
#endif
    } else
        m_isRemoteBrowser = true;

    m_clientID = connectInet(targetIp.utf8().data(), targetPort);
    if (!m_clientID)
        completionHandler(makeString(targetIp.utf8().span(), ':', targetPort, " is not reachable."_s));
    else
        completionHandler(std::nullopt);
}

bool SessionHost::isConnected() const
{
    return m_clientID.has_value();
}

void SessionHost::didClose(Inspector::RemoteInspectorSocketEndpoint&, Inspector::ConnectionID)
{
    inspectorDisconnected();

#if PLATFORM(WIN)
    if (m_browserHandle) {
        ::TerminateProcess(m_browserHandle.get(), 0);
        m_browserHandle = { };
    }
#endif

    m_clientID = std::nullopt;
}

std::optional<Vector<SessionHost::Target>> SessionHost::parseTargetList(const struct Event& event)
{
    auto result = parseTargetListJSON(*event.message);
    if (!result)
        return std::nullopt;

    Vector<SessionHost::Target> targetList;
    for (const auto& itemObject : *result) {
        SessionHost::Target target;
        String name;
        String type;
        auto targetId = itemObject->getInteger("targetID"_s);
        if (!targetId
            || !itemObject->getString("name"_s, name)
            || !itemObject->getString("type"_s, type)
            || type != "automation"_s)
            continue;

        target.id = *targetId;
        target.name = name.utf8();
        targetList.append(target);
    }
    return targetList;
}

void SessionHost::receivedSetTargetList(const struct Event& event)
{
    ASSERT(isMainThread());

    if (!event.connectionID || !event.message)
        return;

    auto targetList = parseTargetList(event);
    if (targetList)
        setTargetList(*event.connectionID, WTFMove(*targetList));
}

void SessionHost::receivedSendMessageToFrontend(const struct Event& event)
{
    ASSERT(isMainThread());

    if (!event.connectionID || !event.targetID || !event.message)
        return;

    ASSERT(*event.connectionID == m_connectionID && *event.targetID == m_target.id);
    dispatchMessage(event.message.value());
}

void SessionHost::receivedStartAutomationSessionReturn(const struct Event&)
{
    ASSERT(isMainThread());

    m_capabilities.browserName = String::fromUTF8("TODO/browserName");
    m_capabilities.browserVersion = String::fromUTF8("TODO/browserVersion");
}

void SessionHost::startAutomationSession(Function<void (bool, std::optional<String>)>&& completionHandler)
{
    ASSERT(!m_startSessionCompletionHandler);
    m_startSessionCompletionHandler = WTFMove(completionHandler);
    m_sessionID = createVersion4UUIDString();

    auto capabilitiesObject = JSON::Object::create();
    capabilitiesObject->setBoolean("acceptInsecureCerts"_s, m_capabilities.acceptInsecureCerts.value_or(false));

    auto messageObject = JSON::Object::create();
    messageObject->setString("sessionID"_s, m_sessionID);
    messageObject->setObject("capabilities"_s, capabilitiesObject);

    auto sendMessageEvent = JSON::Object::create();
    sendMessageEvent->setString("event"_s, "StartAutomationSession"_s);
    sendMessageEvent->setString("message"_s, messageObject->toJSONString());
    sendWebInspectorEvent(sendMessageEvent->toJSONString());
}

void SessionHost::setTargetList(uint64_t connectionID, Vector<Target>&& targetList)
{
    // The server notifies all its clients when connection is lost by sending an empty target list.
    // We only care about automation connection.
    if (m_connectionID && m_connectionID != connectionID)
        return;

    ASSERT(targetList.size() <= 1);
    if (targetList.isEmpty()) {
        // Disconnected from backend
        m_clientID = std::nullopt;
        inspectorDisconnected();
        if (m_startSessionCompletionHandler) {
            auto startSessionCompletionHandler = std::exchange(m_startSessionCompletionHandler, nullptr);
            startSessionCompletionHandler(true, "received empty target list"_s);
        }
        return;
    }

    m_target = targetList[0];
    if (m_connectionID) {
        ASSERT(m_connectionID == connectionID);
        return;
    }

    if (!m_startSessionCompletionHandler) {
        // Session creation was already rejected.
        return;
    }

    m_connectionID = connectionID;

    auto sendEvent = JSON::Object::create();
    sendEvent->setString("event"_s, "Setup"_s);
    sendEvent->setInteger("connectionID"_s, m_connectionID);
    sendEvent->setInteger("targetID"_s, m_target.id);
    sendWebInspectorEvent(sendEvent->toJSONString());

    auto startSessionCompletionHandler = std::exchange(m_startSessionCompletionHandler, nullptr);
    startSessionCompletionHandler(true, std::nullopt);
}

void SessionHost::sendMessageToBackend(const String& message)
{
    auto sendEvent = JSON::Object::create();
    sendEvent->setString("event"_s, "SendMessageToBackend"_s);
    sendEvent->setInteger("connectionID"_s, m_connectionID);
    sendEvent->setInteger("targetID"_s, m_target.id);
    sendEvent->setString("message"_s, message);
    sendWebInspectorEvent(sendEvent->toJSONString());
}

} // namespace WebDriver
