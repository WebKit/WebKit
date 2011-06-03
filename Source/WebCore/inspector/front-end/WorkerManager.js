/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

WebInspector.WorkerManager = function()
{
    this._workerIdToWindow = {};
    InspectorBackend.registerDomainDispatcher("Worker", new WebInspector.WorkerMessageForwarder(this));
}

WebInspector.WorkerManager.isWorkerFrontend = function()
{
    return !!WebInspector.queryParamsObject["workerId"];
}

WebInspector.WorkerManager.loaded = function()
{
    var workerId = WebInspector.queryParamsObject["workerId"];
    if (!workerId) {
        WebInspector.workerManager = new WebInspector.WorkerManager();
        return;
    }

    function receiveMessage(event)
    {
        var message = event.data;
        InspectorBackend.dispatch(message);
    }
    window.addEventListener("message", receiveMessage, true);


    InspectorBackend.sendMessageObjectToBackend = function(message)
    {
        window.opener.postMessage({workerId: workerId, command: "sendMessageToBackend", message: message}, "*");
    }

    InspectorFrontendHost.loaded = function()
    {
        window.opener.postMessage({workerId: workerId, command: "loaded"}, "*");
    }
}

WebInspector.WorkerManager.Events = {
    WorkerAdded: "worker-added",
    WorkerRemoved: "worker-removed",
    WorkersCleared: "workers-cleared",
    WorkerInspectorClosed: "worker-inspector-closed"
}

WebInspector.WorkerManager.prototype = {
    _workerCreated: function(workerId, url, inspectorConnected)
     {
        if (inspectorConnected)
            this._openInspectorWindow(workerId);
        this.dispatchEventToListeners(WebInspector.WorkerManager.Events.WorkerAdded, {workerId: workerId, url: url, inspectorConnected: inspectorConnected});
     },

    _workerTerminated: function(workerId)
     {
        this.closeWorkerInspector(workerId);
        this.dispatchEventToListeners(WebInspector.WorkerManager.Events.WorkerRemoved, workerId);
     },

    _sendMessageToWorkerInspector: function(workerId, message)
    {
        var workerInspectorWindow = this._workerIdToWindow[workerId];
        if (workerInspectorWindow)
            workerInspectorWindow.postMessage(message, "*");
    },

    openWorkerInspector: function(workerId)
    {
        this._openInspectorWindow(workerId);
        WorkerAgent.connectToWorker(workerId);
    },

    _openInspectorWindow: function(workerId)
    {
        var url = location.href + "&workerId=" + workerId;
        url = url.replace("docked=true&", "");
        var workerInspectorWindow = window.open(url);
        this._workerIdToWindow[workerId] = workerInspectorWindow;
        workerInspectorWindow.addEventListener("beforeunload", this._workerInspectorClosing.bind(this, workerId), true);
    },

    closeWorkerInspector: function(workerId)
    {
        var workerInspectorWindow = this._workerIdToWindow[workerId];
        if (workerInspectorWindow)
            workerInspectorWindow.close();
    },

    reset: function()
    {
        for (var workerId in this._workerIdToWindow)
            this.closeWorkerInspector(workerId);
        this.dispatchEventToListeners(WebInspector.WorkerManager.Events.WorkersCleared);
    },

    _workerInspectorClosing: function(workerId, event)
    {
        delete this._workerIdToWindow[workerId];
        WorkerAgent.disconnectFromWorker(workerId);
        this.dispatchEventToListeners(WebInspector.WorkerManager.Events.WorkerInspectorClosed, workerId);
    }
}

WebInspector.WorkerManager.prototype.__proto__ = WebInspector.Object.prototype;

WebInspector.WorkerMessageForwarder = function(workerManager)
{
    this._workerManager = workerManager;
    window.addEventListener("message", this._receiveMessage.bind(this), true);
}

WebInspector.WorkerMessageForwarder.prototype = {
    _receiveMessage: function(event)
    {
        var workerId = event.data.workerId;
        workerId = parseInt(workerId);
        var command = event.data.command;
        var message = event.data.message;

        if (command == "sendMessageToBackend")
            WorkerAgent.sendMessageToWorker(workerId, message);
    },

    workerCreated: function(workerId, url, inspectorConnected)
    {
        this._workerManager._workerCreated(workerId, url, inspectorConnected);
    },

    workerTerminated: function(workerId)
    {
        this._workerManager._workerTerminated(workerId);
    },

    dispatchMessageFromWorker: function(workerId, message)
    {
        this._workerManager._sendMessageToWorkerInspector(workerId, message);
    }
}
