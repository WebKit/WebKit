/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

WI.WorkerManager = class WorkerManager extends WI.Object
{
    constructor()
    {
        super();

        this._connections = new Map;
    }

    // Target

    initializeTarget(target)
    {
        if (target.WorkerAgent)
            target.WorkerAgent.enable();
    }

    // WorkerObserver

    workerCreated(workerId, url)
    {
        let connection = new InspectorBackend.WorkerConnection(workerId);
        let workerTarget = new WI.WorkerTarget(workerId, url, connection);
        workerTarget.initialize();

        WI.targetManager.addTarget(workerTarget);

        this._connections.set(workerId, connection);

        // Unpause the worker now that we have sent all initialization messages.
        // Ignore errors if a worker went away quickly.
        WorkerAgent.initialized(workerId).catch(function(){});
    }

    workerTerminated(workerId)
    {
        let connection = this._connections.take(workerId);

        WI.targetManager.removeTarget(connection.target);
    }

    dispatchMessageFromWorker(workerId, message)
    {
        let connection = this._connections.get(workerId);

        console.assert(connection);
        if (!connection)
            return;

        connection.dispatch(message);
    }
};
