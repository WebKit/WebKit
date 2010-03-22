/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

/**
 * @fileoverview Injects "injected" object into the inspectable page.
 */


var InspectorControllerDispatcher = {};

/**
 * Main dispatch method, all calls from the host to InspectorController go
 * through this one.
 * @param {string} functionName Function to call
 * @param {string} json_args JSON-serialized call parameters.
 * @return {string} JSON-serialized result of the dispatched call.
 */
InspectorControllerDispatcher.dispatch = function(functionName, json_args)
{
    var params = JSON.parse(json_args);
    InspectorBackend[functionName].apply(InspectorBackend, params);
};

/**
 * Special controller object for APU related messages. Outgoing messages
 * are sent to this object if the ApuAgentDispatcher is enabled.
 **/
var ApuAgentDispatcher = { enabled : false };

/**
 * Dispatches messages to APU. This filters and transforms
 * outgoing messages that are used by APU.
 * @param {string} method name of the dispatch method.
 **/
ApuAgentDispatcher.dispatchToApu = function(method, args)
{
    if (method !== "addRecordToTimeline" && method !== "updateResource" && method !== "addResource")
        return;
    // TODO(knorton): Transform args so they can be used
    // by APU.
    DevToolsAgentHost.dispatchToApu(JSON.stringify(args));
};

/**
 * This is called by the InspectorFrontend for serialization.
 * We serialize the call and send it to the client over the IPC
 * using dispatchOut bound method.
 */
function dispatch(method, var_args) {
    // Handle all messages with non-primitieve arguments here.
    var args = Array.prototype.slice.call(arguments);

    if (method === "inspectedWindowCleared" || method === "reset" || method === "setAttachedWindow") {
        // Filter out messages we don't need here.
        // We do it on the sender side since they may have non-serializable
        // parameters.
        return;
    }

    // Sniff some inspector controller state changes in order to support
    // cross-navigation instrumentation. Keep names in sync with
    // webdevtoolsagent_impl.
    if (method === "timelineProfilerWasStarted")
        DevToolsAgentHost.runtimeFeatureStateChanged("timeline-profiler", true);
    else if (method === "timelineProfilerWasStopped")
        DevToolsAgentHost.runtimeFeatureStateChanged("timeline-profiler", false);
    else if (method === "resourceTrackingWasEnabled")
        DevToolsAgentHost.runtimeFeatureStateChanged("resource-tracking", true);
    else if (method === "resourceTrackingWasDisabled")
        DevToolsAgentHost.runtimeFeatureStateChanged("resource-tracking", false);

    if (ApuAgentDispatcher.enabled) {
        ApuAgentDispatcher.dispatchToApu(method, args);
        return;
    }

    var call = JSON.stringify(args);
    DevToolsAgentHost.dispatch(call);
};

function close() {
    // This method is called when InspectorFrontend closes in layout tests.
}

function inspectedPageDestroyed() {
}
