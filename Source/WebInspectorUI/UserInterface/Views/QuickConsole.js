/*
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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

WI.QuickConsole = class QuickConsole extends WI.View
{
    constructor(element)
    {
        super(element);

        this._toggleOrFocusKeyboardShortcut = new WI.KeyboardShortcut(null, WI.KeyboardShortcut.Key.Escape, this._toggleOrFocus.bind(this));
        this._toggleOrFocusKeyboardShortcut.implicitlyPreventsDefault = false;
        this._keyboardShortcutDisabled = false;

        this._useExecutionContextOfInspectedNode = InspectorBackend.hasDomain("DOM");
        this._restoreSelectedExecutionContextForFrame = null;

        this.element.classList.add("quick-console");
        this.element.addEventListener("mousedown", this._handleMouseDown.bind(this));
        this.element.addEventListener("dragover", this._handleDragOver.bind(this));
        this.element.addEventListener("drop", this._handleDrop.bind(this), true); // Ensure that dropping a DOM node doesn't copy text.

        this.prompt = new WI.ConsolePrompt(null, "text/javascript");
        this.addSubview(this.prompt);

        // FIXME: CodeMirror 4 has a default "Esc" key handler that always prevents default.
        // Our keyboard shortcut above will respect the default prevented and ignore the event
        // and not toggle the console. Install our own Escape key handler that will trigger
        // when the ConsolePrompt is empty, to restore toggling behavior. A better solution
        // would be for CodeMirror's event handler to pass if it doesn't do anything.
        this.prompt.escapeKeyHandlerWhenEmpty = function() { WI.toggleSplitConsole(); };

        this._navigationBar = new WI.SizesToFitNavigationBar;
        this.addSubview(this._navigationBar);

        this._activeExecutionContextNavigationItemDivider = new WI.DividerNavigationItem;
        this._navigationBar.addNavigationItem(this._activeExecutionContextNavigationItemDivider);

        this._activeExecutionContextNavigationItem = new WI.NavigationItem("active-execution-context");
        WI.addMouseDownContextMenuHandlers(this._activeExecutionContextNavigationItem.element, this._populateActiveExecutionContextNavigationItemContextMenu.bind(this));
        this._navigationBar.addNavigationItem(this._activeExecutionContextNavigationItem);

        this._updateActiveExecutionContextDisplay();

        WI.settings.consoleSavedResultAlias.addEventListener(WI.Setting.Event.Changed, this._handleConsoleSavedResultAliasSettingChanged, this);
        WI.settings.engineeringShowInternalExecutionContexts.addEventListener(WI.Setting.Event.Changed, this._handleEngineeringShowInternalExecutionContextsSettingChanged, this);

        WI.Frame.addEventListener(WI.Frame.Event.PageExecutionContextChanged, this._handleFramePageExecutionContextChanged, this);
        WI.Frame.addEventListener(WI.Frame.Event.ExecutionContextsCleared, this._handleFrameExecutionContextsCleared, this);

        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.ActiveCallFrameDidChange, this._handleDebuggerActiveCallFrameDidChange, this);

        WI.runtimeManager.addEventListener(WI.RuntimeManager.Event.ActiveExecutionContextChanged, this._handleActiveExecutionContextChanged, this);

        WI.notifications.addEventListener(WI.Notification.TransitionPageTarget, this._handleTransitionPageTarget, this);

        WI.targetManager.addEventListener(WI.TargetManager.Event.TargetRemoved, this._handleTargetRemoved, this);

        WI.domManager.addEventListener(WI.DOMManager.Event.InspectedNodeChanged, this._handleInspectedNodeChanged, this);

        WI.consoleDrawer.toggleButtonShortcutTooltip(this._toggleOrFocusKeyboardShortcut);
        WI.consoleDrawer.addEventListener(WI.ConsoleDrawer.Event.CollapsedStateChanged, this._updateStyles, this);
        WI.TabBrowser.addEventListener(WI.TabBrowser.Event.SelectedTabContentViewDidChange, this._updateStyles, this);

        WI.whenTargetsAvailable().then(() => {
            this._updateActiveExecutionContextDisplay();
        });
    }

    // Public

    set keyboardShortcutDisabled(disabled)
    {
        this._keyboardShortcutDisabled = disabled;
    }

    closed()
    {
        WI.settings.consoleSavedResultAlias.removeEventListener(null, null, this);
        WI.Frame.removeEventListener(null, null, this);
        WI.debuggerManager.removeEventListener(null, null, this);
        WI.runtimeManager.removeEventListener(null, null, this);
        WI.targetManager.removeEventListener(null, null, this);
        WI.consoleDrawer.removeEventListener(null, null, this);
        WI.TabBrowser.removeEventListener(null, null, this);

        super.closed();
    }

    // Private

    _displayNameForExecutionContext(context, maxLength = Infinity)
    {
        function truncate(string, length) {
            if (!Number.isFinite(maxLength))
                return string;
            return string.trim().truncateMiddle(length);
        }

        if (context.type === WI.ExecutionContext.Type.Internal)
            return WI.unlocalizedString("[Internal] ") + context.name;

        let target = context.target;
        if (target.type === WI.TargetType.Worker)
            return truncate(target.displayName, maxLength);

        let frame = context.frame;
        if (frame) {
            if (context === frame.executionContextList.pageExecutionContext) {
                let resourceName = frame.mainResource.displayName;
                let frameName = frame.name;
                if (frameName) {
                    // Attempt to show all of the frame name, but ensure that at least 20 characters
                    // of the resource name are shown as well.
                    let frameNameMaxLength = Math.max(maxLength - resourceName.length, 20);
                    return WI.UIString("%s (%s)").format(truncate(frameName, frameNameMaxLength), truncate(resourceName, maxLength - frameNameMaxLength));
                }
                return truncate(resourceName, maxLength);
            }
        }

        return truncate(context.name, maxLength);
    }

    _resolveDesiredActiveExecutionContext(forceInspectedNode)
    {
        let executionContext = null;

        if (this._useExecutionContextOfInspectedNode || forceInspectedNode) {
            let inspectedNode = WI.domManager.inspectedNode;
            if (inspectedNode) {
                let frame = inspectedNode.frame;
                if (frame) {
                    let pageExecutionContext = frame.pageExecutionContext;
                    if (pageExecutionContext)
                        executionContext = pageExecutionContext;
                }
            }
        }

        if (!executionContext && WI.networkManager.mainFrame)
            executionContext = WI.networkManager.mainFrame.pageExecutionContext;

        return executionContext || WI.mainTarget.executionContext;
    }

    _setActiveExecutionContext(context)
    {
        let wasActive = WI.runtimeManager.activeExecutionContext === context;

        WI.runtimeManager.activeExecutionContext = context;

        if (wasActive)
            this._updateActiveExecutionContextDisplay();
    }

    _updateActiveExecutionContextDisplay()
    {
        let toggleHidden = (hidden) => {
            this._activeExecutionContextNavigationItemDivider.hidden = hidden;
            this._activeExecutionContextNavigationItem.hidden = hidden;
        };

        if (WI.debuggerManager.activeCallFrame) {
            toggleHidden(true);
            return;
        }

        if (!WI.runtimeManager.activeExecutionContext || !WI.networkManager.mainFrame) {
            toggleHidden(true);
            return;
        }

        if (WI.networkManager.frames.length === 1 && WI.networkManager.mainFrame.executionContextList.contexts.length === 1 && !WI.targetManager.workerTargets.length) {
            toggleHidden(true);
            return;
        }

        const maxLength = 40;

        if (this._useExecutionContextOfInspectedNode) {
            this._activeExecutionContextNavigationItem.element.classList.add("automatic");
            this._activeExecutionContextNavigationItem.element.textContent = WI.UIString("Auto \u2014 %s").format(this._displayNameForExecutionContext(WI.runtimeManager.activeExecutionContext, maxLength));
            this._activeExecutionContextNavigationItem.tooltip = WI.UIString("Execution context for %s").format(WI.RuntimeManager.preferredSavedResultPrefix() + "0");
        } else {
            this._activeExecutionContextNavigationItem.element.classList.remove("automatic");
            this._activeExecutionContextNavigationItem.element.textContent = this._displayNameForExecutionContext(WI.runtimeManager.activeExecutionContext, maxLength);
            this._activeExecutionContextNavigationItem.tooltip = this._displayNameForExecutionContext(WI.runtimeManager.activeExecutionContext);
        }

        this._activeExecutionContextNavigationItem.element.appendChild(WI.ImageUtilities.useSVGSymbol("Images/UpDownArrows.svg", "selector-arrows"));

        toggleHidden(false);
    }

    _populateActiveExecutionContextNavigationItemContextMenu(contextMenu)
    {
        const maxLength = 120;

        let activeExecutionContext = WI.runtimeManager.activeExecutionContext;

        if (InspectorBackend.hasDomain("DOM")) {
            let executionContextForInspectedNode = this._resolveDesiredActiveExecutionContext(true);
            contextMenu.appendCheckboxItem(WI.UIString("Auto \u2014 %s").format(this._displayNameForExecutionContext(executionContextForInspectedNode, maxLength)), () => {
                this._useExecutionContextOfInspectedNode = true;
                this._setActiveExecutionContext(executionContextForInspectedNode);
            }, this._useExecutionContextOfInspectedNode);

            contextMenu.appendSeparator();
        }

        let indent = 0;
        let addExecutionContext = (context) => {
            if (context.type === WI.ExecutionContext.Type.Internal && !WI.settings.engineeringShowInternalExecutionContexts.value)
                return;

            let additionalIndent = (context.frame && context !== context.frame.executionContextList.pageExecutionContext) || context.type !== WI.ExecutionContext.Type.Normal;

            // Mimic macOS `-[NSMenuItem setIndentationLevel]`.
            contextMenu.appendCheckboxItem("   ".repeat(indent + additionalIndent) + this._displayNameForExecutionContext(context, maxLength), () => {
                this._useExecutionContextOfInspectedNode = false;
                this._setActiveExecutionContext(context);
            }, activeExecutionContext === context);
        };

        let addExecutionContextsForFrame = (frame) => {
            let pageExecutionContext = frame.executionContextList.pageExecutionContext;

            let contexts = frame.executionContextList.contexts.sort((a, b) => {
                if (a === pageExecutionContext)
                    return -1;
                if (b === pageExecutionContext)
                    return 1;

                const executionContextTypeRanking = [
                    WI.ExecutionContext.Type.Normal,
                    WI.ExecutionContext.Type.User,
                    WI.ExecutionContext.Type.Internal,
                ];
                return executionContextTypeRanking.indexOf(a.type) - executionContextTypeRanking.indexOf(b.type);
            });
            for (let context of contexts)
                addExecutionContext(context);
        };

        let mainFrame = WI.networkManager.mainFrame;
        addExecutionContextsForFrame(mainFrame);

        indent = 1;

        let otherFrames = WI.networkManager.frames.filter((frame) => frame !== mainFrame && frame.executionContextList.pageExecutionContext);
        if (otherFrames.length) {
            contextMenu.appendHeader(WI.UIString("Frames"));

            for (let frame of otherFrames)
                addExecutionContextsForFrame(frame);
        }

        let workerTargets = WI.targetManager.workerTargets;
        if (workerTargets.length) {
            contextMenu.appendHeader(WI.UIString("Workers"));

            for (let target of workerTargets)
                addExecutionContext(target.executionContext);
        }
    }

    _handleMouseDown(event)
    {
        if (event.target !== this.element)
            return;

        event.preventDefault();
        this.prompt.focus();
    }

    _handleDragOver(event)
    {
        if (event.dataTransfer.types.includes(WI.DOMTreeOutline.DOMNodeIdDragType)) {
            event.preventDefault();
            event.dataTransfer.dropEffect = "copy";
        }
    }

    _handleDrop(event)
    {
        let domNodeId = event.dataTransfer.getData(WI.DOMTreeOutline.DOMNodeIdDragType);
        if (domNodeId) {
            event.preventDefault();

            let domNode = WI.domManager.nodeForId(domNodeId);
            WI.RemoteObject.resolveNode(domNode, WI.RuntimeManager.ConsoleObjectGroup)
            .then((remoteObject) => {
                let text = domNode.nodeType() === Node.ELEMENT_NODE ? WI.UIString("Dropped Element") : WI.UIString("Dropped Node");
                const addSpecialUserLogClass = true;
                WI.consoleLogViewController.appendImmediateExecutionWithResult(text, remoteObject, addSpecialUserLogClass);

                this.prompt.focus();
            });
        }
    }

    _handleConsoleSavedResultAliasSettingChanged()
    {
        this._updateActiveExecutionContextDisplay();
    }

    _handleEngineeringShowInternalExecutionContextsSettingChanged(event)
    {
        if (WI.runtimeManager.activeExecutionContext.type !== WI.ExecutionContext.Type.Internal)
            return;

        this._useExecutionContextOfInspectedNode = InspectorBackend.hasDomain("DOM");
        this._setActiveExecutionContext(this._resolveDesiredActiveExecutionContext());
    }

    _handleFramePageExecutionContextChanged(event)
    {
        if (this._restoreSelectedExecutionContextForFrame !== event.target)
            return;

        this._restoreSelectedExecutionContextForFrame = null;

        let {context} = event.data;

        this._useExecutionContextOfInspectedNode = false;
        this._setActiveExecutionContext(context);
    }

    _handleFrameExecutionContextsCleared(event)
    {
        let {committingProvisionalLoad, contexts} = event.data;

        let hasActiveExecutionContext = contexts.some((context) => context === WI.runtimeManager.activeExecutionContext);
        if (!hasActiveExecutionContext)
            return;

        // If this frame is navigating and it is selected in the UI we want to reselect its new item after navigation.
        if (committingProvisionalLoad && !this._restoreSelectedExecutionContextForFrame) {
            this._restoreSelectedExecutionContextForFrame = event.target;

            // As a fail safe, if the frame never gets an execution context, clear the restore value.
            setTimeout(() => {
                this._restoreSelectedExecutionContextForFrame = null;
            }, 10);
            return;
        }

        this._useExecutionContextOfInspectedNode = InspectorBackend.hasDomain("DOM");
        this._setActiveExecutionContext(this._resolveDesiredActiveExecutionContext());
    }

    _handleDebuggerActiveCallFrameDidChange(event)
    {
        this._updateActiveExecutionContextDisplay();
    }

    _handleActiveExecutionContextChanged(event)
    {
        this._updateActiveExecutionContextDisplay();
    }

    _handleTransitionPageTarget()
    {
        this._updateActiveExecutionContextDisplay();
    }

    _handleTargetRemoved(event)
    {
        let {target} = event.data;
        if (target !== WI.runtimeManager.activeExecutionContext)
            return;

        this._useExecutionContextOfInspectedNode = InspectorBackend.hasDomain("DOM");
        this._setActiveExecutionContext(this._resolveDesiredActiveExecutionContext());
    }

    _handleInspectedNodeChanged(event)
    {
        if (!this._useExecutionContextOfInspectedNode)
            return;

        this._setActiveExecutionContext(this._resolveDesiredActiveExecutionContext());
    }

    _toggleOrFocus(event)
    {
        if (this._keyboardShortcutDisabled)
            return;

        if (this.prompt.focused) {
            WI.toggleSplitConsole();
            event.preventDefault();
        } else if (!WI.isEditingAnyField() && !WI.isEventTargetAnEditableField(event)) {
            this.prompt.focus();
            event.preventDefault();
        }
    }

    _updateStyles()
    {
        this.element.classList.toggle("showing-log", WI.isShowingConsoleTab() || WI.isShowingSplitConsole());
    }
};
