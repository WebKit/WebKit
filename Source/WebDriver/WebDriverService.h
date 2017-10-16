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

#pragma once

#include "HTTPServer.h"
#include "Session.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>

namespace Inspector {
class InspectorArray;
class InspectorObject;
}

namespace WebDriver {

struct Capabilities;
class CommandResult;
class Session;

class WebDriverService final : public HTTPRequestHandler {
public:
    WebDriverService();
    ~WebDriverService() = default;

    int run(int argc, char** argv);

    static bool platformCompareBrowserVersions(const String&, const String&);

private:
    enum class HTTPMethod { Get, Post, Delete };
    typedef void (WebDriverService::*CommandHandler)(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    struct Command {
        HTTPMethod method;
        const char* uriTemplate;
        CommandHandler handler;
    };
    static const Command s_commands[];

    static std::optional<HTTPMethod> toCommandHTTPMethod(const String& method);
    static bool findCommand(const String& method, const String& path, CommandHandler*, HashMap<String, String>& parameters);

    void newSession(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void deleteSession(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void setTimeouts(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void go(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void getCurrentURL(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void back(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void forward(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void refresh(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void getTitle(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void getWindowHandle(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void closeWindow(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void switchToWindow(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void getWindowHandles(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void switchToFrame(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void switchToParentFrame(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void getWindowRect(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void setWindowRect(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void findElement(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void findElements(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void findElementFromElement(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void findElementsFromElement(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void isElementSelected(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void getElementText(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void getElementTagName(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void getElementRect(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void isElementEnabled(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void isElementDisplayed(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void getElementAttribute(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void elementClick(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void elementClear(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void elementSendKeys(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void elementSubmit(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void executeScript(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void executeAsyncScript(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void getAllCookies(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void getNamedCookie(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void addCookie(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void deleteCookie(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void deleteAllCookies(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void dismissAlert(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void acceptAlert(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void getAlertText(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void sendAlertText(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void takeScreenshot(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);
    void takeElementScreenshot(RefPtr<Inspector::InspectorObject>&&, Function<void (CommandResult&&)>&&);

    static Capabilities platformCapabilities();
    RefPtr<Inspector::InspectorObject> processCapabilities(const Inspector::InspectorObject&, Function<void (CommandResult&&)>&) const;
    RefPtr<Inspector::InspectorObject> validatedCapabilities(const Inspector::InspectorObject&) const;
    RefPtr<Inspector::InspectorObject> mergeCapabilities(const Inspector::InspectorObject&, const Inspector::InspectorObject&) const;
    std::optional<String> matchCapabilities(const Inspector::InspectorObject&) const;
    bool platformValidateCapability(const String&, const RefPtr<Inspector::InspectorValue>&) const;
    std::optional<String> platformMatchCapability(const String&, const RefPtr<Inspector::InspectorValue>&) const;
    void parseCapabilities(const Inspector::InspectorObject& desiredCapabilities, Capabilities&) const;
    void platformParseCapabilities(const Inspector::InspectorObject& desiredCapabilities, Capabilities&) const;
    RefPtr<Session> findSessionOrCompleteWithError(Inspector::InspectorObject&, Function<void (CommandResult&&)>&);

    void handleRequest(HTTPRequestHandler::Request&&, Function<void (HTTPRequestHandler::Response&&)>&& replyHandler) override;
    void sendResponse(Function<void (HTTPRequestHandler::Response&&)>&& replyHandler, CommandResult&&) const;

    HTTPServer m_server;
    HashMap<String, RefPtr<Session>> m_sessions;
    Session* m_activeSession { nullptr };
};

} // namespace WebDriver
