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

WI.Breakpoint = class Breakpoint extends WI.Object
{
    constructor(sourceCodeLocation, {contentIdentifier, disabled, condition, ignoreCount, autoContinue} = {})
    {
        console.assert(sourceCodeLocation instanceof WI.SourceCodeLocation);
        console.assert(!contentIdentifier || typeof contentIdentifier === "string");
        console.assert(!disabled || typeof disabled === "boolean");
        console.assert(!condition || typeof condition === "string");
        console.assert(!ignoreCount || !isNaN(ignoreCount));
        console.assert(!autoContinue || typeof autoContinue === "boolean");

        super();

        this._id = null;
        this._sourceCodeLocation = sourceCodeLocation;

        let sourceCode = this._sourceCodeLocation.sourceCode;
        if (sourceCode) {
            this._contentIdentifier = sourceCode.contentIdentifier;
            console.assert(!contentIdentifier || contentIdentifier === this._contentIdentifier, "The content identifier from the source code should match the given value.");
        } else
            this._contentIdentifier = contentIdentifier || null;
        console.assert(this._contentIdentifier || this._isSpecial(), "There should always be a content identifier for a breakpoint.");

        this._scriptIdentifier = sourceCode instanceof WI.Script ? sourceCode.id : null;
        this._target = sourceCode instanceof WI.Script ? sourceCode.target : null;
        this._disabled = disabled || false;
        this._condition = condition || "";
        this._ignoreCount = ignoreCount || 0;
        this._autoContinue = autoContinue || false;
        this._actions = [];
        this._resolved = false;

        this._sourceCodeLocation.addEventListener(WI.SourceCodeLocation.Event.LocationChanged, this._sourceCodeLocationLocationChanged, this);
        this._sourceCodeLocation.addEventListener(WI.SourceCodeLocation.Event.DisplayLocationChanged, this._sourceCodeLocationDisplayLocationChanged, this);
    }

    // Import / Export

    static fromJSON(json)
    {
        const sourceCode = null;
        let breakpoint = new Breakpoint(new WI.SourceCodeLocation(sourceCode, json.lineNumber || 0, json.columnNumber || 0), {
            // The 'url' fallback is for transitioning from older frontends and should be removed.
            contentIdentifier: json.contentIdentifier || json.url,
            disabled: json.disabled,
            condition: json.condition,
            ignoreCount: json.ignoreCount,
            autoContinue: json.autoContinue,
        });
        breakpoint._actions = json.actions.map((actionJSON) => WI.BreakpointAction.fromJSON(actionJSON, breakpoint));
        return breakpoint;
    }

    toJSON(key)
    {
        // The id, scriptIdentifier, target, and resolved state are tied to the current session, so don't include them for serialization.
        let json = {
            contentIdentifier: this._contentIdentifier,
            lineNumber: this._sourceCodeLocation.lineNumber,
            columnNumber: this._sourceCodeLocation.columnNumber,
            disabled: this._disabled,
            condition: this._condition,
            ignoreCount: this._ignoreCount,
            actions: this._actions.map((action) => action.toJSON()),
            autoContinue: this._autoContinue,
        };
        if (key === WI.ObjectStore.toJSONSymbol)
            json[WI.objectStores.breakpoints.keyPath] = this._contentIdentifier + ":" + this._sourceCodeLocation.lineNumber + ":" + this._sourceCodeLocation.columnNumber;
        return json;
    }

    // Public

    get sourceCodeLocation() { return this._sourceCodeLocation; }
    get contentIdentifier() { return this._contentIdentifier; }
    get scriptIdentifier() { return this._scriptIdentifier; }
    get target() { return this._target; }

    get identifier()
    {
        return this._id;
    }

    set identifier(id)
    {
        this._id = id || null;
    }

    get resolved()
    {
        return this._resolved;
    }

    set resolved(resolved)
    {
        if (this._resolved === resolved)
            return;

        console.assert(!resolved || this._sourceCodeLocation.sourceCode || this._isSpecial(), "Breakpoints must have a SourceCode to be resolved.", this);

        this._resolved = resolved || false;

        this.dispatchEventToListeners(WI.Breakpoint.Event.ResolvedStateDidChange);
    }

    get disabled()
    {
        return this._disabled;
    }

    set disabled(disabled)
    {
        if (this._disabled === disabled)
            return;

        this._disabled = disabled || false;

        this.dispatchEventToListeners(WI.Breakpoint.Event.DisabledStateDidChange);
    }

    get condition()
    {
        return this._condition;
    }

    set condition(condition)
    {
        if (this._condition === condition)
            return;

        this._condition = condition;

        this.dispatchEventToListeners(WI.Breakpoint.Event.ConditionDidChange);
    }

    get ignoreCount()
    {
        return this._ignoreCount;
    }

    set ignoreCount(ignoreCount)
    {
        console.assert(ignoreCount >= 0, "Ignore count cannot be negative.");
        if (ignoreCount < 0)
            return;

        if (this._ignoreCount === ignoreCount)
            return;

        this._ignoreCount = ignoreCount;

        this.dispatchEventToListeners(WI.Breakpoint.Event.IgnoreCountDidChange);
    }

    get autoContinue()
    {
        return this._autoContinue;
    }

    set autoContinue(cont)
    {
        if (this._autoContinue === cont)
            return;

        this._autoContinue = cont;

        this.dispatchEventToListeners(WI.Breakpoint.Event.AutoContinueDidChange);
    }

    get actions()
    {
        return this._actions;
    }

    get probeActions()
    {
        return this._actions.filter(function(action) {
            return action.type === WI.BreakpointAction.Type.Probe;
        });
    }

    cycleToNextMode()
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
    }

    createAction(type, precedingAction, data)
    {
        var newAction = new WI.BreakpointAction(this, type, data || null);

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

        this.dispatchEventToListeners(WI.Breakpoint.Event.ActionsDidChange);

        return newAction;
    }

    recreateAction(type, actionToReplace)
    {
        let index = this._actions.indexOf(actionToReplace);
        console.assert(index !== -1);
        if (index === -1)
            return null;

        const data = null;
        let action = new WI.BreakpointAction(this, type, data);
        this._actions[index] = action;

        this.dispatchEventToListeners(WI.Breakpoint.Event.ActionsDidChange);

        return action;
    }

    removeAction(action)
    {
        var index = this._actions.indexOf(action);
        console.assert(index !== -1);
        if (index === -1)
            return;

        this._actions.splice(index, 1);

        if (!this._actions.length)
            this.autoContinue = false;

        this.dispatchEventToListeners(WI.Breakpoint.Event.ActionsDidChange);
    }

    clearActions(type)
    {
        if (!type)
            this._actions = [];
        else
            this._actions = this._actions.filter(function(action) { return action.type !== type; });

        this.dispatchEventToListeners(WI.Breakpoint.Event.ActionsDidChange);
    }

    saveIdentityToCookie(cookie)
    {
        cookie["breakpoint-content-identifier"] = this._contentIdentifier;
        cookie["breakpoint-line-number"] = this._sourceCodeLocation.lineNumber;
        cookie["breakpoint-column-number"] = this._sourceCodeLocation.columnNumber;
    }

    // Protected (Called by BreakpointAction)

    breakpointActionDidChange(action)
    {
        var index = this._actions.indexOf(action);
        console.assert(index !== -1);
        if (index === -1)
            return;

        this.dispatchEventToListeners(WI.Breakpoint.Event.ActionsDidChange);
    }

    // Private

    _isSpecial()
    {
        return this._sourceCodeLocation.isEqual(new WI.SourceCodeLocation(null, Infinity, Infinity));
    }

    _sourceCodeLocationLocationChanged(event)
    {
        this.dispatchEventToListeners(WI.Breakpoint.Event.LocationDidChange, event.data);
    }

    _sourceCodeLocationDisplayLocationChanged(event)
    {
        this.dispatchEventToListeners(WI.Breakpoint.Event.DisplayLocationDidChange, event.data);
    }
};

WI.Breakpoint.TypeIdentifier = "breakpoint";

WI.Breakpoint.Event = {
    DisabledStateDidChange: "breakpoint-disabled-state-did-change",
    ResolvedStateDidChange: "breakpoint-resolved-state-did-change",
    ConditionDidChange: "breakpoint-condition-did-change",
    IgnoreCountDidChange: "breakpoint-ignore-count-did-change",
    ActionsDidChange: "breakpoint-actions-did-change",
    AutoContinueDidChange: "breakpoint-auto-continue-did-change",
    LocationDidChange: "breakpoint-location-did-change",
    DisplayLocationDidChange: "breakpoint-display-location-did-change",
};
