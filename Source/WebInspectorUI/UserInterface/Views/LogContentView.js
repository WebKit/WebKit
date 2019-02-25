/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

// FIXME: <https://webkit.org/b/143545> Web Inspector: LogContentView should use higher level objects

WI.LogContentView = class LogContentView extends WI.ContentView
{
    constructor(representedObject)
    {
        super(representedObject);

        this._nestingLevel = 0;
        this._selectedMessages = [];

        // FIXME: Try to use a marker, instead of a list of messages that get re-added.
        this._provisionalMessages = [];

        this.element.classList.add("log");

        this.messagesElement = document.createElement("div");
        this.messagesElement.classList.add("console-messages");
        this.messagesElement.tabIndex = 0;
        this.messagesElement.setAttribute("role", "log");
        this.messagesElement.addEventListener("mousedown", this._mousedown.bind(this));
        this.messagesElement.addEventListener("keydown", this._keyDown.bind(this));
        this.messagesElement.addEventListener("keypress", this._keyPress.bind(this));
        this.messagesElement.addEventListener("dragstart", this._ondragstart.bind(this), true);
        this.element.appendChild(this.messagesElement);

        this.prompt = WI.quickConsole.prompt;

        this._keyboardShortcutCommandA = new WI.KeyboardShortcut(WI.KeyboardShortcut.Modifier.CommandOrControl, "A");
        this._keyboardShortcutEsc = new WI.KeyboardShortcut(null, WI.KeyboardShortcut.Key.Escape);

        this._logViewController = new WI.JavaScriptLogViewController(this.messagesElement, this.element, this.prompt, this, "console-prompt-history");
        this._lastMessageView = null;

        const fixed = true;
        this._findBanner = new WI.FindBanner(this, "console-find-banner", fixed);
        this._findBanner.targetElement = this.element;

        this._currentSearchQuery = "";
        this._searchMatches = [];
        this._selectedSearchMatch = null;
        this._selectedSearchMatchIsValid = false;

        this._preserveLogNavigationItem = new WI.CheckboxNavigationItem("preserve-log", WI.UIString("Preserve Log"), !WI.settings.clearLogOnNavigate.value);
        this._preserveLogNavigationItem.tooltip = WI.UIString("Do not clear the console on new page loads");
        this._preserveLogNavigationItem.addEventListener(WI.CheckboxNavigationItem.Event.CheckedDidChange, () => {
            WI.settings.clearLogOnNavigate.value = !WI.settings.clearLogOnNavigate.value;
        });
        WI.settings.clearLogOnNavigate.addEventListener(WI.Setting.Event.Changed, this._handleClearLogOnNavigateSettingChanged, this);

        this._emulateInUserGestureNavigationItem = new WI.CheckboxNavigationItem("emulate-in-user-gesture", WI.UIString("Emulate User Gesture"), WI.settings.emulateInUserGesture.value);
        this._emulateInUserGestureNavigationItem.tooltip = WI.UIString("Run console commands as if inside a user gesture");
        this._emulateInUserGestureNavigationItem.addEventListener(WI.CheckboxNavigationItem.Event.CheckedDidChange, () => {
            WI.settings.emulateInUserGesture.value = !WI.settings.emulateInUserGesture.value;
        });
        WI.settings.emulateInUserGesture.addEventListener(WI.Setting.Event.Changed, this._handleEmulateInUserGestureSettingChanged, this);

        this._checkboxesNavigationItemGroup = new WI.GroupNavigationItem([this._preserveLogNavigationItem, this._emulateInUserGestureNavigationItem, new WI.DividerNavigationItem]);

        let scopeBarItems = [
            new WI.ScopeBarItem(WI.LogContentView.Scopes.All, WI.UIString("All"), {exclusive: true}),
            new WI.ScopeBarItem(WI.LogContentView.Scopes.Errors, WI.UIString("Errors"), {className: "errors"}),
            new WI.ScopeBarItem(WI.LogContentView.Scopes.Warnings, WI.UIString("Warnings"), {className: "warnings"}),
            new WI.ScopeBarItem(WI.LogContentView.Scopes.Logs, WI.UIString("Logs"), {className: "logs"}),
            new WI.ScopeBarItem(WI.LogContentView.Scopes.Infos, WI.UIString("Infos"), {className: "infos", hidden: true}),
            new WI.ScopeBarItem(WI.LogContentView.Scopes.Debugs, WI.UIString("Debugs"), {className: "debugs", hidden: true}),
        ];

        this._scopeBar = new WI.ScopeBar("log-scope-bar", scopeBarItems, scopeBarItems[0]);
        this._scopeBar.addEventListener(WI.ScopeBar.Event.SelectionChanged, this._scopeBarSelectionDidChange, this);

        this._hasNonDefaultLogChannelMessage = false;
        if (WI.ConsoleManager.supportsLogChannels()) {
            let messageChannelBarItems = [
                new WI.ScopeBarItem(WI.LogContentView.Scopes.AllChannels, WI.UIString("All"), {exclusive: true}),
                new WI.ScopeBarItem(WI.LogContentView.Scopes.Media, WI.UIString("Media"), {className: "media"}),
                new WI.ScopeBarItem(WI.LogContentView.Scopes.MediaSource, WI.UIString("MediaSource"), {className: "mediasource"}),
                new WI.ScopeBarItem(WI.LogContentView.Scopes.WebRTC, WI.UIString("WebRTC"), {className: "webrtc"}),
            ];

            this._messageSourceBar = new WI.ScopeBar("message-channel-scope-bar", messageChannelBarItems, messageChannelBarItems[0]);
            this._messageSourceBar.addEventListener(WI.ScopeBar.Event.SelectionChanged, this._messageSourceBarSelectionDidChange, this);
        }

        this._garbageCollectNavigationItem = new WI.ButtonNavigationItem("garbage-collect", WI.UIString("Collect garbage"), "Images/NavigationItemGarbageCollect.svg", 16, 16);
        this._garbageCollectNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
        this._garbageCollectNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._garbageCollect, this);

        this._clearLogNavigationItem = new WI.ButtonNavigationItem("clear-log", WI.UIString("Clear log (%s or %s)").format(WI.clearKeyboardShortcut.displayName, this._logViewController.messagesAlternateClearKeyboardShortcut.displayName), "Images/NavigationItemTrash.svg", 15, 15);
        this._clearLogNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
        this._clearLogNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._clearLog, this);

        this._showConsoleTabNavigationItem = new WI.ButtonNavigationItem("show-tab", WI.UIString("Show Console tab"), "Images/SplitToggleUp.svg", 16, 16);
        this._showConsoleTabNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.High;
        this._showConsoleTabNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._showConsoleTab, this);

        this.messagesElement.addEventListener("contextmenu", this._handleContextMenuEvent.bind(this), false);

        WI.consoleManager.addEventListener(WI.ConsoleManager.Event.SessionStarted, this._sessionStarted, this);
        WI.consoleManager.addEventListener(WI.ConsoleManager.Event.MessageAdded, this._messageAdded, this);
        WI.consoleManager.addEventListener(WI.ConsoleManager.Event.PreviousMessageRepeatCountUpdated, this._previousMessageRepeatCountUpdated, this);
        WI.consoleManager.addEventListener(WI.ConsoleManager.Event.Cleared, this._logCleared, this);

        WI.Frame.addEventListener(WI.Frame.Event.ProvisionalLoadStarted, this._provisionalLoadStarted, this);
    }

    // Public

    get navigationItems()
    {
        let navigationItems = [this._scopeBar, new WI.DividerNavigationItem];

        if (this._hasNonDefaultLogChannelMessage && this._messageSourceBar)
            navigationItems.push(this._messageSourceBar, new WI.DividerNavigationItem);

        if (window.HeapAgent && HeapAgent.gc)
            navigationItems.push(this._garbageCollectNavigationItem);

        navigationItems.push(this._clearLogNavigationItem);

        if (WI.isShowingSplitConsole())
            navigationItems.push(new WI.DividerNavigationItem, this._showConsoleTabNavigationItem);
        else if (WI.isShowingConsoleTab())
            navigationItems.unshift(this._findBanner, this._checkboxesNavigationItemGroup);

        return navigationItems;
    }

    get scopeBar()
    {
        return this._scopeBar;
    }

    get logViewController()
    {
        return this._logViewController;
    }

    get scrollableElements()
    {
        return [this.element];
    }

    get shouldKeepElementsScrolledToBottom()
    {
        return true;
    }

    shown()
    {
        super.shown();

        this._logViewController.renderPendingMessages();
    }

    closed()
    {
        // While it may be possible to get here, this is a singleton ContentView instance
        // that is often re-inserted back into different ContentBrowsers, so we shouldn't
        // remove the event listeners. The singleton will never go away anyways.
        console.assert(this === WI.consoleContentView);

        super.closed();
    }

    didAppendConsoleMessageView(messageView)
    {
        console.assert(messageView instanceof WI.ConsoleMessageView || messageView instanceof WI.ConsoleCommandView);

        // Nest the message.
        var type = messageView instanceof WI.ConsoleCommandView ? null : messageView.message.type;
        if (this._nestingLevel && type !== WI.ConsoleMessage.MessageType.EndGroup) {
            var x = 16 * this._nestingLevel;
            var messageElement = messageView.element;
            messageElement.style.left = x + "px";
            messageElement.style.width = "calc(100% - " + x + "px)";
        }

        // Update the nesting level.
        switch (type) {
        case WI.ConsoleMessage.MessageType.StartGroup:
        case WI.ConsoleMessage.MessageType.StartGroupCollapsed:
            ++this._nestingLevel;
            break;
        case WI.ConsoleMessage.MessageType.EndGroup:
            if (this._nestingLevel > 0)
                --this._nestingLevel;
            break;
        }

        this._clearFocusableChildren();

        // Some results don't populate until further backend dispatches occur (like the DOM tree).
        // We want to remove focusable children after those pending dispatches too.
        let target = messageView.message ? messageView.message.target : WI.runtimeManager.activeExecutionContext.target;
        target.connection.runAfterPendingDispatches(this._clearFocusableChildren.bind(this));

        if (type && type !== WI.ConsoleMessage.MessageType.EndGroup) {
            console.assert(messageView.message instanceof WI.ConsoleMessage);
            if (!(messageView.message instanceof WI.ConsoleCommandResultMessage))
                this._markScopeBarItemUnread(messageView.message.level);

            console.assert(messageView.element instanceof Element);
            this._filterMessageElements([messageView.element]);
        }
    }

    get supportsSearch() { return true; }
    get numberOfSearchResults() { return this.hasPerformedSearch ? this._searchMatches.length : null; }
    get hasPerformedSearch() { return this._currentSearchQuery !== ""; }

    get supportsCustomFindBanner()
    {
        return WI.isShowingConsoleTab();
    }

    showCustomFindBanner()
    {
        if (!this.visible)
            return;

        this._findBanner.focus();
    }

    get supportsSave()
    {
        if (!this.visible)
            return false;

        if (WI.isShowingSplitConsole())
            return false;

        return true;
    }

    get saveData()
    {
        return {url: "web-inspector:///Console.txt", content: this._formatMessagesAsData(false), forceSaveAs: true};
    }

    handleCopyEvent(event)
    {
        if (!this._selectedMessages.length)
            return;

        event.clipboardData.setData("text/plain", this._formatMessagesAsData(true));
        event.stopPropagation();
        event.preventDefault();
    }

    handleClearShortcut(event)
    {
        this._logViewController.requestClearMessages();
    }

    handlePopulateFindShortcut()
    {
        let searchQuery = this.searchQueryWithSelection();
        if (!searchQuery)
            return;

        this._findBanner.searchQuery = searchQuery;

        this.performSearch(this._findBanner.searchQuery);
    }

    handleFindNextShortcut()
    {
        this.findBannerRevealNextResult(this._findBanner);
    }

    handleFindPreviousShortcut()
    {
        this.findBannerRevealPreviousResult(this._findBanner);
    }

    findBannerRevealPreviousResult()
    {
        this.highlightPreviousSearchMatch();
    }

    highlightPreviousSearchMatch()
    {
        if (!this.hasPerformedSearch || isEmptyObject(this._searchMatches))
            return;

        var index = this._selectedSearchMatch ? this._searchMatches.indexOf(this._selectedSearchMatch) : this._searchMatches.length;
        this._highlightSearchMatchAtIndex(index - 1);
    }

    findBannerRevealNextResult()
    {
        this.highlightNextSearchMatch();
    }

    highlightNextSearchMatch()
    {
        if (!this.hasPerformedSearch || isEmptyObject(this._searchMatches))
            return;

        var index = this._selectedSearchMatch ? this._searchMatches.indexOf(this._selectedSearchMatch) + 1 : 0;
        this._highlightSearchMatchAtIndex(index);
    }

    findBannerWantsToClearAndBlur(findBanner)
    {
        if (this._selectedMessages.length)
            this.messagesElement.focus();
        else
            this.prompt.focus();
    }

    // Protected

    layout()
    {
        this._scrollElementHeight = this.messagesElement.getBoundingClientRect().height;
    }

    // Private

    _formatMessagesAsData(onlySelected)
    {
        var messages = this._allMessageElements();

        if (onlySelected) {
            messages = messages.filter(function(message) {
                return message.classList.contains(WI.LogContentView.SelectedStyleClassName);
            });
        }

        var data = "";

        var isPrefixOptional = messages.length <= 1 && onlySelected;
        messages.forEach(function(messageElement, index) {
            var messageView = messageElement.__messageView || messageElement.__commandView;
            if (!messageView)
                return;

            if (index > 0)
                data += "\n";
            data += messageView.toClipboardString(isPrefixOptional);
        });

        return data;
    }

    _sessionStarted(event)
    {
        if (WI.settings.clearLogOnNavigate.value) {
            this._reappendProvisionalMessages();
            return;
        }

        const isFirstSession = false;
        const newSessionReason = event.data.wasReloaded ? WI.ConsoleSession.NewSessionReason.PageReloaded : WI.ConsoleSession.NewSessionReason.PageNavigated;
        this._logViewController.startNewSession(isFirstSession, {newSessionReason, timestamp: event.data.timestamp});

        this._clearProvisionalState();
    }

    _scopeFromMessageSource(source)
    {
        switch (source) {
        case WI.ConsoleMessage.MessageSource.Media:
            return WI.LogContentView.Scopes.Media;
        case WI.ConsoleMessage.MessageSource.WebRTC:
            return WI.LogContentView.Scopes.WebRTC;
        case WI.ConsoleMessage.MessageSource.MediaSource:
            return WI.LogContentView.Scopes.MediaSource;
        }

        return undefined;
    }

    _scopeFromMessageLevel(level)
    {
        switch (level) {
        case WI.ConsoleMessage.MessageLevel.Warning:
            return WI.LogContentView.Scopes.Warnings;
        case WI.ConsoleMessage.MessageLevel.Error:
            return WI.LogContentView.Scopes.Errors;
        case WI.ConsoleMessage.MessageLevel.Log:
            return WI.LogContentView.Scopes.Logs;
        case WI.ConsoleMessage.MessageLevel.Info:
            return this._hasNonDefaultLogChannelMessage ? WI.LogContentView.Scopes.Infos : WI.LogContentView.Scopes.Logs;
        case WI.ConsoleMessage.MessageLevel.Debug:
            return this._hasNonDefaultLogChannelMessage ? WI.LogContentView.Scopes.Debugs : WI.LogContentView.Scopes.Logs;
        }
        console.assert(false, "This should not be reached.");

        return undefined;
    }

    _markScopeBarItemUnread(level)
    {
        let messageLevel = this._scopeFromMessageLevel(level);
        if (!messageLevel)
            return;

        let item = this._scopeBar.item(messageLevel);
        if (item && !item.selected && !this._scopeBar.item(WI.LogContentView.Scopes.All).selected)
            item.element.classList.add("unread");
    }

    _messageAdded(event)
    {
        let message = event.data.message;
        if (this._startedProvisionalLoad)
            this._provisionalMessages.push(message);

        if (!this._hasNonDefaultLogChannelMessage && WI.consoleManager.logChannelSources.includes(message.source)) {
            this._hasNonDefaultLogChannelMessage = true;
            this.dispatchEventToListeners(WI.ContentView.Event.NavigationItemsDidChange);
            this._scopeBar.item(WI.LogContentView.Scopes.Infos).hidden = false;
            this._scopeBar.item(WI.LogContentView.Scopes.Debugs).hidden = false;
        }

        this._logViewController.appendConsoleMessage(message);
    }

    _previousMessageRepeatCountUpdated(event)
    {
        if (this._logViewController.updatePreviousMessageRepeatCount(event.data.count) && this._lastMessageView)
            this._markScopeBarItemUnread(this._lastMessageView.message.level);
    }

    _handleContextMenuEvent(event)
    {
        if (!window.getSelection().isCollapsed) {
            // If there is a selection, we want to show our normal context menu
            // (with Copy, etc.), and not Clear Log.
            return;
        }

        // In the case that there are selected messages, only clear that selection if the right-click
        // is not on the element or descendants of the selected messages.
        if (this._selectedMessages.length && !this._selectedMessages.some(element => element.contains(event.target))) {
            this._clearMessagesSelection();
            this._mousedown(event);
        }

        // If there are no selected messages, right-clicking will not reset the current mouse state
        // meaning that when the context menu is dismissed, console messages will be selected when
        // the user moves the mouse even though no buttons are pressed.
        if (!this._selectedMessages.length)
            this._mouseup(event);

        // We don't want to show the custom menu for links in the console.
        if (event.target.enclosingNodeOrSelfWithNodeName("a"))
            return;

        let contextMenu = WI.ContextMenu.createFromEvent(event);

        if (this._selectedMessages.length) {
            contextMenu.appendItem(WI.UIString("Copy Selected"), () => {
                InspectorFrontendHost.copyText(this._formatMessagesAsData(true));
            });

            contextMenu.appendItem(WI.UIString("Save Selected"), () => {
                const forceSaveAs = true;
                WI.FileUtilities.save({
                    url: "web-inspector:///Console.txt",
                    content: this._formatMessagesAsData(true),
                }, forceSaveAs);
            });

            contextMenu.appendSeparator();
        }

        contextMenu.appendItem(WI.UIString("Clear Log"), this._clearLog.bind(this));
        contextMenu.appendSeparator();
    }

    _mousedown(event)
    {
        if (this._selectedMessages.length && (event.button !== 0 || event.ctrlKey))
            return;

        if (event.defaultPrevented) {
            // Default was prevented on the event, so this means something deeper (like a disclosure triangle)
            // handled the mouse down. In this case we want to clear the selection and don't make a new selection.
            this._clearMessagesSelection();
            return;
        }

        this._mouseDownWrapper = event.target.enclosingNodeOrSelfWithClass(WI.LogContentView.ItemWrapperStyleClassName);
        this._mouseDownShiftKey = event.shiftKey;
        this._mouseDownCommandKey = event.metaKey;
        this._mouseMoveIsRowSelection = false;

        window.addEventListener("mousemove", this);
        window.addEventListener("mouseup", this);
    }

    _targetInMessageCanBeSelected(target, message)
    {
        if (target.enclosingNodeOrSelfWithNodeName("a"))
            return false;
        return true;
    }

    _mousemove(event)
    {
        var selection = window.getSelection();
        var wrapper = event.target.enclosingNodeOrSelfWithClass(WI.LogContentView.ItemWrapperStyleClassName);

        if (!wrapper) {
            // No wrapper under the mouse, so look at the selection to try and find one.
            if (!selection.isCollapsed) {
                wrapper = selection.focusNode.parentNode.enclosingNodeOrSelfWithClass(WI.LogContentView.ItemWrapperStyleClassName);
                selection.removeAllRanges();
            }

            if (!wrapper)
                return;
        }

        if (!selection.isCollapsed)
            this._clearMessagesSelection();

        if (wrapper === this._mouseDownWrapper && !this._mouseMoveIsRowSelection)
            return;

        selection.removeAllRanges();

        if (!this._mouseMoveIsRowSelection)
            this._updateMessagesSelection(this._mouseDownWrapper, this._mouseDownCommandKey, this._mouseDownShiftKey, false);

        this._updateMessagesSelection(wrapper, false, true, false);

        this._mouseMoveIsRowSelection = true;

        event.preventDefault();
        event.stopPropagation();
    }

    _mouseup(event)
    {
        window.removeEventListener("mousemove", this);
        window.removeEventListener("mouseup", this);

        var selection = window.getSelection();
        var wrapper = event.target.enclosingNodeOrSelfWithClass(WI.LogContentView.ItemWrapperStyleClassName);

        if (wrapper && (selection.isCollapsed || event.shiftKey)) {
            selection.removeAllRanges();

            if (this._targetInMessageCanBeSelected(event.target, wrapper)) {
                var sameWrapper = wrapper === this._mouseDownWrapper;
                this._updateMessagesSelection(wrapper, sameWrapper ? this._mouseDownCommandKey : false, sameWrapper ? this._mouseDownShiftKey : true, false);
            }
        } else if (!selection.isCollapsed) {
            // There is a text selection, clear the row selection.
            this._clearMessagesSelection();
        } else if (!this._mouseDownWrapper) {
            // The mouse didn't hit a console item, so clear the row selection.
            this._clearMessagesSelection();

            // Focus the prompt. Focusing the prompt needs to happen after the click to work.
            setTimeout(() => { this.prompt.focus(); }, 0);
        }

        delete this._mouseMoveIsRowSelection;
        delete this._mouseDownWrapper;
        delete this._mouseDownShiftKey;
        delete this._mouseDownCommandKey;
    }

    _ondragstart(event)
    {
        if (event.target.enclosingNodeOrSelfWithClass(WI.DOMTreeOutline.StyleClassName)) {
            event.stopPropagation();
            event.preventDefault();
        }
    }

    handleEvent(event)
    {
        switch (event.type) {
        case "mousemove":
            this._mousemove(event);
            break;
        case "mouseup":
            this._mouseup(event);
            break;
        }
    }

    _updateMessagesSelection(message, multipleSelection, rangeSelection, shouldScrollIntoView)
    {
        console.assert(message);
        if (!message)
            return;

        var alreadySelectedMessage = this._selectedMessages.includes(message);
        if (alreadySelectedMessage && this._selectedMessages.length && multipleSelection) {
            message.classList.remove(WI.LogContentView.SelectedStyleClassName);
            this._selectedMessages.remove(message);
            return;
        }

        if (!multipleSelection && !rangeSelection)
            this._clearMessagesSelection();

        if (rangeSelection) {
            var messages = this._visibleMessageElements();

            var refIndex = this._referenceMessageForRangeSelection ? messages.indexOf(this._referenceMessageForRangeSelection) : 0;
            var targetIndex = messages.indexOf(message);

            var newRange = [Math.min(refIndex, targetIndex), Math.max(refIndex, targetIndex)];

            if (this._selectionRange && this._selectionRange[0] === newRange[0] && this._selectionRange[1] === newRange[1])
                return;

            var startIndex = this._selectionRange ? Math.min(this._selectionRange[0], newRange[0]) : newRange[0];
            var endIndex = this._selectionRange ? Math.max(this._selectionRange[1], newRange[1]) : newRange[1];

            for (var i = startIndex; i <= endIndex; ++i) {
                var messageInRange = messages[i];
                if (i >= newRange[0] && i <= newRange[1] && !messageInRange.classList.contains(WI.LogContentView.SelectedStyleClassName)) {
                    messageInRange.classList.add(WI.LogContentView.SelectedStyleClassName);
                    this._selectedMessages.push(messageInRange);
                } else if (i < newRange[0] || i > newRange[1] && messageInRange.classList.contains(WI.LogContentView.SelectedStyleClassName)) {
                    messageInRange.classList.remove(WI.LogContentView.SelectedStyleClassName);
                    this._selectedMessages.remove(messageInRange);
                }
            }

            this._selectionRange = newRange;
        } else {
            message.classList.add(WI.LogContentView.SelectedStyleClassName);
            this._selectedMessages.push(message);
        }

        if (!rangeSelection)
            this._referenceMessageForRangeSelection = message;

        if (shouldScrollIntoView && !alreadySelectedMessage)
            this._ensureMessageIsVisible(this._selectedMessages.lastValue);
    }

    _ensureMessageIsVisible(message)
    {
        if (!message)
            return;

        var y = this._positionForMessage(message).y;
        if (y < 0) {
            this.element.scrollTop += y;
            return;
        }

        var nextMessage = this._nextMessage(message);
        if (nextMessage) {
            y = this._positionForMessage(nextMessage).y;
            if (y > this._scrollElementHeight)
                this.element.scrollTop += y - this._scrollElementHeight;
        } else {
            y += message.getBoundingClientRect().height;
            if (y > this._scrollElementHeight)
                this.element.scrollTop += y - this._scrollElementHeight;
        }
    }

    _positionForMessage(message)
    {
        var pagePoint = window.webkitConvertPointFromNodeToPage(message, new WebKitPoint(0, 0));
        return window.webkitConvertPointFromPageToNode(this.element, pagePoint);
    }

    _isMessageVisible(message)
    {
        var node = message;

        if (node.classList.contains(WI.LogContentView.FilteredOutStyleClassName))
            return false;

        if (this.hasPerformedSearch && node.classList.contains(WI.LogContentView.FilteredOutBySearchStyleClassName))
            return false;

        if (message.classList.contains("console-group-title"))
            node = node.parentNode.parentNode;

        while (node && node !== this.messagesElement) {
            if (node.classList.contains("collapsed"))
                return false;
            node = node.parentNode;
        }

        return true;
    }

    _isMessageSelected(message)
    {
        return message.classList.contains(WI.LogContentView.SelectedStyleClassName);
    }

    _clearMessagesSelection()
    {
        this._selectedMessages.forEach(function(message) {
            message.classList.remove(WI.LogContentView.SelectedStyleClassName);
        });
        this._selectedMessages = [];
        delete this._referenceMessageForRangeSelection;
    }

    _selectAllMessages()
    {
        this._clearMessagesSelection();

        var messages = this._visibleMessageElements();
        for (var i = 0; i < messages.length; ++i) {
            var message = messages[i];
            message.classList.add(WI.LogContentView.SelectedStyleClassName);
            this._selectedMessages.push(message);
        }
    }

    _allMessageElements()
    {
        return Array.from(this.messagesElement.querySelectorAll(".console-message, .console-user-command"));
    }

    _unfilteredMessageElements()
    {
        return this._allMessageElements().filter(function(message) {
            return !message.classList.contains(WI.LogContentView.FilteredOutStyleClassName);
        });
    }

    _visibleMessageElements()
    {
        var unfilteredMessages = this._unfilteredMessageElements();

        if (!this.hasPerformedSearch)
            return unfilteredMessages;

        return unfilteredMessages.filter(function(message) {
            return !message.classList.contains(WI.LogContentView.FilteredOutBySearchStyleClassName);
        });
    }

    _logCleared(event)
    {
        for (let item of this._scopeBar.items)
            item.element.classList.remove("unread");

        this._logViewController.clear();
        this._nestingLevel = 0;

        if (this._currentSearchQuery)
            this.performSearch(this._currentSearchQuery);
    }

    _showConsoleTab()
    {
        WI.showConsoleTab();
    }

    _clearLog()
    {
        WI.consoleManager.requestClearMessages();
    }

    _garbageCollect()
    {
        // COMPATIBILITY (iOS 10.3): Worker targets did not support HeapAgent.
        for (let target of WI.targets) {
            if (target.HeapAgent)
                target.HeapAgent.gc();
        }
    }

    _messageShouldBeVisible(message)
    {
        let messageSource = this._messageSourceBar && this._scopeFromMessageSource(message.source);
        if (messageSource && !this._messageSourceBar.item(messageSource).selected && !this._messageSourceBar.item(WI.LogContentView.Scopes.AllChannels).selected)
            return false;

        let messageLevel = this._scopeFromMessageLevel(message.level);
        if (messageLevel)
            return this._scopeBar.item(messageLevel).selected || this._scopeBar.item(WI.LogContentView.Scopes.All).selected;

        return true;
    }

    _messageSourceBarSelectionDidChange(event)
    {
        let selectedItem = this._messageSourceBar.selectedItems[0];
        if (selectedItem.id === WI.LogContentView.Scopes.AllChannels) {
            for (let item of this._messageSourceBar.items)
                item.element.classList.remove("unread");
        } else
            selectedItem.element.classList.remove("unread");

        this._filterMessageElements(this._allMessageElements());
    }

    _scopeBarSelectionDidChange(event)
    {
        let selectedItem = this._scopeBar.selectedItems[0];

        if (selectedItem.id === WI.LogContentView.Scopes.All) {
            for (let item of this._scopeBar.items)
                item.element.classList.remove("unread");
        } else
            selectedItem.element.classList.remove("unread");

        this._filterMessageElements(this._allMessageElements());
    }

    _filterMessageElements(messageElements)
    {
        messageElements.forEach(function(messageElement) {
            let visible = messageElement.__commandView instanceof WI.ConsoleCommandView || messageElement.__message instanceof WI.ConsoleCommandResultMessage;
            if (!visible)
                visible = this._messageShouldBeVisible(messageElement.__message);

            let classList = messageElement.classList;
            if (visible)
                classList.remove(WI.LogContentView.FilteredOutStyleClassName);
            else {
                this._selectedMessages.remove(messageElement);
                classList.remove(WI.LogContentView.SelectedStyleClassName);
                classList.add(WI.LogContentView.FilteredOutStyleClassName);
            }
        }, this);

        this.performSearch(this._currentSearchQuery);
    }

    _handleClearLogOnNavigateSettingChanged()
    {
        this._preserveLogNavigationItem.checked = !WI.settings.clearLogOnNavigate.value;
    }

    _handleEmulateInUserGestureSettingChanged()
    {
        this._emulateInUserGestureNavigationItem.checked = WI.settings.emulateInUserGesture.value;
    }

    _keyDown(event)
    {
        let isRTL = WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL;

        if (this._keyboardShortcutCommandA.matchesEvent(event))
            this._commandAWasPressed(event);
        else if (this._keyboardShortcutEsc.matchesEvent(event))
            this._escapeWasPressed(event);
        else if (event.keyIdentifier === "Up")
            this._upArrowWasPressed(event);
        else if (event.keyIdentifier === "Down")
            this._downArrowWasPressed(event);
        else if ((!isRTL && event.keyIdentifier === "Left") || (isRTL && event.keyIdentifier === "Right"))
            this._leftArrowWasPressed(event);
        else if ((!isRTL && event.keyIdentifier === "Right") || (isRTL && event.keyIdentifier === "Left"))
            this._rightArrowWasPressed(event);
        else if (event.keyIdentifier === "Enter" && event.metaKey)
            this._commandEnterWasPressed(event);
    }

    _keyPress(event)
    {
        const isCommandC = event.metaKey && event.keyCode === 99;
        if (!isCommandC)
            this.prompt.focus();
    }

    _commandAWasPressed(event)
    {
        this._selectAllMessages();
        event.preventDefault();
    }

    _escapeWasPressed(event)
    {
        if (this._selectedMessages.length)
            this._clearMessagesSelection();
        else
            this.prompt.focus();

        event.preventDefault();
    }

    _upArrowWasPressed(event)
    {
        var messages = this._visibleMessageElements();

        if (!this._selectedMessages.length) {
            if (messages.length)
                this._updateMessagesSelection(messages.lastValue, false, false, true);
            return;
        }

        var lastMessage = this._selectedMessages.lastValue;
        var previousMessage = this._previousMessage(lastMessage);
        if (previousMessage)
            this._updateMessagesSelection(previousMessage, false, event.shiftKey, true);
        else if (!event.shiftKey) {
            this._clearMessagesSelection();
            this._updateMessagesSelection(messages[0], false, false, true);
        }

        event.preventDefault();
    }

    _downArrowWasPressed(event)
    {
        var messages = this._visibleMessageElements();

        if (!this._selectedMessages.length) {
            if (messages.length)
                this._updateMessagesSelection(messages[0], false, false, true);
            return;
        }

        var lastMessage = this._selectedMessages.lastValue;
        var nextMessage = this._nextMessage(lastMessage);
        if (nextMessage)
            this._updateMessagesSelection(nextMessage, false, event.shiftKey, true);
        else if (!event.shiftKey) {
            this._clearMessagesSelection();
            this._updateMessagesSelection(messages.lastValue, false, false, true);
        }

        event.preventDefault();
    }

    _leftArrowWasPressed(event)
    {
        if (this._selectedMessages.length !== 1)
            return;

        var currentMessage = this._selectedMessages[0];
        if (currentMessage.classList.contains("console-group-title")) {
            currentMessage.parentNode.classList.add("collapsed");
            event.preventDefault();
        } else if (currentMessage.__messageView && currentMessage.__messageView.expandable) {
            currentMessage.__messageView.collapse();
            event.preventDefault();
        }
    }

    _rightArrowWasPressed(event)
    {
        if (this._selectedMessages.length !== 1)
            return;

        var currentMessage = this._selectedMessages[0];
        if (currentMessage.classList.contains("console-group-title")) {
            currentMessage.parentNode.classList.remove("collapsed");
            event.preventDefault();
        } else if (currentMessage.__messageView && currentMessage.__messageView.expandable) {
            currentMessage.__messageView.expand();
            event.preventDefault();
        }
    }

    _commandEnterWasPressed(event)
    {
        if (this._selectedMessages.length !== 1)
            return;

        let message = this._selectedMessages[0];
        if (message.__commandView && message.__commandView.commandText) {
            this._logViewController.consolePromptTextCommitted(null, message.__commandView.commandText);
            event.preventDefault();
        }
    }

    _previousMessage(message)
    {
        var messages = this._visibleMessageElements();
        for (var i = messages.indexOf(message) - 1; i >= 0; --i) {
            if (this._isMessageVisible(messages[i]))
                return messages[i];
        }
        return null;
    }

    _nextMessage(message)
    {
        var messages = this._visibleMessageElements();
        for (var i = messages.indexOf(message) + 1; i < messages.length; ++i) {
            if (this._isMessageVisible(messages[i]))
                return messages[i];
        }
        return null;
    }

    _clearFocusableChildren()
    {
        var focusableElements = this.messagesElement.querySelectorAll("[tabindex]");
        for (var i = 0, count = focusableElements.length; i < count; ++i)
            focusableElements[i].removeAttribute("tabindex");
    }

    findBannerPerformSearch(findBanner, searchQuery)
    {
        this.performSearch(searchQuery);
    }

    findBannerSearchCleared()
    {
        this.searchCleared();
    }

    revealNextSearchResult()
    {
        this.findBannerRevealNextResult();
    }

    revealPreviousSearchResult()
    {
        this.findBannerRevealPreviousResult();
    }

    performSearch(searchQuery)
    {
        if (!isEmptyObject(this._searchHighlightDOMChanges))
            WI.revertDOMChanges(this._searchHighlightDOMChanges);

        this._currentSearchQuery = searchQuery;
        this._searchHighlightDOMChanges = [];
        this._searchMatches = [];
        this._selectedSearchMatchIsValid = false;
        this._selectedSearchMatch = null;
        let numberOfResults = 0;

        if (this._currentSearchQuery === "") {
            this.element.classList.remove(WI.LogContentView.SearchInProgressStyleClassName);
            this.dispatchEventToListeners(WI.ContentView.Event.NumberOfSearchResultsDidChange);
            return;
        }

        this.element.classList.add(WI.LogContentView.SearchInProgressStyleClassName);

        let searchRegex = WI.SearchUtilities.regExpForString(this._currentSearchQuery, WI.SearchUtilities.defaultSettings);
        this._unfilteredMessageElements().forEach(function(message) {
            let matchRanges = [];
            let text = message.textContent;
            let match = searchRegex.exec(text);
            while (match) {
                numberOfResults++;
                matchRanges.push({offset: match.index, length: match[0].length});
                match = searchRegex.exec(text);
            }

            if (!isEmptyObject(matchRanges))
                this._highlightRanges(message, matchRanges);

            let classList = message.classList;
            if (!isEmptyObject(matchRanges) || message.__commandView instanceof WI.ConsoleCommandView || message.__message instanceof WI.ConsoleCommandResultMessage)
                classList.remove(WI.LogContentView.FilteredOutBySearchStyleClassName);
            else
                classList.add(WI.LogContentView.FilteredOutBySearchStyleClassName);
        }, this);

        this.dispatchEventToListeners(WI.ContentView.Event.NumberOfSearchResultsDidChange);

        this._findBanner.numberOfResults = numberOfResults;

        if (!this._selectedSearchMatchIsValid && this._selectedSearchMatch) {
            this._selectedSearchMatch.highlight.classList.remove(WI.LogContentView.SelectedStyleClassName);
            this._selectedSearchMatch = null;
        }
    }

    searchHidden()
    {
        this.searchCleared();
    }

    searchCleared()
    {
        this.performSearch("");
    }

    _highlightRanges(message, matchRanges)
    {
        var highlightedElements = WI.highlightRangesWithStyleClass(message, matchRanges, WI.LogContentView.HighlightedStyleClassName, this._searchHighlightDOMChanges);

        console.assert(highlightedElements.length === matchRanges.length);

        matchRanges.forEach(function(range, index) {
            this._searchMatches.push({message, range, highlight: highlightedElements[index]});

            if (this._selectedSearchMatch && !this._selectedSearchMatchIsValid && this._selectedSearchMatch.message === message) {
                this._selectedSearchMatchIsValid = this._rangesOverlap(this._selectedSearchMatch.range, range);
                if (this._selectedSearchMatchIsValid) {
                    delete this._selectedSearchMatch;
                    this._highlightSearchMatchAtIndex(this._searchMatches.length - 1);
                }
            }
        }, this);
    }

    _rangesOverlap(range1, range2)
    {
        return range1.offset <= range2.offset + range2.length && range2.offset <= range1.offset + range1.length;
    }

    _highlightSearchMatchAtIndex(index)
    {
        if (index >= this._searchMatches.length)
            index = 0;
        else if (index < 0)
            index = this._searchMatches.length - 1;

        if (this._selectedSearchMatch)
            this._selectedSearchMatch.highlight.classList.remove(WI.LogContentView.SelectedStyleClassName);

        this._selectedSearchMatch = this._searchMatches[index];
        this._selectedSearchMatch.highlight.classList.add(WI.LogContentView.SelectedStyleClassName);

        this._ensureMessageIsVisible(this._selectedSearchMatch.message);
    }

    _provisionalLoadStarted()
    {
        this._startedProvisionalLoad = true;
    }

    _reappendProvisionalMessages()
    {
        if (!this._startedProvisionalLoad)
            return;

        this._startedProvisionalLoad = false;

        for (let provisionalMessage of this._provisionalMessages)
            this._logViewController.appendConsoleMessage(provisionalMessage);

        this._provisionalMessages = [];
    }

    _clearProvisionalState()
    {
        this._startedProvisionalLoad = false;
        this._provisionalMessages = [];
    }
};

WI.LogContentView.Scopes = {
    All: "log-all",
    Errors: "log-errors",
    Warnings: "log-warnings",
    Logs: "log-logs",
    Infos: "log-infos",
    Debugs: "log-debugs",
    AllChannels: "log-all-channels",
    Media: "log-media",
    WebRTC: "log-webrtc",
    MediaSource: "log-mediasource",
};

WI.LogContentView.ItemWrapperStyleClassName = "console-item";
WI.LogContentView.FilteredOutStyleClassName = "filtered-out";
WI.LogContentView.SelectedStyleClassName = "selected";
WI.LogContentView.SearchInProgressStyleClassName = "search-in-progress";
WI.LogContentView.FilteredOutBySearchStyleClassName = "filtered-out-by-search";
WI.LogContentView.HighlightedStyleClassName = "highlighted";
