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
#include <wtf/JSONValues.h>
#include <wtf/text/StringHash.h>

#if ENABLE(WEBDRIVER_BIDI)
#include "WebSocketServer.h"
#endif

namespace WebDriver {

struct Capabilities;
class CommandResult;
class Session;

class WebDriverService final : public HTTPRequestHandler
#if ENABLE(WEBDRIVER_BIDI)
    , public WebSocketMessageHandler
#endif
{
public:
    WebDriverService();
    ~WebDriverService();

    int run(int argc, char** argv);

    static void platformInit();
    static bool platformCompareBrowserVersions(const String&, const String&);

    RefPtr<Session> session() const { return m_session; }

private:
    enum class HTTPMethod { Get, Post, Delete };
    typedef void (WebDriverService::*CommandHandler)(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    struct Command {
        HTTPMethod method;
        const char* uriTemplate;
        CommandHandler handler;
    };
    static const Command s_commands[];

    static std::optional<HTTPMethod> toCommandHTTPMethod(const String& method);
    static bool findCommand(HTTPMethod, const String& path, CommandHandler*, HashMap<String, String>& parameters);

    void newSession(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void deleteSession(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void status(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void getTimeouts(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void setTimeouts(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void go(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void getCurrentURL(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void back(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void forward(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void refresh(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void getTitle(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void getWindowHandle(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void closeWindow(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void switchToWindow(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void getWindowHandles(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void newWindow(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void switchToFrame(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void switchToParentFrame(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void getWindowRect(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void setWindowRect(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void maximizeWindow(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void minimizeWindow(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void fullscreenWindow(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void findElement(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void findElements(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void findElementFromElement(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void findElementsFromElement(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void findElementFromShadowRoot(RefPtr<JSON::Object>&&, Function<void(CommandResult&&)>&&);
    void findElementsFromShadowRoot(RefPtr<JSON::Object>&&, Function<void(CommandResult&&)>&&);
    void getActiveElement(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void getElementShadowRoot(RefPtr<JSON::Object>&&, Function<void(CommandResult&&)>&&);
    void isElementSelected(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void getElementAttribute(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void getElementProperty(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void getElementCSSValue(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void getElementText(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void getElementTagName(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void getElementRect(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void isElementEnabled(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void getComputedRole(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void getComputedLabel(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void isElementDisplayed(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void elementClick(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void elementClear(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void elementSendKeys(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void getPageSource(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void executeScript(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void executeAsyncScript(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void getAllCookies(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void getNamedCookie(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void addCookie(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void deleteCookie(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void deleteAllCookies(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void performActions(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void releaseActions(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void dismissAlert(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void acceptAlert(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void getAlertText(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void sendAlertText(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void takeScreenshot(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);
    void takeElementScreenshot(RefPtr<JSON::Object>&&, Function<void (CommandResult&&)>&&);

#if ENABLE(WEBDRIVER_BIDI)
    // BiDi message handlers
    void bidiSessionStatus(unsigned id, RefPtr<JSON::Object>&&, Function<void (WebSocketMessageHandler::Message&&)>&&);
#endif

    static Capabilities platformCapabilities();
    Vector<Capabilities> processCapabilities(const JSON::Object&, Function<void (CommandResult&&)>&) const;
    RefPtr<JSON::Object> validatedCapabilities(const JSON::Object&) const;
    RefPtr<JSON::Object> mergeCapabilities(const JSON::Object&, const JSON::Object&) const;
    RefPtr<JSON::Object> matchCapabilities(const JSON::Object&) const;
    bool platformValidateCapability(const String&, const Ref<JSON::Value>&) const;
    bool platformMatchCapability(const String&, const Ref<JSON::Value>&) const;
    bool platformSupportProxyType(const String&) const;
    bool platformSupportBidi() const;
    void parseCapabilities(const JSON::Object& desiredCapabilities, Capabilities&) const;
    void platformParseCapabilities(const JSON::Object& desiredCapabilities, Capabilities&) const;
    void connectToBrowser(Vector<Capabilities>&&, Function<void (CommandResult&&)>&&);
    void createSession(Vector<Capabilities>&&, Ref<SessionHost>&&, Function<void (CommandResult&&)>&&);
    bool findSessionOrCompleteWithError(JSON::Object&, Function<void (CommandResult&&)>&);

    void handleRequest(HTTPRequestHandler::Request&&, Function<void (HTTPRequestHandler::Response&&)>&& replyHandler) override;
    void sendResponse(Function<void (HTTPRequestHandler::Response&&)>&& replyHandler, CommandResult&&) const;

#if ENABLE(WEBDRIVER_BIDI)
    bool acceptHandshake(HTTPRequestHandler::Request&&) override;
    void handleMessage(WebSocketMessageHandler::Message&&, Function<void (WebSocketMessageHandler::Message&&)>&& replyHandler) override;
    void clientDisconnected(const WebSocketMessageHandler::Connection&) override;

    void onBrowserTerminated(const String& sessionId);

    typedef void (WebDriverService::*BidiCommandHandler)(unsigned id, RefPtr<JSON::Object>&&, Function<void (WebSocketMessageHandler::Message&&)>&&);
    struct BidiCommand {
        String method;
        BidiCommandHandler handler;
    };
    static const BidiCommand s_bidiCommands[];
    static bool findBidiCommand(RefPtr<JSON::Value>&, BidiCommandHandler*, unsigned& id, RefPtr<JSON::Object>& parsedParams);
#endif // ENABLE(WEBDRIVER_BIDI)

    HTTPServer m_server;
#if ENABLE(WEBDRIVER_BIDI)
    WebSocketServer m_bidiServer;
    SessionHost::BrowserTerminatedObserver m_browserTerminatedObserver;
#endif
    RefPtr<Session> m_session;

    String m_targetAddress;
    uint16_t m_targetPort { 0 };
};

} // namespace WebDriver
