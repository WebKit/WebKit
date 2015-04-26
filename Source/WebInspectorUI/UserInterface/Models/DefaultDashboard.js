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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.DefaultDashboard = class DefaultDashboard extends WebInspector.Object
{
    constructor()
    {
        super();

        this._waitingForFirstMainResourceToStartTrackingSize = true;

        // Necessary events required to track load of resources.
        WebInspector.Frame.addEventListener(WebInspector.Frame.Event.ResourceWasAdded, this._resourceWasAdded, this);
        WebInspector.frameResourceManager.addEventListener(WebInspector.FrameResourceManager.Event.FrameWasAdded, this._frameWasAdded, this);

        // Necessary events required to track console messages.
        var logManager = WebInspector.logManager;
        logManager.addEventListener(WebInspector.LogManager.Event.Cleared, this._consoleWasCleared, this);
        logManager.addEventListener(WebInspector.LogManager.Event.ActiveLogCleared, this._consoleWasCleared, this);
        logManager.addEventListener(WebInspector.LogManager.Event.MessageAdded, this._consoleMessageAdded, this);
        logManager.addEventListener(WebInspector.LogManager.Event.PreviousMessageRepeatCountUpdated, this._consoleMessageWasRepeated, this);

        this._resourcesCount = 0;
        this._logs = 0;
        this._errors = 0;
        this._issues = 0;
    }

    // Public

    get resourcesCount()
    {
        return this._resourcesCount;
    }

    set resourcesCount(value)
    {
        this._resourcesCount = value;
        this._dataDidChange();
    }

    get logs()
    {
        return this._logs;
    }

    set logs(value)
    {
        this._logs = value;
        this._dataDidChange();
    }

    get errors()
    {
        return this._errors;
    }

    set errors(value)
    {
        this._errors = value;
        this._dataDidChange();
    }

    get issues()
    {
        return this._issues;
    }

    set issues(value)
    {
        this._issues = value;
        this._dataDidChange();
    }

    // Private

    _dataDidChange()
    {
        this.dispatchEventToListeners(WebInspector.DefaultDashboard.Event.DataDidChange);
    }

    _resourceWasAdded(event)
    {
        ++this.resourcesCount;
    }

    _frameWasAdded(event)
    {
        ++this.resourcesCount;
    }

    _resourceSizeDidChange(event)
    {
        this.resourcesSize += event.target.size - event.data.previousSize;
    }

    _consoleMessageAdded(event)
    {
        var message = event.data.message;
        this._lastConsoleMessageType = message.level;
        this._incrementConsoleMessageType(message.level, message.repeatCount);
    }

    _consoleMessageWasRepeated(event)
    {
        this._incrementConsoleMessageType(this._lastConsoleMessageType, 1);
    }

    _incrementConsoleMessageType(type, increment)
    {
        switch (type) {
        case WebInspector.ConsoleMessage.MessageLevel.Log:
            this.logs += increment;
            break;
        case WebInspector.ConsoleMessage.MessageLevel.Warning:
            this.issues += increment;
            break;
        case WebInspector.ConsoleMessage.MessageLevel.Error:
            this.errors += increment;
            break;
        }
    }

    _consoleWasCleared(event)
    {
        this._logs = 0;
        this._issues = 0;
        this._errors = 0;
        this._dataDidChange();
    }
};

WebInspector.DefaultDashboard.Event = {
    DataDidChange: "default-dashboard-data-did-change"
};
