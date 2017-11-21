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

WI.LogManager = class LogManager extends WI.Object
{
    constructor()
    {
        super();

        this._clearMessagesRequested = false;
        this._isNewPageOrReload = false;

        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);

        this._customLoggingChannels = [];
        this._loggingChannelSources = [];

        if (WI.LogManager.supportsLogChannels()) {
            this._loggingChannelSources = [WI.ConsoleMessage.MessageSource.Media, WI.ConsoleMessage.MessageSource.WebRTC];
            ConsoleAgent.getLoggingChannels((error, channels) => {
                if (error)
                    return;

                for (let channel of channels) {
                    console.assert(this._loggingChannelSources.includes(channel.source));
                }

                this._customLoggingChannels = channels.map(WI.LoggingChannel.fromPayload);
            });
        }
    }

    // Static

    static supportsLogChannels()
    {
        return !!ConsoleAgent.getLoggingChannels;
    }

    // Public

    get customLoggingChannels() { return this._customLoggingChannels; }
    get logChannelSources() { return this._loggingChannelSources; }

    messageWasAdded(target, source, level, text, type, url, line, column, repeatCount, parameters, stackTrace, requestId)
    {
        // Called from WI.ConsoleObserver.

        // FIXME: Get a request from request ID.

        if (parameters)
            parameters = parameters.map((x) => WI.RemoteObject.fromPayload(x, target));

        let message = new WI.ConsoleMessage(target, source, level, text, type, url, line, column, repeatCount, parameters, stackTrace, null);

        this.dispatchEventToListeners(WI.LogManager.Event.MessageAdded, {message});

        if (message.level === "warning" || message.level === "error")
            WI.issueManager.issueWasAdded(message);
    }

    messagesCleared()
    {
        // Called from WI.ConsoleObserver.

        WI.ConsoleCommandResultMessage.clearMaximumSavedResultIndex();

        if (this._clearMessagesRequested) {
            // Frontend requested "clear console" and Backend successfully completed the request.
            this._clearMessagesRequested = false;
            this.dispatchEventToListeners(WI.LogManager.Event.Cleared);
        } else {
            // Received an unrequested clear console event.
            // This could be for a navigation or other reasons (like console.clear()).
            // If this was a reload, we may not want to dispatch WI.LogManager.Event.Cleared.
            // To detect if this is a reload we wait a turn and check if there was a main resource change reload.
            setTimeout(this._delayedMessagesCleared.bind(this), 0);
        }
    }

    _delayedMessagesCleared()
    {
        if (this._isNewPageOrReload) {
            this._isNewPageOrReload = false;

            if (!WI.settings.clearLogOnNavigate.value)
                return;
        }

        // A console.clear() or command line clear() happened.
        this.dispatchEventToListeners(WI.LogManager.Event.Cleared);
    }

    messageRepeatCountUpdated(count)
    {
        // Called from WI.ConsoleObserver.

        this.dispatchEventToListeners(WI.LogManager.Event.PreviousMessageRepeatCountUpdated, {count});
    }

    requestClearMessages()
    {
        this._clearMessagesRequested = true;

        for (let target of WI.targets)
            target.ConsoleAgent.clearMessages();
    }

    // Private

    _mainResourceDidChange(event)
    {
        console.assert(event.target instanceof WI.Frame);

        if (!event.target.isMainFrame())
            return;

        this._isNewPageOrReload = true;

        let timestamp = Date.now();
        let wasReloaded = event.data.oldMainResource && event.data.oldMainResource.url === event.target.mainResource.url;
        this.dispatchEventToListeners(WI.LogManager.Event.SessionStarted, {timestamp, wasReloaded});

        WI.ConsoleCommandResultMessage.clearMaximumSavedResultIndex();
    }
};

WI.LogManager.Event = {
    SessionStarted: "log-manager-session-was-started",
    Cleared: "log-manager-cleared",
    MessageAdded: "log-manager-message-added",
    PreviousMessageRepeatCountUpdated: "log-manager-previous-message-repeat-count-updated"
};
