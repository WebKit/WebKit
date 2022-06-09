/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

WI.ConsoleObserver = class ConsoleObserver extends InspectorBackend.Dispatcher
{
    // Events defined by the "Console" domain.

    messageAdded(message)
    {
        if (message.source === "console-api" && message.type === "clear")
            return;

        if (message.type === "assert" && !message.text)
            message.text = WI.UIString("Assertion");

        WI.consoleManager.messageWasAdded(this._target, message.source, message.level, message.text, message.type, message.url, message.line, message.column || 0, message.repeatCount, message.parameters, message.stackTrace, message.networkRequestId);
    }

    messageRepeatCountUpdated(count)
    {
        WI.consoleManager.messageRepeatCountUpdated(count);
    }

    messagesCleared()
    {
        WI.consoleManager.messagesCleared();
    }

    heapSnapshot(timestamp, snapshotStringData, title)
    {
        let workerProxy = WI.HeapSnapshotWorkerProxy.singleton();
        workerProxy.createSnapshot(snapshotStringData, title || null, ({objectId, snapshot: serializedSnapshot}) => {
            let snapshot = WI.HeapSnapshotProxy.deserialize(objectId, serializedSnapshot);
            snapshot.snapshotStringData = snapshotStringData;
            WI.timelineManager.heapSnapshotAdded(timestamp, snapshot);
        });
    }
};
