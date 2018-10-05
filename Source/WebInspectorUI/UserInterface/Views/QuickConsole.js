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

        this._mainExecutionContextPathComponent = this._createExecutionContextPathComponent(WI.mainTarget.executionContext);

        this._otherExecutionContextPathComponents = [];
        this._frameToPathComponent = new Map;
        this._targetToPathComponent = new Map;

        this._restoreSelectedExecutionContextForFrame = false;

        this.element.classList.add("quick-console");
        this.element.addEventListener("mousedown", this._handleMouseDown.bind(this));

        this.prompt = new WI.ConsolePrompt(null, "text/javascript");
        this.prompt.element.classList.add("text-prompt");
        this.addSubview(this.prompt);

        // FIXME: CodeMirror 4 has a default "Esc" key handler that always prevents default.
        // Our keyboard shortcut above will respect the default prevented and ignore the event
        // and not toggle the console. Install our own Escape key handler that will trigger
        // when the ConsolePrompt is empty, to restore toggling behavior. A better solution
        // would be for CodeMirror's event handler to pass if it doesn't do anything.
        this.prompt.escapeKeyHandlerWhenEmpty = function() { WI.toggleSplitConsole(); };

        this._navigationBar = new WI.QuickConsoleNavigationBar;
        this.addSubview(this._navigationBar);

        this._executionContextSelectorItem = new WI.HierarchicalPathNavigationItem;
        this._executionContextSelectorItem.showSelectorArrows = true;
        this._navigationBar.addNavigationItem(this._executionContextSelectorItem);

        this._executionContextSelectorDivider = new WI.DividerNavigationItem;
        this._navigationBar.addNavigationItem(this._executionContextSelectorDivider);

        this._rebuildExecutionContextPathComponents();

        WI.consoleDrawer.toggleButtonShortcutTooltip(this._toggleOrFocusKeyboardShortcut);
        WI.consoleDrawer.addEventListener(WI.ConsoleDrawer.Event.CollapsedStateChanged, this._updateStyles, this);

        WI.Frame.addEventListener(WI.Frame.Event.PageExecutionContextChanged, this._framePageExecutionContextsChanged, this);
        WI.Frame.addEventListener(WI.Frame.Event.ExecutionContextsCleared, this._frameExecutionContextsCleared, this);

        WI.debuggerManager.addEventListener(WI.DebuggerManager.Event.ActiveCallFrameDidChange, this._debuggerActiveCallFrameDidChange, this);

        WI.runtimeManager.addEventListener(WI.RuntimeManager.Event.ActiveExecutionContextChanged, this._activeExecutionContextChanged, this);

        WI.targetManager.addEventListener(WI.TargetManager.Event.TargetAdded, this._targetAdded, this);
        WI.targetManager.addEventListener(WI.TargetManager.Event.TargetRemoved, this._targetRemoved, this);

        WI.TabBrowser.addEventListener(WI.TabBrowser.Event.SelectedTabContentViewDidChange, this._updateStyles, this);
    }

    // Public

    get navigationBar()
    {
        return this._navigationBar;
    }

    get selectedExecutionContext()
    {
        return WI.runtimeManager.activeExecutionContext;
    }

    set selectedExecutionContext(executionContext)
    {
        WI.runtimeManager.activeExecutionContext = executionContext;
    }

    closed()
    {
        WI.Frame.removeEventListener(null, null, this);
        WI.debuggerManager.removeEventListener(null, null, this);
        WI.runtimeManager.removeEventListener(null, null, this);
        WI.targetManager.removeEventListener(null, null, this);
        WI.consoleDrawer.removeEventListener(null, null, this);
        WI.TabBrowser.removeEventListener(null, null, this);

        super.closed();
    }

    // Protected

    layout()
    {
        // A hard maximum size of 33% of the window.
        let maximumAllowedHeight = Math.round(window.innerHeight * 0.33);
        this.prompt.element.style.maxHeight = maximumAllowedHeight + "px";
    }

    // Private

    _handleMouseDown(event)
    {
        if (event.target !== this.element)
            return;

        event.preventDefault();
        this.prompt.focus();
    }

    _executionContextPathComponentsToDisplay()
    {
        // If we are in the debugger the console will use the active call frame, don't show the selector.
        if (WI.debuggerManager.activeCallFrame)
            return [];

        // If there is only the Main ExecutionContext, don't show the selector.
        if (!this._otherExecutionContextPathComponents.length)
            return [];

        if (this.selectedExecutionContext === WI.mainTarget.executionContext)
            return [this._mainExecutionContextPathComponent];

        return this._otherExecutionContextPathComponents.filter((component) => component.representedObject === this.selectedExecutionContext);
    }

    _rebuildExecutionContextPathComponents()
    {
        let components = this._executionContextPathComponentsToDisplay();
        let isEmpty = !components.length;

        this._executionContextSelectorItem.components = components;

        this._executionContextSelectorItem.hidden = isEmpty;
        this._executionContextSelectorDivider.hidden = isEmpty;
    }

    _framePageExecutionContextsChanged(event)
    {
        let frame = event.target;

        let shouldAutomaticallySelect = this._restoreSelectedExecutionContextForFrame === frame;

        let newExecutionContextPathComponent = this._insertExecutionContextPathComponentForFrame(frame, shouldAutomaticallySelect);

        if (shouldAutomaticallySelect) {
            this._restoreSelectedExecutionContextForFrame = null;
            this.selectedExecutionContext = newExecutionContextPathComponent.representedObject;
        }
    }

    _frameExecutionContextsCleared(event)
    {
        let frame = event.target;

        // If this frame is navigating and it is selected in the UI we want to reselect its new item after navigation.
        if (event.data.committingProvisionalLoad && !this._restoreSelectedExecutionContextForFrame) {
            let executionContextPathComponent = this._frameToPathComponent.get(frame);
            if (executionContextPathComponent && executionContextPathComponent.representedObject === this.selectedExecutionContext) {
                this._restoreSelectedExecutionContextForFrame = frame;
                // As a fail safe, if the frame never gets an execution context, clear the restore value.
                setTimeout(() => { this._restoreSelectedExecutionContextForFrame = false; }, 10);
            }
        }

        this._removeExecutionContextPathComponentForFrame(frame);
    }

    _activeExecutionContextChanged(event)
    {
        this._rebuildExecutionContextPathComponents();

        this._executionContextSelectorItem.element.classList.toggle("non-default-execution-context", this.selectedExecutionContext !== WI.mainTarget.executionContext);
    }

    _createExecutionContextPathComponent(executionContext, preferredName)
    {
        console.assert(executionContext instanceof WI.ExecutionContext);

        let pathComponent = new WI.HierarchicalPathComponent(preferredName || executionContext.name, "execution-context", executionContext, true, true);
        pathComponent.addEventListener(WI.HierarchicalPathComponent.Event.SiblingWasSelected, this._pathComponentSelected, this);
        pathComponent.addEventListener(WI.HierarchicalPathComponent.Event.Clicked, this._pathComponentClicked, this);
        pathComponent.truncatedDisplayNameLength = 50;
        return pathComponent;
    }

    _createExecutionContextPathComponentFromFrame(frame)
    {
        let preferredName = frame.name ? WI.UIString("%s (%s)").format(frame.name, frame.mainResource.displayName) : frame.mainResource.displayName;
        return this._createExecutionContextPathComponent(frame.pageExecutionContext, preferredName);
    }

    _compareExecutionContextPathComponents(a, b)
    {
        let aExecutionContext = a.representedObject;
        let bExecutionContext = b.representedObject;

        // "Targets" (workers) at the top.
        let aNonMainTarget = aExecutionContext.target !== WI.mainTarget;
        let bNonMainTarget = bExecutionContext.target !== WI.mainTarget;
        if (aNonMainTarget && !bNonMainTarget)
            return -1;
        if (bNonMainTarget && !aNonMainTarget)
            return 1;
        if (aNonMainTarget && bNonMainTarget)
            return a.displayName.extendedLocaleCompare(b.displayName);

        // "Main Frame" follows.
        if (aExecutionContext === WI.mainTarget.executionContext)
            return -1;
        if (bExecutionContext === WI.mainTarget.executionContext)
            return 1;

        // Only Frame contexts remain.
        console.assert(aExecutionContext.frame);
        console.assert(bExecutionContext.frame);

        // Frames with a name above frames without a name.
        if (aExecutionContext.frame.name && !bExecutionContext.frame.name)
            return -1;
        if (!aExecutionContext.frame.name && bExecutionContext.frame.name)
            return 1;

        return a.displayName.extendedLocaleCompare(b.displayName);
    }

    _insertOtherExecutionContextPathComponent(executionContextPathComponent, skipRebuild)
    {
        let index = insertionIndexForObjectInListSortedByFunction(executionContextPathComponent, this._otherExecutionContextPathComponents, this._compareExecutionContextPathComponents);

        let prev = index > 0 ? this._otherExecutionContextPathComponents[index - 1] : this._mainExecutionContextPathComponent;
        let next = this._otherExecutionContextPathComponents[index] || null;
        if (prev) {
            prev.nextSibling = executionContextPathComponent;
            executionContextPathComponent.previousSibling = prev;
        }
        if (next) {
            next.previousSibling = executionContextPathComponent;
            executionContextPathComponent.nextSibling = next;
        }

        this._otherExecutionContextPathComponents.splice(index, 0, executionContextPathComponent);

        if (!skipRebuild)
            this._rebuildExecutionContextPathComponents();
    }

    _removeOtherExecutionContextPathComponent(executionContextPathComponent, skipRebuild)
    {
        executionContextPathComponent.removeEventListener(WI.HierarchicalPathComponent.Event.SiblingWasSelected, this._pathComponentSelected, this);
        executionContextPathComponent.removeEventListener(WI.HierarchicalPathComponent.Event.Clicked, this._pathComponentClicked, this);

        let prev = executionContextPathComponent.previousSibling;
        let next = executionContextPathComponent.nextSibling;
        if (prev)
            prev.nextSibling = next;
        if (next)
            next.previousSibling = prev;

        this._otherExecutionContextPathComponents.remove(executionContextPathComponent, true);

        if (!skipRebuild)
            this._rebuildExecutionContextPathComponents();
    }

    _insertExecutionContextPathComponentForFrame(frame, skipRebuild)
    {
        if (frame.isMainFrame())
            return this._mainExecutionContextPathComponent;

        let executionContextPathComponent = this._createExecutionContextPathComponentFromFrame(frame);
        this._insertOtherExecutionContextPathComponent(executionContextPathComponent, skipRebuild);
        this._frameToPathComponent.set(frame, executionContextPathComponent);

        return executionContextPathComponent;
    }

    _removeExecutionContextPathComponentForFrame(frame, skipRebuild)
    {
        if (frame.isMainFrame())
            return;

        let executionContextPathComponent = this._frameToPathComponent.take(frame);
        this._removeOtherExecutionContextPathComponent(executionContextPathComponent, skipRebuild);
    }

    _targetAdded(event)
    {
        let target = event.data.target;
        console.assert(target.type === WI.Target.Type.Worker);
        let preferredName = WI.UIString("Worker \u2014 %s").format(target.displayName);
        let executionContextPathComponent = this._createExecutionContextPathComponent(target.executionContext, preferredName);

        this._targetToPathComponent.set(target, executionContextPathComponent);
        this._insertOtherExecutionContextPathComponent(executionContextPathComponent);
    }

    _targetRemoved(event)
    {
        let target = event.data.target;
        let executionContextPathComponent = this._targetToPathComponent.take(target);
        this._removeOtherExecutionContextPathComponent(executionContextPathComponent);

        if (this.selectedExecutionContext === executionContextPathComponent.representedObject)
            this.selectedExecutionContext = WI.mainTarget.executionContext;
    }

    _pathComponentSelected(event)
    {
        let executionContext = event.data.pathComponent.representedObject;
        this.selectedExecutionContext = executionContext;
    }

    _pathComponentClicked(event)
    {
        this.prompt.focus();
    }

    _debuggerActiveCallFrameDidChange(event)
    {
        this._rebuildExecutionContextPathComponents();
    }

    _toggleOrFocus(event)
    {
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
