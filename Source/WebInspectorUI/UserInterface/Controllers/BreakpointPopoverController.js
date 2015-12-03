/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.BreakpointPopoverController = class BreakpointPopoverController extends WebInspector.Object
{
    constructor()
    {
        super();

        this._breakpoint = null;
        this._popover = null;
        this._popoverContentElement = null;
    }

    // Public

    appendContextMenuItems(contextMenu, breakpoint, breakpointDisplayElement)
    {
        console.assert(document.body.contains(breakpointDisplayElement), "Breakpoint popover display element must be in the DOM.");

        function editBreakpoint()
        {
            console.assert(!this._popover, "Breakpoint popover already exists.");
            if (this._popover)
                return;

            this._createPopoverContent(breakpoint);
            this._popover = new WebInspector.Popover(this);
            this._popover.content = this._popoverContentElement;

            var bounds = WebInspector.Rect.rectFromClientRect(breakpointDisplayElement.getBoundingClientRect());
            bounds.origin.x -= 1; // Move the anchor left one pixel so it looks more centered.
            this._popover.present(bounds.pad(2), [WebInspector.RectEdge.MAX_Y]);
        }

        function removeBreakpoint()
        {
            WebInspector.debuggerManager.removeBreakpoint(breakpoint);
        }

        function toggleBreakpoint()
        {
            breakpoint.disabled = !breakpoint.disabled;
        }

        function toggleAutoContinue()
        {
            breakpoint.autoContinue = !breakpoint.autoContinue;
        }

        function revealOriginalSourceCodeLocation()
        {
            WebInspector.showOriginalOrFormattedSourceCodeLocation(breakpoint.sourceCodeLocation);
        }

        if (WebInspector.debuggerManager.isBreakpointEditable(breakpoint))
            contextMenu.appendItem(WebInspector.UIString("Edit Breakpoint…"), editBreakpoint.bind(this));

        if (breakpoint.autoContinue && !breakpoint.disabled) {
            contextMenu.appendItem(WebInspector.UIString("Disable Breakpoint"), toggleBreakpoint.bind(this));
            contextMenu.appendItem(WebInspector.UIString("Cancel Automatic Continue"), toggleAutoContinue.bind(this));
        } else if (!breakpoint.disabled)
            contextMenu.appendItem(WebInspector.UIString("Disable Breakpoint"), toggleBreakpoint.bind(this));
        else
            contextMenu.appendItem(WebInspector.UIString("Enable Breakpoint"), toggleBreakpoint.bind(this));

        if (!breakpoint.autoContinue && !breakpoint.disabled && breakpoint.actions.length)
            contextMenu.appendItem(WebInspector.UIString("Set to Automatically Continue"), toggleAutoContinue.bind(this));

        if (WebInspector.debuggerManager.isBreakpointRemovable(breakpoint)) {
            contextMenu.appendSeparator();
            contextMenu.appendItem(WebInspector.UIString("Delete Breakpoint"), removeBreakpoint.bind(this));
        }

        if (breakpoint._sourceCodeLocation.hasMappedLocation()) {
            contextMenu.appendSeparator();
            contextMenu.appendItem(WebInspector.UIString("Reveal in Original Resource"), revealOriginalSourceCodeLocation.bind(this));
        }
    }

    // CodeMirrorCompletionController delegate

    completionControllerShouldAllowEscapeCompletion()
    {
        return false;
    }

    // Private

    _createPopoverContent(breakpoint)
    {
        console.assert(!this._popoverContentElement, "Popover content element already exists.");
        if (this._popoverContentElement)
            return;

        this._breakpoint = breakpoint;
        this._popoverContentElement = document.createElement("div");
        this._popoverContentElement.className = "edit-breakpoint-popover-content";

        var checkboxElement = document.createElement("input");
        checkboxElement.type = "checkbox";
        checkboxElement.checked = !this._breakpoint.disabled;
        checkboxElement.addEventListener("change", this._popoverToggleEnabledCheckboxChanged.bind(this));

        var checkboxLabel = document.createElement("label");
        checkboxLabel.className = "toggle";
        checkboxLabel.appendChild(checkboxElement);
        checkboxLabel.appendChild(document.createTextNode(this._breakpoint.sourceCodeLocation.displayLocationString()));

        var table = document.createElement("table");

        var conditionRow = table.appendChild(document.createElement("tr"));
        var conditionHeader = conditionRow.appendChild(document.createElement("th"));
        var conditionData = conditionRow.appendChild(document.createElement("td"));
        var conditionLabel = conditionHeader.appendChild(document.createElement("label"));
        conditionLabel.textContent = WebInspector.UIString("Condition");
        var conditionEditorElement = conditionData.appendChild(document.createElement("div"));
        conditionEditorElement.classList.add("edit-breakpoint-popover-condition", WebInspector.SyntaxHighlightedStyleClassName);

        this._conditionCodeMirror = WebInspector.CodeMirrorEditor.create(conditionEditorElement, {
            extraKeys: {Tab: false},
            lineWrapping: false,
            mode: "text/javascript",
            matchBrackets: true,
            placeholder: WebInspector.UIString("Conditional expression"),
            scrollbarStyle: null,
            value: this._breakpoint.condition || "",
        });

        var conditionCodeMirrorInputField = this._conditionCodeMirror.getInputField();
        conditionCodeMirrorInputField.id = "codemirror-condition-input-field";
        conditionLabel.setAttribute("for", conditionCodeMirrorInputField.id);

        this._conditionCodeMirrorEscapeOrEnterKeyHandler = this._conditionCodeMirrorEscapeOrEnterKey.bind(this);
        this._conditionCodeMirror.addKeyMap({
            "Esc": this._conditionCodeMirrorEscapeOrEnterKeyHandler,
            "Enter": this._conditionCodeMirrorEscapeOrEnterKeyHandler,
        });

        this._conditionCodeMirror.on("change", this._conditionCodeMirrorChanged.bind(this));
        this._conditionCodeMirror.on("beforeChange", this._conditionCodeMirrorBeforeChange.bind(this));

        var completionController = new WebInspector.CodeMirrorCompletionController(this._conditionCodeMirror, this);
        completionController.addExtendedCompletionProvider("javascript", WebInspector.javaScriptRuntimeCompletionProvider);

        // CodeMirror needs a refresh after the popover displays, to layout, otherwise it doesn't appear.
        setTimeout(function() {
            this._conditionCodeMirror.refresh();
            this._conditionCodeMirror.focus();
        }.bind(this), 0);

        // COMPATIBILITY (iOS 7): Debugger.setBreakpoint did not support options.
        if (DebuggerAgent.setBreakpoint.supports("options")) {
            // COMPATIBILITY (iOS 9): Legacy backends don't support breakpoint ignore count. Since support
            // can't be tested directly, check for CSS.getSupportedSystemFontFamilyNames.
            // FIXME: Use explicit version checking once https://webkit.org/b/148680 is fixed.
            if (CSSAgent.getSupportedSystemFontFamilyNames) {
                var ignoreCountRow = table.appendChild(document.createElement("tr"));
                var ignoreCountHeader = ignoreCountRow.appendChild(document.createElement("th"));
                var ignoreCountLabel = ignoreCountHeader.appendChild(document.createElement("label"));
                var ignoreCountData = ignoreCountRow.appendChild(document.createElement("td"));
                this._ignoreCountInput = ignoreCountData.appendChild(document.createElement("input"));
                this._ignoreCountInput.id = "edit-breakpoint-popover-ignore";
                this._ignoreCountInput.type = "number";
                this._ignoreCountInput.min = 0;
                this._ignoreCountInput.value = this._ignoreCount || 0;
                this._ignoreCountInput.addEventListener("change", this._popoverIgnoreInputChanged.bind(this));

                var ignoreCountText = ignoreCountData.appendChild(document.createElement("span"));
                ignoreCountLabel.setAttribute("for", this._ignoreCountInput.id);
                ignoreCountLabel.textContent = WebInspector.UIString("Ignore");
                ignoreCountText.textContent = WebInspector.UIString("times before stopping");
            }

            var actionRow = table.appendChild(document.createElement("tr"));
            var actionHeader = actionRow.appendChild(document.createElement("th"));
            var actionData = this._actionsContainer = actionRow.appendChild(document.createElement("td"));
            var actionLabel = actionHeader.appendChild(document.createElement("label"));
            actionLabel.textContent = WebInspector.UIString("Action");

            if (!this._breakpoint.actions.length)
                this._popoverActionsCreateAddActionButton();
            else {
                this._popoverContentElement.classList.add(WebInspector.BreakpointPopoverController.WidePopoverClassName);
                for (var i = 0; i < this._breakpoint.actions.length; ++i) {
                    var breakpointActionView = new WebInspector.BreakpointActionView(this._breakpoint.actions[i], this, true);
                    this._popoverActionsInsertBreakpointActionView(breakpointActionView, i);
                }
            }

            var optionsRow = this._popoverOptionsRowElement = table.appendChild(document.createElement("tr"));
            if (!this._breakpoint.actions.length)
                optionsRow.classList.add(WebInspector.BreakpointPopoverController.HiddenStyleClassName);
            var optionsHeader = optionsRow.appendChild(document.createElement("th"));
            var optionsData = optionsRow.appendChild(document.createElement("td"));
            var optionsLabel = optionsHeader.appendChild(document.createElement("label"));
            var optionsCheckbox = this._popoverOptionsCheckboxElement = optionsData.appendChild(document.createElement("input"));
            var optionsCheckboxLabel = optionsData.appendChild(document.createElement("label"));
            optionsCheckbox.id = "edit-breakpoint-popoover-auto-continue";
            optionsCheckbox.type = "checkbox";
            optionsCheckbox.checked = this._breakpoint.autoContinue;
            optionsCheckbox.addEventListener("change", this._popoverToggleAutoContinueCheckboxChanged.bind(this));
            optionsLabel.textContent = WebInspector.UIString("Options");
            optionsCheckboxLabel.setAttribute("for", optionsCheckbox.id);
            optionsCheckboxLabel.textContent = WebInspector.UIString("Automatically continue after evaluating");
        }

        this._popoverContentElement.appendChild(checkboxLabel);
        this._popoverContentElement.appendChild(table);
    }

    _popoverToggleEnabledCheckboxChanged(event)
    {
        this._breakpoint.disabled = !event.target.checked;
    }

    _conditionCodeMirrorChanged(codeMirror, change)
    {
        this._breakpoint.condition = (codeMirror.getValue() || "").trim();
    }

    _conditionCodeMirrorBeforeChange(codeMirror, change)
    {
        var newText = change.text.join("").replace(/\n/g, "");
        change.update(change.from, change.to, [newText]);
        return true;
    }

    _conditionCodeMirrorEscapeOrEnterKey()
    {
        this._popover.dismiss();
    }

    _popoverIgnoreInputChanged(event)
    {
        var ignoreCount = 0;
        if (event.target.value) {
            ignoreCount = parseInt(event.target.value, 10);
            if (isNaN(ignoreCount) || ignoreCount < 0)
                ignoreCount = 0;
        }

        this._ignoreCountInput.value = ignoreCount;
        this._breakpoint.ignoreCount = ignoreCount;
    }

    _popoverToggleAutoContinueCheckboxChanged(event)
    {
        this._breakpoint.autoContinue = event.target.checked;
    }

    _popoverActionsCreateAddActionButton()
    {
        this._popoverContentElement.classList.remove(WebInspector.BreakpointPopoverController.WidePopoverClassName);
        this._actionsContainer.removeChildren();

        var addActionButton = this._actionsContainer.appendChild(document.createElement("button"));
        addActionButton.textContent = WebInspector.UIString("Add Action");
        addActionButton.addEventListener("click", this._popoverActionsAddActionButtonClicked.bind(this));
    }

    _popoverActionsAddActionButtonClicked(event)
    {
        this._popoverContentElement.classList.add(WebInspector.BreakpointPopoverController.WidePopoverClassName);
        this._actionsContainer.removeChildren();

        var newAction = this._breakpoint.createAction(WebInspector.Breakpoint.DefaultBreakpointActionType);
        var newBreakpointActionView = new WebInspector.BreakpointActionView(newAction, this);
        this._popoverActionsInsertBreakpointActionView(newBreakpointActionView, -1);
        this._popoverOptionsRowElement.classList.remove(WebInspector.BreakpointPopoverController.HiddenStyleClassName);
        this._popover.update();
    }

    _popoverActionsInsertBreakpointActionView(breakpointActionView, index)
    {
        if (index === -1)
            this._actionsContainer.appendChild(breakpointActionView.element);
        else {
            var nextElement = this._actionsContainer.children[index + 1] || null;
            this._actionsContainer.insertBefore(breakpointActionView.element, nextElement);
        }
    }

    breakpointActionViewAppendActionView(breakpointActionView, newAction)
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
        this._popoverOptionsRowElement.classList.remove(WebInspector.BreakpointPopoverController.HiddenStyleClassName);

        this._popover.update();
    }

    breakpointActionViewRemoveActionView(breakpointActionView)
    {
        breakpointActionView.element.remove();

        if (!this._actionsContainer.children.length) {
            this._popoverActionsCreateAddActionButton();
            this._popoverOptionsRowElement.classList.add(WebInspector.BreakpointPopoverController.HiddenStyleClassName);
            this._popoverOptionsCheckboxElement.checked = false;
        }

        this._popover.update();
    }

    breakpointActionViewResized(breakpointActionView)
    {
        this._popover.update();
    }

    willDismissPopover(popover)
    {
        console.assert(this._popover === popover);
        this._popoverContentElement = null;
        this._popoverOptionsRowElement = null;
        this._popoverOptionsCheckboxElement = null;
        this._actionsContainer = null;
        this._popover = null;
    }

    didDismissPopover(popover)
    {
        // Remove Evaluate and Probe actions that have no data.
        var emptyActions = this._breakpoint.actions.filter(function(action) {
            if (action.type !== WebInspector.BreakpointAction.Type.Evaluate && action.type !== WebInspector.BreakpointAction.Type.Probe)
                return false;
            return !(action.data && action.data.trim());
        });

        for (var action of emptyActions)
            this._breakpoint.removeAction(action);

        this._breakpoint = null;
    }
};

WebInspector.BreakpointPopoverController.WidePopoverClassName = "wide";
WebInspector.BreakpointPopoverController.HiddenStyleClassName = "hidden";
