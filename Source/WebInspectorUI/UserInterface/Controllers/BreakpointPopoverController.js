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

WI.BreakpointPopoverController = class BreakpointPopoverController extends WI.Object
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

        const editBreakpoint = () => {
            console.assert(!this._popover, "Breakpoint popover already exists.");
            if (this._popover)
                return;

            this._createPopoverContent(breakpoint);
            this._popover = new WI.Popover(this);
            this._popover.content = this._popoverContentElement;

            let bounds = WI.Rect.rectFromClientRect(breakpointDisplayElement.getBoundingClientRect());
            bounds.origin.x -= 1; // Move the anchor left one pixel so it looks more centered.
            this._popover.present(bounds.pad(2), [WI.RectEdge.MAX_Y]);
        };

        const removeBreakpoint = () => {
            WI.debuggerManager.removeBreakpoint(breakpoint);
        };

        const toggleBreakpoint = () => {
            breakpoint.disabled = !breakpoint.disabled;
        };

        const toggleAutoContinue = () => {
            breakpoint.autoContinue = !breakpoint.autoContinue;
        };

        const revealOriginalSourceCodeLocation = () => {
            const options = {
                ignoreNetworkTab: true,
                ignoreSearchTab: true,
            };
            WI.showOriginalOrFormattedSourceCodeLocation(breakpoint.sourceCodeLocation, options);
        };

        if (WI.debuggerManager.isBreakpointEditable(breakpoint))
            contextMenu.appendItem(WI.UIString("Edit Breakpoint\u2026"), editBreakpoint);

        if (breakpoint.autoContinue && !breakpoint.disabled) {
            contextMenu.appendItem(WI.UIString("Disable Breakpoint"), toggleBreakpoint);
            contextMenu.appendItem(WI.UIString("Cancel Automatic Continue"), toggleAutoContinue);
        } else if (!breakpoint.disabled)
            contextMenu.appendItem(WI.UIString("Disable Breakpoint"), toggleBreakpoint);
        else
            contextMenu.appendItem(WI.UIString("Enable Breakpoint"), toggleBreakpoint);

        if (!breakpoint.autoContinue && !breakpoint.disabled && breakpoint.actions.length)
            contextMenu.appendItem(WI.UIString("Set to Automatically Continue"), toggleAutoContinue);

        if (WI.debuggerManager.isBreakpointRemovable(breakpoint))
            contextMenu.appendItem(WI.UIString("Delete Breakpoint"), removeBreakpoint);

        if (breakpoint._sourceCodeLocation.hasMappedLocation()) {
            contextMenu.appendSeparator();
            contextMenu.appendItem(WI.UIString("Reveal in Original Resource"), revealOriginalSourceCodeLocation);
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

        let checkboxElement = document.createElement("input");
        checkboxElement.type = "checkbox";
        checkboxElement.checked = !this._breakpoint.disabled;
        checkboxElement.addEventListener("change", this._popoverToggleEnabledCheckboxChanged.bind(this));

        let checkboxLabel = document.createElement("label");
        checkboxLabel.className = "toggle";
        checkboxLabel.appendChild(checkboxElement);
        checkboxLabel.append(this._breakpoint.sourceCodeLocation.displayLocationString());

        let table = document.createElement("table");

        let conditionRow = table.appendChild(document.createElement("tr"));
        let conditionHeader = conditionRow.appendChild(document.createElement("th"));
        let conditionData = conditionRow.appendChild(document.createElement("td"));
        let conditionLabel = conditionHeader.appendChild(document.createElement("label"));
        conditionLabel.textContent = WI.UIString("Condition");
        let conditionEditorElement = conditionData.appendChild(document.createElement("div"));
        conditionEditorElement.classList.add("edit-breakpoint-popover-condition", WI.SyntaxHighlightedStyleClassName);

        this._conditionCodeMirror = WI.CodeMirrorEditor.create(conditionEditorElement, {
            extraKeys: {Tab: false},
            lineWrapping: false,
            mode: "text/javascript",
            matchBrackets: true,
            placeholder: WI.UIString("Conditional expression"),
            scrollbarStyle: null,
            value: this._breakpoint.condition || "",
        });

        let conditionCodeMirrorInputField = this._conditionCodeMirror.getInputField();
        conditionCodeMirrorInputField.id = "codemirror-condition-input-field";
        conditionLabel.setAttribute("for", conditionCodeMirrorInputField.id);

        this._conditionCodeMirrorEscapeOrEnterKeyHandler = this._conditionCodeMirrorEscapeOrEnterKey.bind(this);
        this._conditionCodeMirror.addKeyMap({
            "Esc": this._conditionCodeMirrorEscapeOrEnterKeyHandler,
            "Enter": this._conditionCodeMirrorEscapeOrEnterKeyHandler,
        });

        this._conditionCodeMirror.on("change", this._conditionCodeMirrorChanged.bind(this));
        this._conditionCodeMirror.on("beforeChange", this._conditionCodeMirrorBeforeChange.bind(this));

        let completionController = new WI.CodeMirrorCompletionController(this._conditionCodeMirror, this);
        completionController.addExtendedCompletionProvider("javascript", WI.javaScriptRuntimeCompletionProvider);

        // CodeMirror needs a refresh after the popover displays, to layout, otherwise it doesn't appear.
        setTimeout(() => {
            this._conditionCodeMirror.refresh();
            this._conditionCodeMirror.focus();
        }, 0);

        // COMPATIBILITY (iOS 9): Legacy backends don't support breakpoint ignore count. Since support
        // can't be tested directly, check for CSS.getSupportedSystemFontFamilyNames.
        // FIXME: Use explicit version checking once https://webkit.org/b/148680 is fixed.
        if (InspectorBackend.domains.CSS.getSupportedSystemFontFamilyNames) {
            let ignoreCountRow = table.appendChild(document.createElement("tr"));
            let ignoreCountHeader = ignoreCountRow.appendChild(document.createElement("th"));
            let ignoreCountLabel = ignoreCountHeader.appendChild(document.createElement("label"));
            let ignoreCountData = ignoreCountRow.appendChild(document.createElement("td"));
            this._ignoreCountInput = ignoreCountData.appendChild(document.createElement("input"));
            this._ignoreCountInput.id = "edit-breakpoint-popover-ignore";
            this._ignoreCountInput.type = "number";
            this._ignoreCountInput.min = 0;
            this._ignoreCountInput.value = 0;
            this._ignoreCountInput.addEventListener("change", this._popoverIgnoreInputChanged.bind(this));

            ignoreCountLabel.setAttribute("for", this._ignoreCountInput.id);
            ignoreCountLabel.textContent = WI.UIString("Ignore");

            this._ignoreCountText = ignoreCountData.appendChild(document.createElement("span"));
            this._updateIgnoreCountText();
        }

        let actionRow = table.appendChild(document.createElement("tr"));
        let actionHeader = actionRow.appendChild(document.createElement("th"));
        let actionData = this._actionsContainer = actionRow.appendChild(document.createElement("td"));
        let actionLabel = actionHeader.appendChild(document.createElement("label"));
        actionLabel.textContent = WI.UIString("Action");

        if (!this._breakpoint.actions.length)
            this._popoverActionsCreateAddActionButton();
        else {
            this._popoverContentElement.classList.add(WI.BreakpointPopoverController.WidePopoverClassName);
            for (let i = 0; i < this._breakpoint.actions.length; ++i) {
                let breakpointActionView = new WI.BreakpointActionView(this._breakpoint.actions[i], this, true);
                this._popoverActionsInsertBreakpointActionView(breakpointActionView, i);
            }
        }

        let optionsRow = this._popoverOptionsRowElement = table.appendChild(document.createElement("tr"));
        if (!this._breakpoint.actions.length)
            optionsRow.classList.add(WI.BreakpointPopoverController.HiddenStyleClassName);
        let optionsHeader = optionsRow.appendChild(document.createElement("th"));
        let optionsData = optionsRow.appendChild(document.createElement("td"));
        let optionsLabel = optionsHeader.appendChild(document.createElement("label"));
        let optionsCheckbox = this._popoverOptionsCheckboxElement = optionsData.appendChild(document.createElement("input"));
        let optionsCheckboxLabel = optionsData.appendChild(document.createElement("label"));
        optionsCheckbox.id = "edit-breakpoint-popover-auto-continue";
        optionsCheckbox.type = "checkbox";
        optionsCheckbox.checked = this._breakpoint.autoContinue;
        optionsCheckbox.addEventListener("change", this._popoverToggleAutoContinueCheckboxChanged.bind(this));
        optionsLabel.textContent = WI.UIString("Options");
        optionsCheckboxLabel.setAttribute("for", optionsCheckbox.id);
        optionsCheckboxLabel.textContent = WI.UIString("Automatically continue after evaluating");

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
        if (change.update) {
            let newText = change.text.join("").replace(/\n/g, "");
            change.update(change.from, change.to, [newText]);
        }

        return true;
    }

    _conditionCodeMirrorEscapeOrEnterKey()
    {
        if (!this._popover)
            return;

        this._popover.dismiss();
    }

    _popoverIgnoreInputChanged(event)
    {
        let ignoreCount = 0;
        if (event.target.value) {
            ignoreCount = parseInt(event.target.value, 10);
            if (isNaN(ignoreCount) || ignoreCount < 0)
                ignoreCount = 0;
        }

        this._ignoreCountInput.value = ignoreCount;
        this._breakpoint.ignoreCount = ignoreCount;

        this._updateIgnoreCountText();
    }

    _popoverToggleAutoContinueCheckboxChanged(event)
    {
        this._breakpoint.autoContinue = event.target.checked;
    }

    _popoverActionsCreateAddActionButton()
    {
        this._popoverContentElement.classList.remove(WI.BreakpointPopoverController.WidePopoverClassName);
        this._actionsContainer.removeChildren();

        let addActionButton = this._actionsContainer.appendChild(document.createElement("button"));
        addActionButton.textContent = WI.UIString("Add Action");
        addActionButton.addEventListener("click", this._popoverActionsAddActionButtonClicked.bind(this));
    }

    _popoverActionsAddActionButtonClicked(event)
    {
        this._popoverContentElement.classList.add(WI.BreakpointPopoverController.WidePopoverClassName);
        this._actionsContainer.removeChildren();

        let newAction = this._breakpoint.createAction(WI.BreakpointAction.Type.Log);
        let newBreakpointActionView = new WI.BreakpointActionView(newAction, this);
        this._popoverActionsInsertBreakpointActionView(newBreakpointActionView, -1);
        this._popoverOptionsRowElement.classList.remove(WI.BreakpointPopoverController.HiddenStyleClassName);
        this._popover.update();
    }

    _popoverActionsInsertBreakpointActionView(breakpointActionView, index)
    {
        if (index === -1)
            this._actionsContainer.appendChild(breakpointActionView.element);
        else {
            let nextElement = this._actionsContainer.children[index + 1] || null;
            this._actionsContainer.insertBefore(breakpointActionView.element, nextElement);
        }
    }

    _updateIgnoreCountText()
    {
        if (this._breakpoint.ignoreCount === 1)
            this._ignoreCountText.textContent = WI.UIString("time before stopping");
        else
            this._ignoreCountText.textContent = WI.UIString("times before stopping");
    }

    breakpointActionViewAppendActionView(breakpointActionView, newAction)
    {
        let newBreakpointActionView = new WI.BreakpointActionView(newAction, this);

        let index = 0;
        let children = this._actionsContainer.children;
        for (let i = 0; children.length; ++i) {
            if (children[i] === breakpointActionView.element) {
                index = i;
                break;
            }
        }

        this._popoverActionsInsertBreakpointActionView(newBreakpointActionView, index);
        this._popoverOptionsRowElement.classList.remove(WI.BreakpointPopoverController.HiddenStyleClassName);

        this._popover.update();
    }

    breakpointActionViewRemoveActionView(breakpointActionView)
    {
        breakpointActionView.element.remove();

        if (!this._actionsContainer.children.length) {
            this._popoverActionsCreateAddActionButton();
            this._popoverOptionsRowElement.classList.add(WI.BreakpointPopoverController.HiddenStyleClassName);
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
        let emptyActions = this._breakpoint.actions.filter(function(action) {
            if (action.type !== WI.BreakpointAction.Type.Evaluate && action.type !== WI.BreakpointAction.Type.Probe)
                return false;
            return !(action.data && action.data.trim());
        });

        for (let action of emptyActions)
            this._breakpoint.removeAction(action);

        this._breakpoint = null;
    }
};

WI.BreakpointPopoverController.WidePopoverClassName = "wide";
WI.BreakpointPopoverController.HiddenStyleClassName = "hidden";
