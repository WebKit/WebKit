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

WebInspector.LogContentView = class LogContentView extends WebInspector.ContentView
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

        this.prompt = WebInspector.quickConsole.prompt;

        this._keyboardShortcutCommandA = new WebInspector.KeyboardShortcut(WebInspector.KeyboardShortcut.Modifier.CommandOrControl, "A");
        this._keyboardShortcutEsc = new WebInspector.KeyboardShortcut(null, WebInspector.KeyboardShortcut.Key.Escape);

        this._logViewController = new WebInspector.JavaScriptLogViewController(this.messagesElement, this.element, this.prompt, this, "console-prompt-history");
        this._lastMessageView = null;

        this._findBanner = new WebInspector.FindBanner(this, "console-find-banner", true);
        this._findBanner.inputField.placeholder = WebInspector.UIString("Filter Console Log");

        var scopeBarItems = [
            new WebInspector.ScopeBarItem(WebInspector.LogContentView.Scopes.All, WebInspector.UIString("All"), true),
            new WebInspector.ScopeBarItem(WebInspector.LogContentView.Scopes.Errors, WebInspector.UIString("Errors"), false, "errors"),
            new WebInspector.ScopeBarItem(WebInspector.LogContentView.Scopes.Warnings, WebInspector.UIString("Warnings"), false, "warnings"),
            new WebInspector.ScopeBarItem(WebInspector.LogContentView.Scopes.Logs, WebInspector.UIString("Logs"), false, "logs")
        ];

        this._scopeBar = new WebInspector.ScopeBar("log-scope-bar", scopeBarItems, scopeBarItems[0]);
        this._scopeBar.addEventListener(WebInspector.ScopeBar.Event.SelectionChanged, this._scopeBarSelectionDidChange, this);

        this._clearLogNavigationItem = new WebInspector.ButtonNavigationItem("clear-log", WebInspector.UIString("Clear log (%s or %s)").format(this._logViewController.messagesClearKeyboardShortcut.displayName, this._logViewController.messagesAlternateClearKeyboardShortcut.displayName), "Images/NavigationItemTrash.svg", 15, 15);
        this._clearLogNavigationItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._clearLog, this);

        var toolTip = WebInspector.UIString("Show console tab");
        this._showConsoleTabNavigationItem = new WebInspector.ButtonNavigationItem("show-tab", toolTip, "Images/SplitToggleUp.svg", 16, 16);
        this._showConsoleTabNavigationItem.addEventListener(WebInspector.ButtonNavigationItem.Event.Clicked, this._showConsoleTab, this);

        this.messagesElement.addEventListener("contextmenu", this._handleContextMenuEvent.bind(this), false);

        WebInspector.logManager.addEventListener(WebInspector.LogManager.Event.SessionStarted, this._sessionStarted, this);
        WebInspector.logManager.addEventListener(WebInspector.LogManager.Event.MessageAdded, this._messageAdded, this);
        WebInspector.logManager.addEventListener(WebInspector.LogManager.Event.PreviousMessageRepeatCountUpdated, this._previousMessageRepeatCountUpdated, this);
        WebInspector.logManager.addEventListener(WebInspector.LogManager.Event.Cleared, this._logCleared, this);

        WebInspector.Frame.addEventListener(WebInspector.Frame.Event.ProvisionalLoadStarted, this._provisionalLoadStarted, this);
    }

    // Public

    get navigationItems()
    {
        let navigationItems = [this._findBanner, this._scopeBar, this._clearLogNavigationItem];
        if (WebInspector.isShowingSplitConsole())
            navigationItems.push(this._showConsoleTabNavigationItem);
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

    get searchInProgress()
    {
        return this.messagesElement.classList.contains(WebInspector.LogContentView.SearchInProgressStyleClassName);
    }

    shown()
    {
        super.shown();

        this._logViewController.renderPendingMessages();
    }

    didAppendConsoleMessageView(messageView)
    {
        console.assert(messageView instanceof WebInspector.ConsoleMessageView || messageView instanceof WebInspector.ConsoleCommandView);

        // Nest the message.
        var type = messageView instanceof WebInspector.ConsoleCommandView ? null : messageView.message.type;
        if (this._nestingLevel && type !== WebInspector.ConsoleMessage.MessageType.EndGroup) {
            var x = 16 * this._nestingLevel;
            var messageElement = messageView.element;
            messageElement.style.left = x + "px";
            messageElement.style.width = "calc(100% - " + x + "px)";
        }

        // Update the nesting level.
        switch (type) {
        case WebInspector.ConsoleMessage.MessageType.StartGroup:
        case WebInspector.ConsoleMessage.MessageType.StartGroupCollapsed:
            ++this._nestingLevel;
            break;
        case WebInspector.ConsoleMessage.MessageType.EndGroup:
            if (this._nestingLevel > 0)
                --this._nestingLevel;
            break;
        }

        this._clearFocusableChildren();

        // Some results don't populate until further backend dispatches occur (like the DOM tree).
        // We want to remove focusable children after those pending dispatches too.
        InspectorBackend.runAfterPendingDispatches(this._clearFocusableChildren.bind(this));

        if (type && type !== WebInspector.ConsoleMessage.MessageType.EndGroup) {
            console.assert(messageView.message instanceof WebInspector.ConsoleMessage);
            if (!(messageView.message instanceof WebInspector.ConsoleCommandResultMessage))
                this._markScopeBarItemUnread(messageView.message.level);

            console.assert(messageView.element instanceof Element);
            this._filterMessageElements([messageView.element]);
        }
    }

    get supportsSearch()
    {
        return true;
    }

    handleFindEvent(event)
    {
        if (!this.visible)
            return;

        this._findBanner.focus();
    }

    get supportsSave()
    {
        if (!this.visible)
            return false;

        if (WebInspector.isShowingSplitConsole())
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

    findBannerRevealPreviousResult()
    {
        this.highlightPreviousSearchMatch();
    }

    highlightPreviousSearchMatch()
    {
        if (!this.searchInProgress || isEmptyObject(this._searchMatches))
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
        if (!this.searchInProgress || isEmptyObject(this._searchMatches))
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
                return message.classList.contains(WebInspector.LogContentView.SelectedStyleClassName);
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
        if (WebInspector.logManager.clearLogOnNavigateSetting.value) {
            this._reappendProvisionalMessages();
            return;
        }

        this._logViewController.startNewSession();

        this._clearProvisionalState();
    }

    _scopeFromMessageLevel(level)
    {
        var messageLevel;

        switch(level) {
        case WebInspector.ConsoleMessage.MessageLevel.Warning:
            messageLevel = WebInspector.LogContentView.Scopes.Warnings;
            break;
        case WebInspector.ConsoleMessage.MessageLevel.Error:
            messageLevel = WebInspector.LogContentView.Scopes.Errors;
            break;
        case WebInspector.ConsoleMessage.MessageLevel.Log:
        case WebInspector.ConsoleMessage.MessageLevel.Info:
        case WebInspector.ConsoleMessage.MessageLevel.Debug:
            messageLevel = WebInspector.LogContentView.Scopes.Logs;
            break;
        }

        return messageLevel;
    }

    _markScopeBarItemUnread(level)
    {
        let messageLevel = this._scopeFromMessageLevel(level);
        if (!messageLevel)
            return;

        let item = this._scopeBar.item(messageLevel);
        if (item && !item.selected && !this._scopeBar.item(WebInspector.LogContentView.Scopes.All).selected)
            item.element.classList.add("unread");
    }

    _messageAdded(event)
    {
        if (this._startedProvisionalLoad)
            this._provisionalMessages.push(event.data.message);

        this._logViewController.appendConsoleMessage(event.data.message);
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
        if (this._selectedMessages.length && !this._selectedMessages.some(element => event.target.isSelfOrDescendant(element))) {
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

        let contextMenu = WebInspector.ContextMenu.createFromEvent(event);

        if (this._selectedMessages.length) {
            contextMenu.appendItem(WebInspector.UIString("Copy Selected"), () => {
                InspectorFrontendHost.copyText(this._formatMessagesAsData(true));
            });
            contextMenu.appendSeparator();
        }

        contextMenu.appendItem(WebInspector.UIString("Clear Log"), this._clearLog.bind(this));
        contextMenu.appendSeparator();

        let clearLogOnReloadUIString = WebInspector.logManager.clearLogOnNavigateSetting.value ? WebInspector.UIString("Keep Log on Navigation") : WebInspector.UIString("Clear Log on Navigation");
        contextMenu.appendItem(clearLogOnReloadUIString, this._toggleClearLogOnNavigateSetting.bind(this));
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

        this._mouseDownWrapper = event.target.enclosingNodeOrSelfWithClass(WebInspector.LogContentView.ItemWrapperStyleClassName);
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
        var wrapper = event.target.enclosingNodeOrSelfWithClass(WebInspector.LogContentView.ItemWrapperStyleClassName);

        if (!wrapper) {
            // No wrapper under the mouse, so look at the selection to try and find one.
            if (!selection.isCollapsed) {
                wrapper = selection.focusNode.parentNode.enclosingNodeOrSelfWithClass(WebInspector.LogContentView.ItemWrapperStyleClassName);
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
        var wrapper = event.target.enclosingNodeOrSelfWithClass(WebInspector.LogContentView.ItemWrapperStyleClassName);

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
        if (event.target.enclosingNodeOrSelfWithClass(WebInspector.DOMTreeOutline.StyleClassName)) {
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
            message.classList.remove(WebInspector.LogContentView.SelectedStyleClassName);
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
                if (i >= newRange[0] && i <= newRange[1] && !messageInRange.classList.contains(WebInspector.LogContentView.SelectedStyleClassName)) {
                    messageInRange.classList.add(WebInspector.LogContentView.SelectedStyleClassName);
                    this._selectedMessages.push(messageInRange);
                } else if (i < newRange[0] || i > newRange[1] && messageInRange.classList.contains(WebInspector.LogContentView.SelectedStyleClassName)) {
                    messageInRange.classList.remove(WebInspector.LogContentView.SelectedStyleClassName);
                    this._selectedMessages.remove(messageInRange);
                }
            }

            this._selectionRange = newRange;
        } else {
            message.classList.add(WebInspector.LogContentView.SelectedStyleClassName);
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

        if (node.classList.contains(WebInspector.LogContentView.FilteredOutStyleClassName))
            return false;

        if (this.searchInProgress && node.classList.contains(WebInspector.LogContentView.FilteredOutBySearchStyleClassName))
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
        return message.classList.contains(WebInspector.LogContentView.SelectedStyleClassName);
    }

    _clearMessagesSelection()
    {
        this._selectedMessages.forEach(function(message) {
            message.classList.remove(WebInspector.LogContentView.SelectedStyleClassName);
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
            message.classList.add(WebInspector.LogContentView.SelectedStyleClassName);
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
            return !message.classList.contains(WebInspector.LogContentView.FilteredOutStyleClassName);
        });
    }

    _visibleMessageElements()
    {
        var unfilteredMessages = this._unfilteredMessageElements();

        if (!this.searchInProgress)
            return unfilteredMessages;

        return unfilteredMessages.filter(function(message) {
            return !message.classList.contains(WebInspector.LogContentView.FilteredOutBySearchStyleClassName);
        });
    }

    _logCleared(event)
    {
        for (let item of this._scopeBar.items)
            item.element.classList.remove("unread");

        this._logViewController.clear();
        this._nestingLevel = 0;

        let searchQuery = this._findBanner.searchQuery;
        if (searchQuery)
            this._performSearch(searchQuery);
    }

    _showConsoleTab()
    {
        WebInspector.showConsoleTab();
    }

    _toggleClearLogOnNavigateSetting()
    {
        WebInspector.logManager.clearLogOnNavigateSetting.value = !WebInspector.logManager.clearLogOnNavigateSetting.value;
    }

    _clearLog()
    {
        WebInspector.logManager.requestClearMessages();
    }

    _scopeBarSelectionDidChange(event)
    {
        var item = this._scopeBar.selectedItems[0];
        
        if (item.id === WebInspector.LogContentView.Scopes.All) {
            for (var item of this._scopeBar.items)
                item.element.classList.remove("unread");
        } else
            item.element.classList.remove("unread");

        this._filterMessageElements(this._allMessageElements());
    }

    _filterMessageElements(messageElements)
    {
        var showsAll = this._scopeBar.item(WebInspector.LogContentView.Scopes.All).selected;

        messageElements.forEach(function(messageElement) {
            var visible = showsAll || messageElement.__commandView instanceof WebInspector.ConsoleCommandView || messageElement.__message instanceof WebInspector.ConsoleCommandResultMessage;
            if (!visible) {
                var messageLevel = this._scopeFromMessageLevel(messageElement.__message.level);

                if (messageLevel)
                    visible = this._scopeBar.item(messageLevel).selected;
            }

            var classList = messageElement.classList;
            if (visible)
                classList.remove(WebInspector.LogContentView.FilteredOutStyleClassName);
            else {
                this._selectedMessages.remove(messageElement);
                classList.remove(WebInspector.LogContentView.SelectedStyleClassName);
                classList.add(WebInspector.LogContentView.FilteredOutStyleClassName);
            }
        }, this);

        this._performSearch(this._findBanner.searchQuery);
    }

    _keyDown(event)
    {
        if (this._keyboardShortcutCommandA.matchesEvent(event))
            this._commandAWasPressed(event);
        else if (this._keyboardShortcutEsc.matchesEvent(event))
            this._escapeWasPressed(event);
        else if (event.keyIdentifier === "Up")
            this._upArrowWasPressed(event);
        else if (event.keyIdentifier === "Down")
            this._downArrowWasPressed(event);
        else if (event.keyIdentifier === "Left")
            this._leftArrowWasPressed(event);
        else if (event.keyIdentifier === "Right")
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
    }

    _nextMessage(message)
    {
        var messages = this._visibleMessageElements();
        for (var i = messages.indexOf(message) + 1; i < messages.length; ++i) {
            if (this._isMessageVisible(messages[i]))
                return messages[i];
        }
    }

    _clearFocusableChildren()
    {
        var focusableElements = this.messagesElement.querySelectorAll("[tabindex]");
        for (var i = 0, count = focusableElements.length; i < count; ++i)
            focusableElements[i].removeAttribute("tabindex");
    }

    findBannerPerformSearch(findBanner, searchTerms)
    {
        this._performSearch(searchTerms);
    }

    findBannerSearchCleared()
    {
        this._performSearch("");
    }

    revealNextSearchResult()
    {
        this.findBannerRevealNextResult();
    }

    revealPreviousSearchResult()
    {
        this.findBannerRevealPreviousResult();
    }

    _performSearch(searchTerms)
    {
        if (!isEmptyObject(this._searchHighlightDOMChanges))
            WebInspector.revertDomChanges(this._searchHighlightDOMChanges);

        if (searchTerms === "") {
            delete this._selectedSearchMatch;
            this._matchingSearchElements = [];
            this.messagesElement.classList.remove(WebInspector.LogContentView.SearchInProgressStyleClassName);
            return;
        }

        this.messagesElement.classList.add(WebInspector.LogContentView.SearchInProgressStyleClassName);

        this._searchHighlightDOMChanges = [];
        this._searchMatches = [];
        this._selectedSearchMathIsValid = false;
        let numberOfResults = 0;

        var searchRegex = new RegExp(searchTerms.escapeForRegExp(), "gi");
        this._unfilteredMessageElements().forEach(function(message) {
            var matchRanges = [];
            var text = message.textContent;
            var match = searchRegex.exec(text);
            while (match) {
                numberOfResults++;
                matchRanges.push({ offset: match.index, length: match[0].length });
                match = searchRegex.exec(text);
            }

            if (!isEmptyObject(matchRanges))
                this._highlightRanges(message, matchRanges);

            var classList = message.classList;
            if (!isEmptyObject(matchRanges) || message.__commandView instanceof WebInspector.ConsoleCommandView || message.__message instanceof WebInspector.ConsoleCommandResultMessage)
                classList.remove(WebInspector.LogContentView.FilteredOutBySearchStyleClassName);
            else
                classList.add(WebInspector.LogContentView.FilteredOutBySearchStyleClassName);
        }, this);

        if (!this._selectedSearchMathIsValid && this._selectedSearchMatch) {
            this._selectedSearchMatch.highlight.classList.remove(WebInspector.LogContentView.SelectedStyleClassName);
            delete this._selectedSearchMatch;
        }

        this._findBanner.numberOfResults = numberOfResults;
    }

    _highlightRanges(message, matchRanges)
    {
        var highlightedElements = WebInspector.highlightRangesWithStyleClass(message, matchRanges, WebInspector.LogContentView.HighlightedStyleClassName, this._searchHighlightDOMChanges);

        console.assert(highlightedElements.length === matchRanges.length);

        matchRanges.forEach(function(range, index) {
            this._searchMatches.push({message, range, highlight: highlightedElements[index]});

            if (this._selectedSearchMatch && !this._selectedSearchMathIsValid && this._selectedSearchMatch.message === message) {
                this._selectedSearchMathIsValid = this._rangesOverlap(this._selectedSearchMatch.range, range);
                if (this._selectedSearchMathIsValid) {
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
            this._selectedSearchMatch.highlight.classList.remove(WebInspector.LogContentView.SelectedStyleClassName);

        this._selectedSearchMatch = this._searchMatches[index];
        this._selectedSearchMatch.highlight.classList.add(WebInspector.LogContentView.SelectedStyleClassName);

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

WebInspector.LogContentView.Scopes = {
    All: "log-all",
    Errors: "log-errors",
    Warnings: "log-warnings",
    Logs: "log-logs"
};

WebInspector.LogContentView.ItemWrapperStyleClassName = "console-item";
WebInspector.LogContentView.FilteredOutStyleClassName = "filtered-out";
WebInspector.LogContentView.SelectedStyleClassName = "selected";
WebInspector.LogContentView.SearchInProgressStyleClassName = "search-in-progress";
WebInspector.LogContentView.FilteredOutBySearchStyleClassName = "filtered-out-by-search";
WebInspector.LogContentView.HighlightedStyleClassName = "highlighted";
