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

#include "Actions.h"
#include "Capabilities.h"
#include "SessionHost.h"
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/JSONValues.h>
#include <wtf/ListHashSet.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/StdLibExtras.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/WTFString.h>

#if ENABLE(WEBDRIVER_BIDI)
#include "WebSocketServer.h"
#endif

namespace WebDriver {

class CommandResult;
class SessionHost;

class Session :
#if ENABLE(WEBDRIVER_BIDI)
public BidiMessageHandler // Inherits RefCounted
#else
public RefCounted<Session>
#endif
{
public:
    static Ref<Session> create(Ref<SessionHost>&& host)
    {
        return adoptRef(*new Session(WTF::move(host)));
    }
#if ENABLE(WEBDRIVER_BIDI)
    static Ref<Session> create(Ref<SessionHost>&& host, WeakPtr<WebSocketServer> bidiServer)
    {
        return adoptRef(*new Session(WTF::move(host), WTF::move(bidiServer)));
    }
#endif
    virtual ~Session();

    const RefPtr<StringImpl>& id() const;
    const Capabilities& capabilities() const;
    bool isConnected() const;
#if ENABLE(WEBDRIVER_BIDI)
    bool hasBiDiEnabled() const { return m_hasBiDiEnabled; };
    void setHasBiDiEnabled(bool flag) { m_hasBiDiEnabled = flag; }
#endif
    double scriptTimeout() const { return m_scriptTimeout; }
    double pageLoadTimeout() const { return m_pageLoadTimeout; }
    double implicitWaitTimeout() const { return m_implicitWaitTimeout; }
    static const RefPtr<StringImpl>& webElementIdentifier();
    static const RefPtr<StringImpl>& shadowRootIdentifier();

    enum class FindElementsMode { Single,
        Multiple };
    enum class ExecuteScriptMode { Sync,
        Async };
    enum class ElementIsShadowRoot : bool { No,
        Yes };

    struct Cookie {
        RefPtr<StringImpl> name;
        RefPtr<StringImpl> value;
        std::optional<RefPtr<StringImpl>> path;
        std::optional<RefPtr<StringImpl>> domain;
        std::optional<bool> secure;
        std::optional<bool> httpOnly;
        std::optional<uint64_t> expiry;
        std::optional<RefPtr<StringImpl>> sameSite;
    };

    InputSource& getOrCreateInputSource(StringImpl* id, InputSource::Type, std::optional<PointerType>);

    void waitForNavigationToComplete(Function<void(CommandResult&&)>&&);
    void createTopLevelBrowsingContext(Function<void(CommandResult&&)>&&);
    void close(Function<void(CommandResult&&)>&&);
    void getTimeouts(Function<void(CommandResult&&)>&&);
    void setTimeouts(const Timeouts&, Function<void(CommandResult&&)>&&);

    void go(StringImpl* url, Function<void(CommandResult&&)>&&);
    void getCurrentURL(Function<void(CommandResult&&)>&&);
    void back(Function<void(CommandResult&&)>&&);
    void forward(Function<void(CommandResult&&)>&&);
    void refresh(Function<void(CommandResult&&)>&&);
    void getTitle(Function<void(CommandResult&&)>&&);
    void getWindowHandle(Function<void(CommandResult&&)>&&);
    void closeWindow(Function<void(CommandResult&&)>&&);
    void switchToWindow(StringImpl* windowHandle, Function<void(CommandResult&&)>&&);
    void getWindowHandles(Function<void(CommandResult&&)>&&);
    void newWindow(std::optional<RefPtr<StringImpl>> typeHint, Function<void(CommandResult&&)>&&);
    void switchToFrame(RefPtr<JSON::Value>&&, Function<void(CommandResult&&)>&&);
    void switchToParentFrame(Function<void(CommandResult&&)>&&);
    void getWindowRect(Function<void(CommandResult&&)>&&);
    void setWindowRect(std::optional<double> x, std::optional<double> y, std::optional<double> width, std::optional<double> height, Function<void(CommandResult&&)>&&);
    void maximizeWindow(Function<void(CommandResult&&)>&&);
    void minimizeWindow(Function<void(CommandResult&&)>&&);
    void fullscreenWindow(Function<void(CommandResult&&)>&&);
    void findElements(StringImpl* strategy, StringImpl* selector, FindElementsMode, StringImpl* rootElementID, ElementIsShadowRoot, Function<void(CommandResult&&)>&&);
    void getActiveElement(Function<void(CommandResult&&)>&&);
    void getElementShadowRoot(StringImpl* elementID, Function<void(CommandResult&&)>&&);
    void isElementSelected(StringImpl* elementID, Function<void(CommandResult&&)>&&);
    void getElementAttribute(StringImpl* elementID, StringImpl* attribute, Function<void(CommandResult&&)>&&);
    void getElementProperty(StringImpl* elementID, StringImpl* attribute, Function<void(CommandResult&&)>&&);
    void getElementCSSValue(StringImpl* elementID, StringImpl* cssProperty, Function<void(CommandResult&&)>&&);
    void getElementText(StringImpl* elementID, Function<void(CommandResult&&)>&&);
    void getElementTagName(StringImpl* elementID, Function<void(CommandResult&&)>&&);
    void getElementRect(StringImpl* elementID, Function<void(CommandResult&&)>&&);
    void isElementEnabled(StringImpl* elementID, Function<void(CommandResult&&)>&&);
    void getComputedRole(StringImpl* elementID, Function<void(CommandResult&&)>&&);
    void getComputedLabel(StringImpl* elementID, Function<void(CommandResult&&)>&&);
    void consumeUserActivation(Function<void(CommandResult&&)>&&);
    void isElementDisplayed(StringImpl* elementID, Function<void(CommandResult&&)>&&);
    void elementClick(StringImpl* elementID, Function<void(CommandResult&&)>&&);
    void elementClear(StringImpl* elementID, Function<void(CommandResult&&)>&&);
    void elementSendKeys(StringImpl* elementID, StringImpl* text, Function<void(CommandResult&&)>&&);
    void getPageSource(Function<void(CommandResult&&)>&&);
    void executeScript(StringImpl* script, RefPtr<JSON::Array>&& arguments, ExecuteScriptMode, Function<void(CommandResult&&)>&&);
    void getAllCookies(Function<void(CommandResult&&)>&&);
    void getNamedCookie(StringImpl* name, Function<void(CommandResult&&)>&&);
    void addCookie(const Cookie&, Function<void(CommandResult&&)>&&);
    void deleteCookie(StringImpl* name, Function<void(CommandResult&&)>&&);
    void deleteAllCookies(Function<void(CommandResult&&)>&&);
    void performActions(Vector<Vector<Action>>&&, Function<void(CommandResult&&)>&&);
    void releaseActions(Function<void(CommandResult&&)>&&);
    void dismissAlert(Function<void(CommandResult&&)>&&);
    void acceptAlert(Function<void(CommandResult&&)>&&);
    void getAlertText(Function<void(CommandResult&&)>&&);
    void sendAlertText(StringImpl*, Function<void(CommandResult&&)>&&);
    void takeScreenshot(std::optional<RefPtr<StringImpl>> elementID, std::optional<bool> scrollIntoView, Function<void(CommandResult&&)>&&);

#if ENABLE(WEBDRIVER_BIDI)
    void dispatchBidiMessage(RefPtr<JSON::Object>&&);
    void relayBidiCommand(StringImpl*, unsigned commandId, Function<void(WebSocketMessageHandler::Message&&)>&&);
#endif

private:
    Session(Ref<SessionHost>&&);
#if ENABLE(WEBDRIVER_BIDI)
    Session(Ref<SessionHost>&&, WeakPtr<WebSocketServer>&&);
#endif

    RefPtr<StringImpl> uncheckedTopLevelBrowsingContext() const;
    void switchToTopLevelBrowsingContext(StringImpl*);
    void switchToBrowsingContext(StringImpl*, Function<void(CommandResult&&)>&&);
    void switchToBrowsingContext(StringImpl* toplevelBrowsingContext, StringImpl* browsingContext, Function<void(CommandResult&&)>&&);
    void closeTopLevelBrowsingContext(StringImpl* toplevelBrowsingContext, Function<void(CommandResult&&)>&&);
    void closeAllToplevelBrowsingContexts(StringImpl* toplevelBrowsingContext, Function<void(CommandResult&&)>&&);

    void getToplevelBrowsingContextRect(Function<void(CommandResult&&)>&&);

    std::optional<RefPtr<StringImpl>> pageLoadStrategyString() const;
    bool clearBrowsingContextsOnError(const CommandResult&);

    void handleUserPrompts(Function<void(CommandResult&&)>&&);
    void handleUnexpectedAlertOpen(Function<void(CommandResult&&)>&&);
    void dismissAndNotifyAlert(Function<void(CommandResult&&)>&&);
    void acceptAndNotifyAlert(Function<void(CommandResult&&)>&&);
    void reportUnexpectedAlertOpen(Function<void(CommandResult&&)>&&);

    RefPtr<JSON::Object> createElement(RefPtr<JSON::Value>&&);
    Ref<JSON::Object> createElement(StringImpl* elementID);
    RefPtr<JSON::Object> createShadowRoot(RefPtr<JSON::Value>&&);
    RefPtr<JSON::Object> extractElement(const JSON::Value&);
    RefPtr<StringImpl> extractElementID(const JSON::Value&);
    std::expected<Ref<JSON::Value>, CommandResult> replaceReferences(Ref<JSON::Value>&&);
    std::expected<Ref<JSON::Value>, CommandResult> replaceReferences(Ref<JSON::Value>&&, HashSet<Ref<JSON::Value>>&);
    Ref<JSON::Value> handleScriptResult(Ref<JSON::Value>&&);
    void elementIsEditable(StringImpl* elementID, Function<void(CommandResult&&)>&&);

    struct Point {
        int x { 0 };
        int y { 0 };
    };

    struct Size {
        int width { 0 };
        int height { 0 };
    };

    struct Rect {
        Point origin;
        Size size;
    };

    enum class ElementLayoutOption {
        ScrollIntoViewIfNeeded = 1 << 0,
        UseViewportCoordinates = 1 << 1,
    };
    void computeElementLayout(StringImpl* elementID, OptionSet<ElementLayoutOption>, Function<void(std::optional<Rect>&&, std::optional<Point>&&, bool, RefPtr<JSON::Object>&&)>&&);

    void elementIsFileUpload(StringImpl* elementID, Function<void(CommandResult&&)>&&);
    enum class FileUploadType { Single,
        Multiple };
    std::optional<FileUploadType> parseElementIsFileUploadResult(const RefPtr<JSON::Value>&);
    void selectOptionElement(StringImpl* elementID, Function<void(CommandResult&&)>&&);
    void setInputFileUploadFiles(StringImpl* elementID, StringImpl* text, bool multiple, Function<void(CommandResult&&)>&&);
    void didSetInputFileUploadFiles(bool wasCancelled);

    void performMouseInteraction(int x, int y, MouseButton, MouseInteraction, Function<void(CommandResult&&)>&&);

    enum class KeyboardInteractionType { KeyPress,
        KeyRelease,
        InsertByKey };
    struct KeyboardInteraction {
        KeyboardInteractionType type { KeyboardInteractionType::InsertByKey };
        std::optional<RefPtr<StringImpl>> text;
        std::optional<RefPtr<StringImpl>> key;
    };
    enum KeyModifier {
        None = 0,
        Shift = 1 << 0,
        Control = 1 << 1,
        Alternate = 1 << 2,
        Meta = 1 << 3,
    };
    RefPtr<StringImpl> virtualKeyForKey(char16_t, KeyModifier&);
    void performKeyboardInteractions(Vector<KeyboardInteraction>&&, Function<void(CommandResult&&)>&&);

    struct InputSourceState {
        enum class Type { Null,
            Key,
            Pointer };

        Type type;
        RefPtr<StringImpl> subtype;
        std::optional<MouseButton> pressedButton;
        std::optional<MouseInteraction> mouseInteraction;
        // Ordered so that the last entry is the most recently pressed key, which is what
        // 'pressedCharKey' conveys.
        ListHashSet<RefPtr<StringImpl>> pressedKeys;
        HashSet<RefPtr<StringImpl>> pressedVirtualKeys;
    };
    InputSourceState& inputSourceState(StringImpl* id);

    RefPtr<SessionHost> m_host;
    double m_scriptTimeout;
    double m_pageLoadTimeout;
    double m_implicitWaitTimeout;
    std::optional<RefPtr<StringImpl>> m_toplevelBrowsingContext;
    std::optional<RefPtr<StringImpl>> m_currentBrowsingContext;
    std::optional<RefPtr<StringImpl>> m_currentParentBrowsingContext;
    HashMap<RefPtr<StringImpl>, InputSource> m_activeInputSources;
    HashMap<RefPtr<StringImpl>, InputSourceState> m_inputStateTable;
#if ENABLE(WEBDRIVER_BIDI)
    bool m_hasBiDiEnabled { false };

    WeakPtr<WebSocketServer> m_bidiServer;

    // Actual event handlers
    RefPtr<JSON::Object> processLogEntryAdded(RefPtr<JSON::Object>&&);
#endif
};

} // WebDriver
