/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Tobias Reiss <tobi+webkit@basecode.de>
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

WI.ConsoleManager = class ConsoleManager extends WI.Object
{
    constructor()
    {
        super();

        this._warningCount = 0;
        this._errorCount = 0;
        this._issues = [];

        this._lastMessageLevel = null;
        this._clearMessagesRequested = false;
        this._isNewPageOrReload = false;
        this._remoteObjectsToRelease = null;

        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

        this._customLoggingChannels = [];
    }

    // Static

    static supportsLogChannels()
    {
        return InspectorBackend.hasCommand("Console.getLoggingChannels");
    }

    static issueMatchSourceCode(issue, sourceCode)
    {
        if (sourceCode instanceof WI.SourceMapResource)
            return issue.sourceCodeLocation && issue.sourceCodeLocation.displaySourceCode === sourceCode;
        if (sourceCode instanceof WI.Resource)
            return issue.url === sourceCode.url && (!issue.sourceCodeLocation || issue.sourceCodeLocation.sourceCode === sourceCode);
        if (sourceCode instanceof WI.Script)
            return issue.sourceCodeLocation && issue.sourceCodeLocation.sourceCode === sourceCode;
        return false;
    }

    // Public

    get warningCount() { return this._warningCount; }
    get errorCount() { return this._errorCount; }
    get customLoggingChannels() { return this._customLoggingChannels; }

    issuesForSourceCode(sourceCode)
    {
        var issues = [];

        for (var i = 0; i < this._issues.length; ++i) {
            var issue = this._issues[i];
            if (WI.ConsoleManager.issueMatchSourceCode(issue, sourceCode))
                issues.push(issue);
        }

        return issues;
    }

    releaseRemoteObjectWithConsoleClear(remoteObject)
    {
        if (!this._remoteObjectsToRelease)
            this._remoteObjectsToRelease = new Set;
        this._remoteObjectsToRelease.add(remoteObject);
    }

    // ConsoleObserver

    messageWasAdded(target, source, level, text, type, url, line, column, repeatCount, parameters, stackTrace, requestId)
    {
        // FIXME: Get a request from request ID.

        if (parameters)
            parameters = parameters.map((x) => WI.RemoteObject.fromPayload(x, target));

        let message = new WI.ConsoleMessage(target, source, level, text, type, url, line, column, repeatCount, parameters, stackTrace, null);

        this._incrementMessageLevelCount(message.level, message.repeatCount);

        this.dispatchEventToListeners(WI.ConsoleManager.Event.MessageAdded, {message});

        if (message.level === WI.ConsoleMessage.MessageLevel.Warning || message.level === WI.ConsoleMessage.MessageLevel.Error) {
            let issue = new WI.IssueMessage(message);
            this._issues.push(issue);

            this.dispatchEventToListeners(WI.ConsoleManager.Event.IssueAdded, {issue});
        }
    }

    messagesCleared()
    {
        if (this._remoteObjectsToRelease) {
            for (let remoteObject of this._remoteObjectsToRelease)
                remoteObject.release();
            this._remoteObjectsToRelease = null;
        }

        WI.ConsoleCommandResultMessage.clearMaximumSavedResultIndex();

        if (this._clearMessagesRequested) {
            // Frontend requested "clear console" and Backend successfully completed the request.
            this._clearMessagesRequested = false;

            this._warningCount = 0;
            this._errorCount = 0;
            this._issues = [];

            this._lastMessageLevel = null;

            this.dispatchEventToListeners(WI.ConsoleManager.Event.Cleared);
        } else {
            // Received an unrequested clear console event.
            // This could be for a navigation or other reasons (like console.clear()).
            // If this was a reload, we may not want to dispatch WI.ConsoleManager.Event.Cleared.
            // To detect if this is a reload we wait a turn and check if there was a main resource change reload.
            setTimeout(this._delayedMessagesCleared.bind(this), 0);
        }
    }

    messageRepeatCountUpdated(count)
    {
        this._incrementMessageLevelCount(this._lastMessageLevel, 1);

        this.dispatchEventToListeners(WI.ConsoleManager.Event.PreviousMessageRepeatCountUpdated, {count});
    }

    requestClearMessages()
    {
        this._clearMessagesRequested = true;

        for (let target of WI.targets)
            target.ConsoleAgent.clearMessages();
    }

    initializeLogChannels(target)
    {
        console.assert(target.hasDomain("Console"));

        if (!WI.ConsoleManager.supportsLogChannels())
            return;

        if (this._customLoggingChannels.length)
            return;

        target.ConsoleAgent.getLoggingChannels((error, channels) => {
            if (error)
                return;

            this._customLoggingChannels = channels.map(WI.LoggingChannel.fromPayload);
        });
    }

    // Private

    _incrementMessageLevelCount(level, count)
    {
        switch (level) {
        case WI.ConsoleMessage.MessageLevel.Warning:
            this._warningCount += count;
            break;
        case WI.ConsoleMessage.MessageLevel.Error:
            this._errorCount += count;
            break;
        }

        this._lastMessageLevel = level;
    }

    _delayedMessagesCleared()
    {
        if (this._isNewPageOrReload) {
            this._isNewPageOrReload = false;

            if (!WI.settings.clearLogOnNavigate.value)
                return;
        }

        this._warningCount = 0;
        this._errorCount = 0;
        this._issues = [];

        this._lastMessageLevel = null;

        // A console.clear() or command line clear() happened.
        this.dispatchEventToListeners(WI.ConsoleManager.Event.Cleared);
    }

    _mainResourceDidChange(event)
    {
        console.assert(event.target instanceof WI.Frame);

        if (!event.target.isMainFrame())
            return;

        this._isNewPageOrReload = true;

        let timestamp = Date.now();
        let wasReloaded = event.data.oldMainResource && event.data.oldMainResource.url === event.target.mainResource.url;
        this.dispatchEventToListeners(WI.ConsoleManager.Event.SessionStarted, {timestamp, wasReloaded});

        WI.ConsoleCommandResultMessage.clearMaximumSavedResultIndex();
    }
};

WI.ConsoleManager.Event = {
    SessionStarted: "console-manager-session-was-started",
    Cleared: "console-manager-cleared",
    MessageAdded: "console-manager-message-added",
    IssueAdded: "console-manager-issue-added",
    PreviousMessageRepeatCountUpdated: "console-manager-previous-message-repeat-count-updated",
};
