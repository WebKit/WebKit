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
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/JSONValues.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebDriver {

class CommandResult;
class SessionHost;

class Session : public RefCounted<Session> {
public:
    static Ref<Session> create(std::unique_ptr<SessionHost>&& host)
    {
        return adoptRef(*new Session(WTFMove(host)));
    }
    ~Session();

    const String& id() const;
    const Capabilities& capabilities() const;
    bool isConnected() const;
    Seconds scriptTimeout() const  { return m_scriptTimeout; }
    Seconds pageLoadTimeout() const { return m_pageLoadTimeout; }
    Seconds implicitWaitTimeout() const { return m_implicitWaitTimeout; }
    static const String& webElementIdentifier();

    enum class FindElementsMode { Single, Multiple };
    enum class ExecuteScriptMode { Sync, Async };

    struct Cookie {
        String name;
        String value;
        std::optional<String> path;
        std::optional<String> domain;
        std::optional<bool> secure;
        std::optional<bool> httpOnly;
        std::optional<uint64_t> expiry;
    };

    InputSource& getOrCreateInputSource(const String& id, InputSource::Type, std::optional<PointerType>);

    void waitForNavigationToComplete(Function<void (CommandResult&&)>&&);
    void createTopLevelBrowsingContext(Function<void (CommandResult&&)>&&);
    void close(Function<void (CommandResult&&)>&&);
    void getTimeouts(Function<void (CommandResult&&)>&&);
    void setTimeouts(const Timeouts&, Function<void (CommandResult&&)>&&);

    void go(const String& url, Function<void (CommandResult&&)>&&);
    void getCurrentURL(Function<void (CommandResult&&)>&&);
    void back(Function<void (CommandResult&&)>&&);
    void forward(Function<void (CommandResult&&)>&&);
    void refresh(Function<void (CommandResult&&)>&&);
    void getTitle(Function<void (CommandResult&&)>&&);
    void getWindowHandle(Function<void (CommandResult&&)>&&);
    void closeWindow(Function<void (CommandResult&&)>&&);
    void switchToWindow(const String& windowHandle, Function<void (CommandResult&&)>&&);
    void getWindowHandles(Function<void (CommandResult&&)>&&);
    void switchToFrame(RefPtr<JSON::Value>&&, Function<void (CommandResult&&)>&&);
    void switchToParentFrame(Function<void (CommandResult&&)>&&);
    void getWindowRect(Function<void (CommandResult&&)>&&);
    void setWindowRect(std::optional<double> x, std::optional<double> y, std::optional<double> width, std::optional<double> height, Function<void (CommandResult&&)>&&);
    void maximizeWindow(Function<void (CommandResult&&)>&&);
    void minimizeWindow(Function<void (CommandResult&&)>&&);
    void fullscreenWindow(Function<void (CommandResult&&)>&&);
    void findElements(const String& strategy, const String& selector, FindElementsMode, const String& rootElementID, Function<void (CommandResult&&)>&&);
    void getActiveElement(Function<void (CommandResult&&)>&&);
    void isElementSelected(const String& elementID, Function<void (CommandResult&&)>&&);
    void getElementAttribute(const String& elementID, const String& attribute, Function<void (CommandResult&&)>&&);
    void getElementProperty(const String& elementID, const String& attribute, Function<void (CommandResult&&)>&&);
    void getElementCSSValue(const String& elementID, const String& cssProperty, Function<void (CommandResult&&)>&&);
    void getElementText(const String& elementID, Function<void (CommandResult&&)>&&);
    void getElementTagName(const String& elementID, Function<void (CommandResult&&)>&&);
    void getElementRect(const String& elementID, Function<void (CommandResult&&)>&&);
    void isElementEnabled(const String& elementID, Function<void (CommandResult&&)>&&);
    void isElementDisplayed(const String& elementID, Function<void (CommandResult&&)>&&);
    void elementClick(const String& elementID, Function<void (CommandResult&&)>&&);
    void elementClear(const String& elementID, Function<void (CommandResult&&)>&&);
    void elementSendKeys(const String& elementID, const String& text, Function<void (CommandResult&&)>&&);
    void executeScript(const String& script, RefPtr<JSON::Array>&& arguments, ExecuteScriptMode, Function<void (CommandResult&&)>&&);
    void getAllCookies(Function<void (CommandResult&&)>&&);
    void getNamedCookie(const String& name, Function<void (CommandResult&&)>&&);
    void addCookie(const Cookie&, Function<void (CommandResult&&)>&&);
    void deleteCookie(const String& name, Function<void (CommandResult&&)>&&);
    void deleteAllCookies(Function<void (CommandResult&&)>&&);
    void performActions(Vector<Vector<Action>>&&, Function<void (CommandResult&&)>&&);
    void releaseActions(Function<void (CommandResult&&)>&&);
    void dismissAlert(Function<void (CommandResult&&)>&&);
    void acceptAlert(Function<void (CommandResult&&)>&&);
    void getAlertText(Function<void (CommandResult&&)>&&);
    void sendAlertText(const String&, Function<void (CommandResult&&)>&&);
    void takeScreenshot(std::optional<String> elementID, std::optional<bool> scrollIntoView, Function<void (CommandResult&&)>&&);

private:
    Session(std::unique_ptr<SessionHost>&&);

    void switchToTopLevelBrowsingContext(std::optional<String>);
    void switchToBrowsingContext(std::optional<String>);
    void closeTopLevelBrowsingContext(const String& toplevelBrowsingContext, Function<void (CommandResult&&)>&&);
    void closeAllToplevelBrowsingContexts(const String& toplevelBrowsingContext, Function<void (CommandResult&&)>&&);

    void getToplevelBrowsingContextRect(Function<void (CommandResult&&)>&&);

    std::optional<String> pageLoadStrategyString() const;

    void handleUserPrompts(Function<void (CommandResult&&)>&&);
    void handleUnexpectedAlertOpen(Function<void (CommandResult&&)>&&);
    void dismissAndNotifyAlert(Function<void (CommandResult&&)>&&);
    void acceptAndNotifyAlert(Function<void (CommandResult&&)>&&);
    void reportUnexpectedAlertOpen(Function<void (CommandResult&&)>&&);

    RefPtr<JSON::Object> createElement(RefPtr<JSON::Value>&&);
    RefPtr<JSON::Object> createElement(const String& elementID);
    RefPtr<JSON::Object> extractElement(JSON::Value&);
    String extractElementID(JSON::Value&);
    RefPtr<JSON::Value> handleScriptResult(RefPtr<JSON::Value>&&);

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
    void computeElementLayout(const String& elementID, OptionSet<ElementLayoutOption>, Function<void (std::optional<Rect>&&, std::optional<Point>&&, bool, RefPtr<JSON::Object>&&)>&&);

    void selectOptionElement(const String& elementID, Function<void (CommandResult&&)>&&);

    enum class MouseInteraction { Move, Down, Up, SingleClick, DoubleClick };
    void performMouseInteraction(int x, int y, MouseButton, MouseInteraction, Function<void (CommandResult&&)>&&);

    enum class KeyboardInteractionType { KeyPress, KeyRelease, InsertByKey };
    struct KeyboardInteraction {
        KeyboardInteractionType type { KeyboardInteractionType::InsertByKey };
        std::optional<String> text;
        std::optional<String> key;
    };
    enum KeyModifier {
        None = 0,
        Shift = 1 << 0,
        Control = 1 << 1,
        Alternate = 1 << 2,
        Meta = 1 << 3,
    };
    String virtualKeyForKey(UChar, KeyModifier&);
    void performKeyboardInteractions(Vector<KeyboardInteraction>&&, Function<void (CommandResult&&)>&&);

    struct InputSourceState {
        enum class Type { Null, Key, Pointer };

        Type type;
        String subtype;
        std::optional<MouseButton> pressedButton;
        std::optional<String> pressedKey;
        std::optional<String> pressedVirtualKey;
    };
    InputSourceState& inputSourceState(const String& id);

    std::unique_ptr<SessionHost> m_host;
    Seconds m_scriptTimeout;
    Seconds m_pageLoadTimeout;
    Seconds m_implicitWaitTimeout;
    std::optional<String> m_toplevelBrowsingContext;
    std::optional<String> m_currentBrowsingContext;
    HashMap<String, InputSource> m_activeInputSources;
    HashMap<String, InputSourceState> m_inputStateTable;
};

} // WebDriver
