/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

WebInspector.Breakpoint = function(sourceCodeLocationOrInfo, disabled, condition)
{
    WebInspector.Object.call(this);

    if (sourceCodeLocationOrInfo instanceof WebInspector.SourceCodeLocation) {
        var sourceCode = sourceCodeLocationOrInfo.sourceCode;
        var url = sourceCode ? sourceCode.url : null;
        var scriptIdentifier = sourceCode instanceof WebInspector.Script ? sourceCode.id : null;
        var location = sourceCodeLocationOrInfo;
    } else if (sourceCodeLocationOrInfo && typeof sourceCodeLocationOrInfo === "object") {
        var url = sourceCodeLocationOrInfo.url;
        var lineNumber = sourceCodeLocationOrInfo.lineNumber || 0;
        var columnNumber = sourceCodeLocationOrInfo.columnNumber || 0;
        var location = new WebInspector.SourceCodeLocation(null, lineNumber, columnNumber);
        var autoContinue = sourceCodeLocationOrInfo.autoContinue || false;
        var actions = sourceCodeLocationOrInfo.actions || [];
        for (var i = 0; i < actions.length; ++i)
            actions[i] = new WebInspector.BreakpointAction(this, actions[i]);
        disabled = sourceCodeLocationOrInfo.disabled;
        condition = sourceCodeLocationOrInfo.condition;
    } else
        console.error("Unexpected type passed to WebInspector.Breakpoint", sourceCodeLocationOrInfo);

    this._id = null;
    this._url = url || null;
    this._scriptIdentifier = scriptIdentifier || null;
    this._disabled = disabled || false;
    this._condition = condition || "";
    this._autoContinue = autoContinue || false;
    this._actions = actions || [];
    this._resolved = false;

    this._sourceCodeLocation = location;
    this._sourceCodeLocation.addEventListener(WebInspector.SourceCodeLocation.Event.LocationChanged, this._sourceCodeLocationLocationChanged, this);
    this._sourceCodeLocation.addEventListener(WebInspector.SourceCodeLocation.Event.DisplayLocationChanged, this._sourceCodeLocationDisplayLocationChanged, this);
};

WebInspector.Object.addConstructorFunctions(WebInspector.Breakpoint);

WebInspector.Breakpoint.PopoverClassName = "edit-breakpoint-popover-content";
WebInspector.Breakpoint.WidePopoverClassName = "wide";
WebInspector.Breakpoint.PopoverConditionInputId = "edit-breakpoint-popover-condition";
WebInspector.Breakpoint.PopoverOptionsAutoContinueInputId = "edit-breakpoint-popoover-auto-continue";
WebInspector.Breakpoint.HiddenStyleClassName = "hidden";

WebInspector.Breakpoint.DefaultBreakpointActionType = WebInspector.BreakpointAction.Type.Log;

WebInspector.Breakpoint.TypeIdentifier = "breakpoint";
WebInspector.Breakpoint.URLCookieKey = "breakpoint-url";
WebInspector.Breakpoint.LineNumberCookieKey = "breakpoint-line-number";
WebInspector.Breakpoint.ColumnNumberCookieKey = "breakpoint-column-number";

WebInspector.Breakpoint.Event = {
    DisabledStateDidChange: "breakpoint-disabled-state-did-change",
    ResolvedStateDidChange: "breakpoint-resolved-state-did-change",
    ConditionDidChange: "breakpoint-condition-did-change",
    ActionsDidChange: "breakpoint-actions-did-change",
    AutoContinueDidChange: "breakpoint-auto-continue-did-change",
    LocationDidChange: "breakpoint-location-did-change",
    DisplayLocationDidChange: "breakpoint-display-location-did-change",
};

WebInspector.Breakpoint.prototype = {
    constructor: WebInspector.Breakpoint,

    // Public

    get id()
    {
        return this._id;
    },

    set id(id)
    {
        this._id = id || null;
    },

    get url()
    {
        return this._url;
    },

    get scriptIdentifier()
    {
        return this._scriptIdentifier;
    },

    get sourceCodeLocation()
    {
        return this._sourceCodeLocation;
    },

    get resolved()
    {
        return this._resolved && WebInspector.debuggerManager.breakpointsEnabled;
    },

    set resolved(resolved)
    {
        if (this._resolved === resolved)
            return;

        this._resolved = resolved || false;

        this.dispatchEventToListeners(WebInspector.Breakpoint.Event.ResolvedStateDidChange);
    },

    get disabled()
    {
        return this._disabled;
    },

    set disabled(disabled)
    {
        if (this._disabled === disabled)
            return;

        this._disabled = disabled || false;

        this.dispatchEventToListeners(WebInspector.Breakpoint.Event.DisabledStateDidChange);
    },

    get condition()
    {
        return this._condition;
    },

    set condition(condition)
    {
        if (this._condition === condition)
            return;

        this._condition = condition;

        this.dispatchEventToListeners(WebInspector.Breakpoint.Event.ConditionDidChange);
    },

    get autoContinue()
    {
        return this._autoContinue;
    },

    set autoContinue(cont)
    {
        if (this._autoContinue === cont)
            return;

        this._autoContinue = cont;

        this.dispatchEventToListeners(WebInspector.Breakpoint.Event.AutoContinueDidChange);
    },

    get actions()
    {
        return this._actions;
    },

    get options()
    {
        return {
            condition: this._condition,
            actions: this._serializableActions(),
            autoContinue: this._autoContinue
        };
    },

    get info()
    {
        // The id, scriptIdentifier and resolved state are tied to the current session, so don't include them for serialization.
        return {
            url: this._url,
            lineNumber: this._sourceCodeLocation.lineNumber,
            columnNumber: this._sourceCodeLocation.columnNumber,
            disabled: this._disabled,
            condition: this._condition,
            actions: this._serializableActions(),
            autoContinue: this._autoContinue
        };
    },

    get probeActions()
    {
        return this._actions.filter(function(action) {
            return action.type === WebInspector.BreakpointAction.Type.Probe;
        });
    },

    cycleToNextMode: function()
    {
        if (this.disabled) {
            // When cycling, clear auto-continue when going from disabled to enabled.
            this.autoContinue = false;
            this.disabled = false;
            return;
        }

        if (this.autoContinue) {
            this.disabled = true;
            return;
        }

        if (this.actions.length) {
            this.autoContinue = true;
            return;
        }

        this.disabled = true;
    },

    appendContextMenuItems: function(contextMenu, breakpointDisplayElement)
    {
        console.assert(document.body.contains(breakpointDisplayElement), "breakpoint popover display element must be in the DOM");

        var boundingClientRect = breakpointDisplayElement.getBoundingClientRect();

        function editBreakpoint()
        {
            this._showEditBreakpointPopover(boundingClientRect);
        }

        function removeBreakpoint()
        {
            WebInspector.debuggerManager.removeBreakpoint(this);
        }

        function toggleBreakpoint()
        {
            this.disabled = !this.disabled;
        }

        function toggleAutoContinue()
        {
            this.autoContinue = !this.autoContinue;
        }

        function revealOriginalSourceCodeLocation()
        {
            WebInspector.resourceSidebarPanel.showOriginalOrFormattedSourceCodeLocation(this._sourceCodeLocation);
        }

        if (WebInspector.debuggerManager.isBreakpointEditable(this))
            contextMenu.appendItem(WebInspector.UIString("Edit Breakpoint…"), editBreakpoint.bind(this));

        if (this.autoContinue && !this.disabled) {
            contextMenu.appendItem(WebInspector.UIString("Disable Breakpoint"), toggleBreakpoint.bind(this));
            contextMenu.appendItem(WebInspector.UIString("Cancel Automatic Continue"), toggleAutoContinue.bind(this));
        } else if (!this.disabled)
            contextMenu.appendItem(WebInspector.UIString("Disable Breakpoint"), toggleBreakpoint.bind(this));
        else
            contextMenu.appendItem(WebInspector.UIString("Enable Breakpoint"), toggleBreakpoint.bind(this));

        if (!this.autoContinue && !this.disabled && this.actions.length)
            contextMenu.appendItem(WebInspector.UIString("Set to Automatically Continue"), toggleAutoContinue.bind(this));

        if (WebInspector.debuggerManager.isBreakpointRemovable(this)) {
            contextMenu.appendSeparator();
            contextMenu.appendItem(WebInspector.UIString("Delete Breakpoint"), removeBreakpoint.bind(this));
        }

        if (this._sourceCodeLocation.hasMappedLocation()) {
            contextMenu.appendSeparator();
            contextMenu.appendItem(WebInspector.UIString("Reveal in Original Resource"), revealOriginalSourceCodeLocation.bind(this));
        }
    },

    createAction: function(type, precedingAction, data)
    {
        var newAction = new WebInspector.BreakpointAction(this, type, data || null);

        if (!precedingAction)
            this._actions.push(newAction);
        else {
            var index = this._actions.indexOf(precedingAction);
            console.assert(index !== -1);
            if (index === -1)
                this._actions.push(newAction);
            else
                this._actions.splice(index + 1, 0, newAction);
        }

        this.dispatchEventToListeners(WebInspector.Breakpoint.Event.ActionsDidChange);

        return newAction;
    },

    recreateAction: function(type, actionToReplace)
    {
        var newAction = new WebInspector.BreakpointAction(this, type, null);

        var index = this._actions.indexOf(actionToReplace);
        console.assert(index !== -1);
        if (index === -1)
            return null;

        this._actions[index] = newAction;

        this.dispatchEventToListeners(WebInspector.Breakpoint.Event.ActionsDidChange);

        return newAction;
    },

    removeAction: function(action)
    {
        var index = this._actions.indexOf(action);
        console.assert(index !== -1);
        if (index === -1)
            return;

        this._actions.splice(index, 1);

        if (!this._actions.length)
            this.autoContinue = false;

        this.dispatchEventToListeners(WebInspector.Breakpoint.Event.ActionsDidChange);
    },

    clearActions: function(type)
    {
        if (!type)
            this._actions = [];
        else
            this._actions = this._actions.filter(function(action) { action.type != type; });

        this.dispatchEventToListeners(WebInspector.Breakpoint.Event.ActionsDidChange);
    },

    saveIdentityToCookie: function(cookie)
    {
        cookie[WebInspector.Breakpoint.URLCookieKey] = this.url;
        cookie[WebInspector.Breakpoint.LineNumberCookieKey] = this.sourceCodeLocation.lineNumber;
        cookie[WebInspector.Breakpoint.ColumnNumberCookieKey] = this.sourceCodeLocation.columnNumber;
    },

    // Protected (Called by BreakpointAction)

    breakpointActionDidChange: function(action)
    {
        var index = this._actions.indexOf(action);
        console.assert(index !== -1);
        if (index === -1)
            return;

        this.dispatchEventToListeners(WebInspector.Breakpoint.Event.ActionsDidChange);
    },

    // Private

    _serializableActions: function()
    {
        var actions = [];
        for (var i = 0; i < this._actions.length; ++i)
            actions.push(this._actions[i].info);
        return actions;
    },

    _popoverToggleEnabledCheckboxChanged: function(event)
    {
        this.disabled = !event.target.checked;
    },

    _popoverConditionInputChanged: function(event)
    {
        this.condition = event.target.value;
    },

    _popoverToggleAutoContinueCheckboxChanged: function(event)
    {
        this.autoContinue = event.target.checked;
    },

    _popoverConditionInputKeyDown: function(event)
    {
        if (this._keyboardShortcutEsc.matchesEvent(event) || this._keyboardShortcutEnter.matchesEvent(event)) {
            this._popover.dismiss();
            event.stopPropagation();
            event.preventDefault();
        }
    },

    _editBreakpointPopoverContentElement: function()
    {
        var content = this._popoverContentElement = document.createElement("div");
        content.className = WebInspector.Breakpoint.PopoverClassName;

        var checkboxElement = document.createElement("input");
        checkboxElement.type = "checkbox";
        checkboxElement.checked = !this._disabled;
        checkboxElement.addEventListener("change", this._popoverToggleEnabledCheckboxChanged.bind(this));

        var checkboxLabel = document.createElement("label");
        checkboxLabel.className = "toggle";
        checkboxLabel.appendChild(checkboxElement);
        checkboxLabel.appendChild(document.createTextNode(this._sourceCodeLocation.displayLocationString()));

        var table = document.createElement("table");

        var conditionRow = table.appendChild(document.createElement("tr"));
        var conditionHeader = conditionRow.appendChild(document.createElement("th"));
        var conditionData = conditionRow.appendChild(document.createElement("td"));
        var conditionLabel = conditionHeader.appendChild(document.createElement("label"));
        var conditionInput = conditionData.appendChild(document.createElement("input"));
        conditionInput.id = WebInspector.Breakpoint.PopoverConditionInputId;
        conditionInput.value = this._condition || "";
        conditionInput.spellcheck = false;
        conditionInput.addEventListener("change", this._popoverConditionInputChanged.bind(this));
        conditionInput.addEventListener("keydown", this._popoverConditionInputKeyDown.bind(this));
        conditionInput.placeholder = WebInspector.UIString("Conditional expression");
        conditionLabel.setAttribute("for", conditionInput.id);
        conditionLabel.textContent = WebInspector.UIString("Condition");

        if (DebuggerAgent.setBreakpoint.supports("options")) {
            var actionRow = table.appendChild(document.createElement("tr"));
            var actionHeader = actionRow.appendChild(document.createElement("th"));
            var actionData = this._actionsContainer = actionRow.appendChild(document.createElement("td"));
            var actionLabel = actionHeader.appendChild(document.createElement("label"));
            actionLabel.textContent = WebInspector.UIString("Action");

            if (!this._actions.length)
                this._popoverActionsCreateAddActionButton();
            else {
                this._popoverContentElement.classList.add(WebInspector.Breakpoint.WidePopoverClassName);
                for (var i = 0; i < this._actions.length; ++i) {
                    var breakpointActionView = new WebInspector.BreakpointActionView(this._actions[i], this, true);
                    this._popoverActionsInsertBreakpointActionView(breakpointActionView, i);
                }
            }

            var optionsRow = this._popoverOptionsRowElement = table.appendChild(document.createElement("tr"));
            if (!this._actions.length)
                optionsRow.classList.add(WebInspector.Breakpoint.HiddenStyleClassName);
            var optionsHeader = optionsRow.appendChild(document.createElement("th"));
            var optionsData = optionsRow.appendChild(document.createElement("td"));
            var optionsLabel = optionsHeader.appendChild(document.createElement("label"));
            var optionsCheckbox = this._popoverOptionsCheckboxElement = optionsData.appendChild(document.createElement("input"));
            var optionsCheckboxLabel = optionsData.appendChild(document.createElement("label"));
            optionsCheckbox.id = WebInspector.Breakpoint.PopoverOptionsAutoContinueInputId;
            optionsCheckbox.type = "checkbox";
            optionsCheckbox.checked = this._autoContinue;
            optionsCheckbox.addEventListener("change", this._popoverToggleAutoContinueCheckboxChanged.bind(this));
            optionsLabel.textContent = WebInspector.UIString("Options");
            optionsCheckboxLabel.setAttribute("for", optionsCheckbox.id);
            optionsCheckboxLabel.textContent = WebInspector.UIString("Automatically continue after evaluating");
        }

        content.appendChild(checkboxLabel);
        content.appendChild(table);

        return content;
    },

    _popoverActionsCreateAddActionButton: function()
    {
        this._popoverContentElement.classList.remove(WebInspector.Breakpoint.WidePopoverClassName);
        this._actionsContainer.removeChildren();

        var addActionButton = this._actionsContainer.appendChild(document.createElement("button"));
        addActionButton.textContent = WebInspector.UIString("Add Action");
        addActionButton.addEventListener("click", this._popoverActionsAddActionButtonClicked.bind(this));
    },

    _popoverActionsAddActionButtonClicked: function(event)
    {
        this._popoverContentElement.classList.add(WebInspector.Breakpoint.WidePopoverClassName);
        this._actionsContainer.removeChildren();

        var newAction = this.createAction(WebInspector.Breakpoint.DefaultBreakpointActionType);
        var newBreakpointActionView = new WebInspector.BreakpointActionView(newAction, this);
        this._popoverActionsInsertBreakpointActionView(newBreakpointActionView, -1);
        this._popoverOptionsRowElement.classList.remove(WebInspector.Breakpoint.HiddenStyleClassName);
        this._popover.update();
    },

    _popoverActionsInsertBreakpointActionView: function(breakpointActionView, index)
    {
        if (index === -1)
            this._actionsContainer.appendChild(breakpointActionView.element);
        else {
            var nextElement = this._actionsContainer.children[index + 1] || null;
            this._actionsContainer.insertBefore(breakpointActionView.element, nextElement);
        }
    },

    breakpointActionViewAppendActionView: function(breakpointActionView, newAction)
    {
        var newBreakpointActionView = new WebInspector.BreakpointActionView(newAction, this);

        var index = 0;
        var children = this._actionsContainer.children;
        for (var i = 0; children.length; ++i) {
            if (children[i] === breakpointActionView.element) {
                index = i;
                break;
            }
        }

        this._popoverActionsInsertBreakpointActionView(newBreakpointActionView, index);
        this._popoverOptionsRowElement.classList.remove(WebInspector.Breakpoint.HiddenStyleClassName);

        this._popover.update();
    },

    breakpointActionViewRemoveActionView: function(breakpointActionView)
    {
        breakpointActionView.element.remove();

        if (!this._actionsContainer.children.length) {
            this._popoverActionsCreateAddActionButton();
            this._popoverOptionsRowElement.classList.add(WebInspector.Breakpoint.HiddenStyleClassName);
            this._popoverOptionsCheckboxElement.checked = false;
        }

        this._popover.update();
    },

    breakpointActionViewResized: function(breakpointActionView)
    {
        this._popover.update();
    },

    willDismissPopover: function(popover)
    {
        console.assert(this._popover === popover);
        delete this._popoverContentElement;
        delete this._popoverOptionsRowElement;
        delete this._popoverOptionsCheckboxElement;
        delete this._actionsContainer;
        delete this._popover;
    },

    _showEditBreakpointPopover: function(boundingClientRect)
    {
        var bounds = WebInspector.Rect.rectFromClientRect(boundingClientRect);
        bounds.origin.x -= 1; // Move the anchor left one pixel so it looks more centered.

        this._popover = this._popover || new WebInspector.Popover(this);
        this._popover.content = this._editBreakpointPopoverContentElement();
        this._popover.present(bounds.pad(2), [WebInspector.RectEdge.MAX_Y]);

        if (!this._keyboardShortcutEsc) {
            this._keyboardShortcutEsc = new WebInspector.KeyboardShortcut(null, WebInspector.KeyboardShortcut.Key.Escape);
            this._keyboardShortcutEnter = new WebInspector.KeyboardShortcut(null, WebInspector.KeyboardShortcut.Key.Enter);
        }

        document.getElementById(WebInspector.Breakpoint.PopoverConditionInputId).select();
    },

    _sourceCodeLocationLocationChanged: function(event)
    {
        this.dispatchEventToListeners(WebInspector.Breakpoint.Event.LocationDidChange, event.data);
    },

    _sourceCodeLocationDisplayLocationChanged: function(event)
    {
        this.dispatchEventToListeners(WebInspector.Breakpoint.Event.DisplayLocationDidChange, event.data);
    }
};

WebInspector.Breakpoint.prototype.__proto__ = WebInspector.Object.prototype;
