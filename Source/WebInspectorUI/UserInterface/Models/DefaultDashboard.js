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

WI.DefaultDashboard = class DefaultDashboard extends WI.Object
{
    constructor()
    {
        super();

        this._waitingForFirstMainResourceToStartTrackingSize = true;

        // Necessary event required to track page load time and resource sizes.
        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
        WI.timelineManager.addEventListener(WI.TimelineManager.Event.CapturingStopped, this._capturingStopped, this);

        // Necessary events required to track load of resources.
        WI.Frame.addEventListener(WI.Frame.Event.ResourceWasAdded, this._resourceWasAdded, this);
        WI.Target.addEventListener(WI.Target.Event.ResourceAdded, this._resourceWasAdded, this);
        WI.networkManager.addEventListener(WI.NetworkManager.Event.FrameWasAdded, this._frameWasAdded, this);

        // Necessary events required to track console messages.
        WI.consoleManager.addEventListener(WI.ConsoleManager.Event.Cleared, this._consoleWasCleared, this);
        WI.consoleManager.addEventListener(WI.ConsoleManager.Event.MessageAdded, this._consoleMessageAdded, this);
        WI.consoleManager.addEventListener(WI.ConsoleManager.Event.PreviousMessageRepeatCountUpdated, this._consoleMessageWasRepeated, this);

        this._resourcesCount = 0;
        this._resourcesSize = 0;
        this._time = 0;
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

    get resourcesSize()
    {
        return this._resourcesSize;
    }

    set resourcesSize(value)
    {
        this._resourcesSize = value;
        this._dataDidChange();
    }

    get time()
    {
        return this._time;
    }

    set time(value)
    {
        this._time = value;
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
        this.dispatchEventToListeners(WI.DefaultDashboard.Event.DataDidChange);
    }

    _mainResourceDidChange(event)
    {
        console.assert(event.target instanceof WI.Frame);

        if (!event.target.isMainFrame())
            return;

        this._time = 0;
        this._resourcesCount = 1;
        this._resourcesSize = WI.networkManager.mainFrame.mainResource.size || 0;

        // We should only track resource sizes on fresh loads.
        if (this._waitingForFirstMainResourceToStartTrackingSize) {
            this._waitingForFirstMainResourceToStartTrackingSize = false;
            WI.Resource.addEventListener(WI.Resource.Event.SizeDidChange, this._resourceSizeDidChange, this);
        }

        this._dataDidChange();
        this._startUpdatingTime();
    }

    _capturingStopped(event)
    {
        // If recording stops, we should stop the timer if it hasn't stopped already.
        this._stopUpdatingTime();
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
        if (event.target.urlComponents.scheme === "data")
            return;
        this.resourcesSize += event.target.size - event.data.previousSize;
    }

    _startUpdatingTime()
    {
        this._stopUpdatingTime();

        this.time = 0;

        this._timelineBaseTime = Date.now();
        this._timeIntervalDelay = 50;
        this._timeIntervalIdentifier = setInterval(this._updateTime.bind(this), this._timeIntervalDelay);
    }

    _stopUpdatingTime()
    {
        if (!this._timeIntervalIdentifier)
            return;

        clearInterval(this._timeIntervalIdentifier);
        this._timeIntervalIdentifier = undefined;
    }

    _updateTime()
    {
        var duration = Date.now() - this._timelineBaseTime;

        var timeIntervalDelay = this._timeIntervalDelay;
        if (duration >= 1000) // 1 second
            timeIntervalDelay = 100;
        else if (duration >= 60000) // 60 seconds
            timeIntervalDelay = 1000;
        else if (duration >= 3600000) // 1 minute
            timeIntervalDelay = 10000;

        if (timeIntervalDelay !== this._timeIntervalDelay) {
            this._timeIntervalDelay = timeIntervalDelay;

            clearInterval(this._timeIntervalIdentifier);
            this._timeIntervalIdentifier = setInterval(this._updateTime.bind(this), this._timeIntervalDelay);
        }

        var mainFrame = WI.networkManager.mainFrame;
        var mainFrameStartTime = mainFrame.mainResource.firstTimestamp;
        var mainFrameLoadEventTime = mainFrame.loadEventTimestamp;

        if (isNaN(mainFrameStartTime) || isNaN(mainFrameLoadEventTime)) {
            this.time = duration / 1000;
            return;
        }

        this.time = mainFrameLoadEventTime - mainFrameStartTime;

        this._stopUpdatingTime();
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
        case WI.ConsoleMessage.MessageLevel.Log:
        case WI.ConsoleMessage.MessageLevel.Info:
        case WI.ConsoleMessage.MessageLevel.Debug:
            this.logs += increment;
            break;
        case WI.ConsoleMessage.MessageLevel.Warning:
            this.issues += increment;
            break;
        case WI.ConsoleMessage.MessageLevel.Error:
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

WI.DefaultDashboard.Event = {
    DataDidChange: "default-dashboard-data-did-change"
};
