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

WebInspector.LogManager = class LogManager extends WebInspector.Object
{
    constructor()
    {
        super();

        this._clearMessagesRequested = false;
        this._isNewPageOrReload = false;

        WebInspector.Frame.addEventListener(WebInspector.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
    }

    // Public

    messageWasAdded(target, source, level, text, type, url, line, column, repeatCount, parameters, stackTrace, requestId)
    {
        // Called from WebInspector.ConsoleObserver.

        // FIXME: Get a request from request ID.

        if (parameters)
            parameters = parameters.map((x) => WebInspector.RemoteObject.fromPayload(x, target));

        let message = new WebInspector.ConsoleMessage(target, source, level, text, type, url, line, column, repeatCount, parameters, stackTrace, null);

        this.dispatchEventToListeners(WebInspector.LogManager.Event.MessageAdded, {message});

        if (message.level === "warning" || message.level === "error")
            WebInspector.issueManager.issueWasAdded(message);
    }

    messagesCleared()
    {
        // Called from WebInspector.ConsoleObserver.

        WebInspector.ConsoleCommandResultMessage.clearMaximumSavedResultIndex();

        if (this._clearMessagesRequested) {
            // Frontend requested "clear console" and Backend successfully completed the request.
            this._clearMessagesRequested = false;
            this.dispatchEventToListeners(WebInspector.LogManager.Event.Cleared);
        } else {
            // Received an unrequested clear console event.
            // This could be for a navigation or other reasons (like console.clear()).
            // If this was a reload, we may not want to dispatch WebInspector.LogManager.Event.Cleared.
            // To detect if this is a reload we wait a turn and check if there was a main resource change reload.
            setTimeout(this._delayedMessagesCleared.bind(this), 0);
        }
    }

    _delayedMessagesCleared()
    {
        if (this._isNewPageOrReload) {
            this._isNewPageOrReload = false;

            if (!WebInspector.settings.clearLogOnNavigate.value)
                return;
        }

        // A console.clear() or command line clear() happened.
        this.dispatchEventToListeners(WebInspector.LogManager.Event.Cleared);
    }

    messageRepeatCountUpdated(count)
    {
        // Called from WebInspector.ConsoleObserver.

        this.dispatchEventToListeners(WebInspector.LogManager.Event.PreviousMessageRepeatCountUpdated, {count});
    }

    requestClearMessages()
    {
        this._clearMessagesRequested = true;

        for (let target of WebInspector.targets)
            target.ConsoleAgent.clearMessages();
    }

    // Private

    _mainResourceDidChange(event)
    {
        console.assert(event.target instanceof WebInspector.Frame);

        if (!event.target.isMainFrame())
            return;

        this._isNewPageOrReload = true;

        let timestamp = Date.now();
        let wasReloaded = event.data.oldMainResource && event.data.oldMainResource.url === event.target.mainResource.url;
        this.dispatchEventToListeners(WebInspector.LogManager.Event.SessionStarted, {timestamp, wasReloaded});

        WebInspector.ConsoleCommandResultMessage.clearMaximumSavedResultIndex();
    }
};

WebInspector.LogManager.Event = {
    SessionStarted: "log-manager-session-was-started",
    Cleared: "log-manager-cleared",
    MessageAdded: "log-manager-message-added",
    PreviousMessageRepeatCountUpdated: "log-manager-previous-message-repeat-count-updated"
};
