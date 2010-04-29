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
 * @fileoverview Provides communication interface to remote v8 debugger. See
 * protocol decription at http://code.google.com/p/v8/wiki/DebuggerProtocol
 */

/**
 * FIXME: change field naming style to use trailing underscore.
 * @constructor
 */
devtools.DebuggerAgent = function()
{
    RemoteDebuggerAgent.debuggerOutput = this.handleDebuggerOutput_.bind(this);
    RemoteDebuggerAgent.setContextId = this.setContextId_.bind(this);

    /**
     * Id of the inspected page global context. It is used for filtering scripts.
     * @type {number}
     */
    this.contextId_ = null;

    /**
     * Mapping from script id to script info.
     * @type {Object}
     */
    this.parsedScripts_ = null;

    /**
     * Mapping from the request id to the devtools.BreakpointInfo for the
     * breakpoints whose v8 ids are not set yet. These breakpoints are waiting for
     * "setbreakpoint" responses to learn their ids in the v8 debugger.
     * @see #handleSetBreakpointResponse_
     * @type {Object}
     */
    this.requestNumberToBreakpointInfo_ = null;

    /**
     * Information on current stack frames.
     * @type {Array.<devtools.CallFrame>}
     */
    this.callFrames_ = [];

    /**
     * Whether to stop in the debugger on the exceptions.
     * @type {boolean}
     */
    this.pauseOnExceptions_ = false;

    /**
     * Mapping: request sequence number->callback.
     * @type {Object}
     */
    this.requestSeqToCallback_ = null;

    /**
     * Whether the scripts panel has been shown and initialilzed.
     * @type {boolean}
     */
    this.scriptsPanelInitialized_ = false;

    /**
     * Whether the scripts list should be requested next time when context id is
     * set.
     * @type {boolean}
     */
    this.requestScriptsWhenContextIdSet_ = false;

    /**
     * Whether the agent is waiting for initial scripts response.
     * @type {boolean}
     */
    this.waitingForInitialScriptsResponse_ = false;

    /**
     * If backtrace response is received when initial scripts response
     * is not yet processed the backtrace handling will be postponed until
     * after the scripts response processing. The handler bound to its arguments
     * and this agent will be stored in this field then.
     * @type {?function()}
     */
    this.pendingBacktraceResponseHandler_ = null;

    /**
     * Container of all breakpoints set using resource URL. These breakpoints
     * survive page reload. Breakpoints set by script id(for scripts that don't
     * have URLs) are stored in ScriptInfo objects.
     * @type {Object}
     */
    this.urlToBreakpoints_ = {};

    /**
     * Exception message that is shown to user while on exception break.
     * @type {WebInspector.ConsoleMessage}
     */
    this.currentExceptionMessage_ = null;

    /**
     * Whether breakpoints should suspend execution.
     * @type {boolean}
     */
    this.breakpointsActivated_ = true;
};


/**
 * A copy of the scope types from v8/src/mirror-delay.js
 * @enum {number}
 */
devtools.DebuggerAgent.ScopeType = {
    Global: 0,
    Local: 1,
    With: 2,
    Closure: 3,
    Catch: 4
};


/**
 * Resets debugger agent to its initial state.
 */
devtools.DebuggerAgent.prototype.reset = function()
{
    this.contextId_ = null;
    // No need to request scripts since they all will be pushed in AfterCompile
    // events.
    this.requestScriptsWhenContextIdSet_ = false;
    this.waitingForInitialScriptsResponse_ = false;

    this.parsedScripts_ = {};
    this.requestNumberToBreakpointInfo_ = {};
    this.callFrames_ = [];
    this.requestSeqToCallback_ = {};
};


/**
 * Initializes scripts UI. This method is called every time Scripts panel
 * is shown. It will send request for context id if it's not set yet.
 */
devtools.DebuggerAgent.prototype.initUI = function()
{
    // Initialize scripts cache when Scripts panel is shown first time.
    if (this.scriptsPanelInitialized_)
        return;
    this.scriptsPanelInitialized_ = true;
    if (this.contextId_) {
        // We already have context id. This means that we are here from the
        // very beginning of the page load cycle and hence will get all scripts
        // via after-compile events. No need to request scripts for this session.
        //
        // There can be a number of scripts from after-compile events that are
        // pending addition into the UI.
        for (var scriptId in this.parsedScripts_) {
          var script = this.parsedScripts_[scriptId];
          WebInspector.parsedScriptSource(scriptId, script.getUrl(), undefined /* script source */, script.getLineOffset() + 1);
          this.restoreBreakpoints_(scriptId, script.getUrl());
        }
        return;
    }
    this.waitingForInitialScriptsResponse_ = true;
    // Script list should be requested only when current context id is known.
    RemoteDebuggerAgent.getContextId();
    this.requestScriptsWhenContextIdSet_ = true;
};


/**
 * Asynchronously requests the debugger for the script source.
 * @param {number} scriptId Id of the script whose source should be resolved.
 * @param {function(source:?string):void} callback Function that will be called
 *     when the source resolution is completed. "source" parameter will be null
 *     if the resolution fails.
 */
devtools.DebuggerAgent.prototype.resolveScriptSource = function(scriptId, callback)
{
    var script = this.parsedScripts_[scriptId];
    if (!script || script.isUnresolved()) {
        callback(null);
        return;
    }

    var cmd = new devtools.DebugCommand("scripts", {
        "ids": [scriptId],
        "includeSource": true
    });
    devtools.DebuggerAgent.sendCommand_(cmd);
    // Force v8 execution so that it gets to processing the requested command.
    RemoteDebuggerAgent.processDebugCommands();

    var self = this;
    this.requestSeqToCallback_[cmd.getSequenceNumber()] = function(msg) {
        if (msg.isSuccess()) {
            var scriptJson = msg.getBody()[0];
            if (scriptJson) {
                script.source = scriptJson.source;
                callback(scriptJson.source);
            }
            else
                callback(null);
        } else
            callback(null);
    };
};


/**
 * Tells the v8 debugger to stop on as soon as possible.
 */
devtools.DebuggerAgent.prototype.pauseExecution = function()
{
    RemoteDebuggerCommandExecutor.DebuggerPauseScript();
};


/**
 * @param {number} sourceId Id of the script fot the breakpoint.
 * @param {number} line Number of the line for the breakpoint.
 * @param {?string} condition The breakpoint condition.
 */
devtools.DebuggerAgent.prototype.addBreakpoint = function(sourceId, line, enabled, condition)
{
    var script = this.parsedScripts_[sourceId];
    if (!script)
        return;

    line = devtools.DebuggerAgent.webkitToV8LineNumber_(line);

    var commandArguments;
    if (script.getUrl()) {
        var breakpoints = this.urlToBreakpoints_[script.getUrl()];
        if (breakpoints && breakpoints[line])
            return;
        if (!breakpoints) {
            breakpoints = {};
            this.urlToBreakpoints_[script.getUrl()] = breakpoints;
        }

        var breakpointInfo = new devtools.BreakpointInfo(line, enabled, condition);
        breakpoints[line] = breakpointInfo;

        commandArguments = {
            "groupId": this.contextId_,
            "type": "script",
            "target": script.getUrl(),
            "line": line,
            "condition": condition
        };
    } else {
        var breakpointInfo = script.getBreakpointInfo(line);
        if (breakpointInfo)
            return;

        breakpointInfo = new devtools.BreakpointInfo(line, enabled, condition);
        script.addBreakpointInfo(breakpointInfo);

        commandArguments = {
            "groupId": this.contextId_,
            "type": "scriptId",
            "target": sourceId,
            "line": line,
            "condition": condition
        };
    }

    if (!enabled)
        return;

    var cmd = new devtools.DebugCommand("setbreakpoint", commandArguments);

    this.requestNumberToBreakpointInfo_[cmd.getSequenceNumber()] = breakpointInfo;

    devtools.DebuggerAgent.sendCommand_(cmd);
    // Force v8 execution so that it gets to processing the requested command.
    // It is necessary for being able to change a breakpoint just after it
    // has been created (since we need an existing breakpoint id for that).
    RemoteDebuggerAgent.processDebugCommands();
};


/**
 * Changes given line of the script. 
 */
devtools.DebuggerAgent.prototype.editScriptSource = function(sourceId, newContent, callback)
{
    var commandArguments = {
        "script_id": sourceId,
        "new_source": newContent
    };

    var cmd = new devtools.DebugCommand("changelive", commandArguments);
    devtools.DebuggerAgent.sendCommand_(cmd);
    this.requestSeqToCallback_[cmd.getSequenceNumber()] = function(msg) {
        if (!msg.isSuccess())
            WebInspector.log("Unable to modify source code within given scope. Only function bodies are editable at the moment.", WebInspector.ConsoleMessage.MessageLevel.Warning);
        this.resolveScriptSource(sourceId, callback);
        if (WebInspector.panels.scripts.paused)
            this.requestBacktrace_();
    }.bind(this);
    RemoteDebuggerAgent.processDebugCommands();
};


/**
 * @param {number} sourceId Id of the script for the breakpoint.
 * @param {number} line Number of the line for the breakpoint.
 */
devtools.DebuggerAgent.prototype.removeBreakpoint = function(sourceId, line)
{
    var script = this.parsedScripts_[sourceId];
    if (!script)
        return;

    line = devtools.DebuggerAgent.webkitToV8LineNumber_(line);

    var breakpointInfo;
    if (script.getUrl()) {
        var breakpoints = this.urlToBreakpoints_[script.getUrl()];
        if (!breakpoints)
            return;
        breakpointInfo = breakpoints[line];
        delete breakpoints[line];
    } else {
        breakpointInfo = script.getBreakpointInfo(line);
        if (breakpointInfo)
            script.removeBreakpointInfo(breakpointInfo);
    }

    if (!breakpointInfo)
        return;

    breakpointInfo.markAsRemoved();

    var id = breakpointInfo.getV8Id();

    // If we don't know id of this breakpoint in the v8 debugger we cannot send
    // "clearbreakpoint" request. In that case it will be removed in
    // "setbreakpoint" response handler when we learn the id.
    if (id !== -1) {
        this.requestClearBreakpoint_(id);
    }
};


/**
 * @param {boolean} activated Whether breakpoints should be activated.
 */
devtools.DebuggerAgent.prototype.setBreakpointsActivated = function(activated)
{
    this.breakpointsActivated_ = activated;
};


/**
 * Tells the v8 debugger to step into the next statement.
 */
devtools.DebuggerAgent.prototype.stepIntoStatement = function()
{
    this.stepCommand_("in");
};


/**
 * Tells the v8 debugger to step out of current function.
 */
devtools.DebuggerAgent.prototype.stepOutOfFunction = function()
{
    this.stepCommand_("out");
};


/**
 * Tells the v8 debugger to step over the next statement.
 */
devtools.DebuggerAgent.prototype.stepOverStatement = function()
{
    this.stepCommand_("next");
};


/**
 * Tells the v8 debugger to continue execution after it has been stopped on a
 * breakpoint or an exception.
 */
devtools.DebuggerAgent.prototype.resumeExecution = function()
{
    this.clearExceptionMessage_();
    var cmd = new devtools.DebugCommand("continue");
    devtools.DebuggerAgent.sendCommand_(cmd);
};


/**
 * Creates exception message and schedules it for addition to the resource upon
 * backtrace availability.
 * @param {string} url Resource url.
 * @param {number} line Resource line number.
 * @param {string} message Exception text.
 */
devtools.DebuggerAgent.prototype.createExceptionMessage_ = function(url, line, message)
{
    this.currentExceptionMessage_ = new WebInspector.ConsoleMessage(
        WebInspector.ConsoleMessage.MessageSource.JS,
        WebInspector.ConsoleMessage.MessageType.Log,
        WebInspector.ConsoleMessage.MessageLevel.Error,
        line,
        url,
        0 /* group level */,
        1 /* repeat count */,
        "[Exception] " + message);
};


/**
 * Shows pending exception message that is created with createExceptionMessage_
 * earlier.
 */
devtools.DebuggerAgent.prototype.showPendingExceptionMessage_ = function()
{
    if (!this.currentExceptionMessage_)
        return;
    var msg = this.currentExceptionMessage_;
    var resource = WebInspector.resourceURLMap[msg.url];
    if (resource) {
        msg.resource = resource;
        WebInspector.panels.resources.addMessageToResource(resource, msg);
    } else
        this.currentExceptionMessage_ = null;
};


/**
 * Clears exception message from the resource.
 */
devtools.DebuggerAgent.prototype.clearExceptionMessage_ = function()
{
    if (this.currentExceptionMessage_) {
        var messageElement = this.currentExceptionMessage_._resourceMessageLineElement;
        var bubble = messageElement.parentElement;
        bubble.removeChild(messageElement);
        if (!bubble.firstChild) {
            // Last message in bubble removed.
            bubble.parentElement.removeChild(bubble);
        }
        this.currentExceptionMessage_ = null;
    }
};


/**
 * @return {boolean} True iff the debugger will pause execution on the
 * exceptions.
 */
devtools.DebuggerAgent.prototype.pauseOnExceptions = function()
{
    return this.pauseOnExceptions_;
};


/**
 * Tells whether to pause in the debugger on the exceptions or not.
 * @param {boolean} value True iff execution should be stopped in the debugger
 * on the exceptions.
 */
devtools.DebuggerAgent.prototype.setPauseOnExceptions = function(value)
{
    this.pauseOnExceptions_ = value;
};


/**
 * Sends "evaluate" request to the debugger.
 * @param {Object} arguments Request arguments map.
 * @param {function(devtools.DebuggerMessage)} callback Callback to be called
 *     when response is received.
 */
devtools.DebuggerAgent.prototype.requestEvaluate = function(arguments, callback)
{
    var cmd = new devtools.DebugCommand("evaluate", arguments);
    devtools.DebuggerAgent.sendCommand_(cmd);
    this.requestSeqToCallback_[cmd.getSequenceNumber()] = callback;
};


/**
 * Sends "lookup" request for each unresolved property of the object. When
 * response is received the properties will be changed with their resolved
 * values.
 * @param {Object} object Object whose properties should be resolved.
 * @param {function(devtools.DebuggerMessage)} Callback to be called when all
 *     children are resolved.
 * @param {boolean} noIntrinsic Whether intrinsic properties should be included.
 */
devtools.DebuggerAgent.prototype.resolveChildren = function(object, callback, noIntrinsic)
{
    if ("handle" in object) {
        var result = [];
        devtools.DebuggerAgent.formatObjectProperties_(object, result, noIntrinsic);
        callback(result);
    } else {
        this.requestLookup_([object.ref], function(msg) {
            var result = [];
            if (msg.isSuccess()) {
                var handleToObject = msg.getBody();
                var resolved = handleToObject[object.ref];
                devtools.DebuggerAgent.formatObjectProperties_(resolved, result, noIntrinsic);
                callback(result);
            } else
                callback([]);
        });
    }
};


/**
 * Sends "scope" request for the scope object to resolve its variables.
 * @param {Object} scope Scope to be resolved.
 * @param {function(Array.<WebInspector.ObjectPropertyProxy>)} callback
 *     Callback to be called when all scope variables are resolved.
 */
devtools.DebuggerAgent.prototype.resolveScope = function(scope, callback)
{
    var cmd = new devtools.DebugCommand("scope", {
        "frameNumber": scope.frameNumber,
        "number": scope.index,
        "compactFormat": true
    });
    devtools.DebuggerAgent.sendCommand_(cmd);
    this.requestSeqToCallback_[cmd.getSequenceNumber()] = function(msg) {
        var result = [];
        if (msg.isSuccess()) {
            var scopeObjectJson = msg.getBody().object;
            devtools.DebuggerAgent.formatObjectProperties_(scopeObjectJson, result, true /* no intrinsic */);
        }
        callback(result);
    };
};


/**
 * Sends "scopes" request for the frame object to resolve all variables
 * available in the frame.
 * @param {number} callFrameId Id of call frame whose variables need to
 *     be resolved.
 * @param {function(Object)} callback Callback to be called when all frame
 *     variables are resolved.
 */
devtools.DebuggerAgent.prototype.resolveFrameVariables_ = function(callFrameId, callback)
{
    var result = {};

    var frame = this.callFrames_[callFrameId];
    if (!frame) {
        callback(result);
        return;
    }

    var waitingResponses = 0;
    function scopeResponseHandler(msg) {
        waitingResponses--;

        if (msg.isSuccess()) {
            var properties = msg.getBody().object.properties;
            for (var j = 0; j < properties.length; j++)
                result[properties[j].name] = true;
        }

        // When all scopes are resolved invoke the callback.
        if (waitingResponses === 0)
            callback(result);
    };

    for (var i = 0; i < frame.scopeChain.length; i++) {
        var scope = frame.scopeChain[i].objectId;
        if (scope.type === devtools.DebuggerAgent.ScopeType.Global) {
            // Do not resolve global scope since it takes for too long.
            // TODO(yurys): allow to send only property names in the response.
            continue;
        }
        var cmd = new devtools.DebugCommand("scope", {
            "frameNumber": scope.frameNumber,
            "number": scope.index,
            "compactFormat": true
        });
        devtools.DebuggerAgent.sendCommand_(cmd);
        this.requestSeqToCallback_[cmd.getSequenceNumber()] = scopeResponseHandler;
        waitingResponses++;
    }
};

/**
 * Evaluates the expressionString to an object in the call frame and reports
 * all its properties.
 * @param{string} expressionString Expression whose properties should be
 *     collected.
 * @param{number} callFrameId The frame id.
 * @param{function(Object result,bool isException)} reportCompletions Callback
 *     function.
 */
devtools.DebuggerAgent.prototype.resolveCompletionsOnFrame = function(expressionString, callFrameId, reportCompletions)
{
      if (expressionString) {
          expressionString = "var obj = " + expressionString +
              "; var names = {}; for (var n in obj) { names[n] = true; };" +
              "names;";
          this.evaluateInCallFrame(
              callFrameId,
              expressionString,
              function(result) {
                  var names = {};
                  if (!result.isException) {
                      var props = result.value.objectId.properties;
                      // Put all object properties into the map.
                      for (var i = 0; i < props.length; i++)
                          names[props[i].name] = true;
                  }
                  reportCompletions(names, result.isException);
              });
      } else {
          this.resolveFrameVariables_(callFrameId,
              function(result) {
                  reportCompletions(result, false /* isException */);
              });
      }
};


/**
 * @param{number} scriptId
 * @return {string} Type of the context of the script with specified id.
 */
devtools.DebuggerAgent.prototype.getScriptContextType = function(scriptId)
{
    return this.parsedScripts_[scriptId].getContextType();
};


/**
 * Removes specified breakpoint from the v8 debugger.
 * @param {number} breakpointId Id of the breakpoint in the v8 debugger.
 */
devtools.DebuggerAgent.prototype.requestClearBreakpoint_ = function(breakpointId)
{
    var cmd = new devtools.DebugCommand("clearbreakpoint", {
        "breakpoint": breakpointId
    });
    devtools.DebuggerAgent.sendCommand_(cmd);
};


/**
 * Sends "backtrace" request to v8.
 */
devtools.DebuggerAgent.prototype.requestBacktrace_ = function()
{
    var cmd = new devtools.DebugCommand("backtrace", {
        "compactFormat":true
    });
    devtools.DebuggerAgent.sendCommand_(cmd);
};


/**
 * Sends command to v8 debugger.
 * @param {devtools.DebugCommand} cmd Command to execute.
 */
devtools.DebuggerAgent.sendCommand_ = function(cmd)
{
    RemoteDebuggerCommandExecutor.DebuggerCommand(cmd.toJSONProtocol());
};


/**
 * Tells the v8 debugger to make the next execution step.
 * @param {string} action "in", "out" or "next" action.
 */
devtools.DebuggerAgent.prototype.stepCommand_ = function(action)
{
    this.clearExceptionMessage_();
    var cmd = new devtools.DebugCommand("continue", {
        "stepaction": action,
        "stepcount": 1
    });
    devtools.DebuggerAgent.sendCommand_(cmd);
};


/**
 * Sends "lookup" request to v8.
 * @param {number} handle Handle to the object to lookup.
 */
devtools.DebuggerAgent.prototype.requestLookup_ = function(handles, callback)
{
    var cmd = new devtools.DebugCommand("lookup", {
        "compactFormat":true,
        "handles": handles
    });
    devtools.DebuggerAgent.sendCommand_(cmd);
    this.requestSeqToCallback_[cmd.getSequenceNumber()] = callback;
};


/**
 * Sets debugger context id for scripts filtering.
 * @param {number} contextId Id of the inspected page global context.
 */
devtools.DebuggerAgent.prototype.setContextId_ = function(contextId)
{
    this.contextId_ = contextId;

    // If it's the first time context id is set request scripts list.
    if (this.requestScriptsWhenContextIdSet_) {
        this.requestScriptsWhenContextIdSet_ = false;
        var cmd = new devtools.DebugCommand("scripts", {
            "includeSource": false
        });
        devtools.DebuggerAgent.sendCommand_(cmd);
        // Force v8 execution so that it gets to processing the requested command.
        RemoteDebuggerAgent.processDebugCommands();

        var debuggerAgent = this;
        this.requestSeqToCallback_[cmd.getSequenceNumber()] = function(msg) {
            // Handle the response iff the context id hasn't changed since the request
            // was issued. Otherwise if the context id did change all up-to-date
            // scripts will be pushed in after compile events and there is no need to
            // handle the response.
            if (contextId === debuggerAgent.contextId_)
                debuggerAgent.handleScriptsResponse_(msg);

            // We received initial scripts response so flush the flag and
            // see if there is an unhandled backtrace response.
            debuggerAgent.waitingForInitialScriptsResponse_ = false;
            if (debuggerAgent.pendingBacktraceResponseHandler_) {
                debuggerAgent.pendingBacktraceResponseHandler_();
                debuggerAgent.pendingBacktraceResponseHandler_ = null;
            }
        };
    }
};


/**
 * Handles output sent by v8 debugger. The output is either asynchronous event
 * or response to a previously sent request.  See protocol definitioun for more
 * details on the output format.
 * @param {string} output
 */
devtools.DebuggerAgent.prototype.handleDebuggerOutput_ = function(output)
{
    var msg;
    try {
        msg = new devtools.DebuggerMessage(output);
    } catch(e) {
        debugPrint("Failed to handle debugger response:\n" + e);
        throw e;
    }

    if (msg.getType() === "event") {
        if (msg.getEvent() === "break")
            this.handleBreakEvent_(msg);
        else if (msg.getEvent() === "exception")
            this.handleExceptionEvent_(msg);
        else if (msg.getEvent() === "afterCompile")
            this.handleAfterCompileEvent_(msg);
    } else if (msg.getType() === "response") {
        if (msg.getCommand() === "scripts")
            this.invokeCallbackForResponse_(msg);
        else if (msg.getCommand() === "setbreakpoint")
            this.handleSetBreakpointResponse_(msg);
        else if (msg.getCommand() === "changelive")
            this.invokeCallbackForResponse_(msg);
        else if (msg.getCommand() === "clearbreakpoint")
            this.handleClearBreakpointResponse_(msg);
        else if (msg.getCommand() === "backtrace")
            this.handleBacktraceResponse_(msg);
        else if (msg.getCommand() === "lookup")
            this.invokeCallbackForResponse_(msg);
        else if (msg.getCommand() === "evaluate")
            this.invokeCallbackForResponse_(msg);
        else if (msg.getCommand() === "scope")
            this.invokeCallbackForResponse_(msg);
    }
};


/**
 * @param {devtools.DebuggerMessage} msg
 */
devtools.DebuggerAgent.prototype.handleBreakEvent_ = function(msg)
{
    if (!this.breakpointsActivated_) {
        this.resumeExecution();
        return;
    }

    // Force scripts panel to be shown first.
    WebInspector.currentPanel = WebInspector.panels.scripts;

    var body = msg.getBody();

    var line = devtools.DebuggerAgent.v8ToWwebkitLineNumber_(body.sourceLine);
    this.requestBacktrace_();
};


/**
 * @param {devtools.DebuggerMessage} msg
 */
devtools.DebuggerAgent.prototype.handleExceptionEvent_ = function(msg)
{
    var body = msg.getBody();
    // No script field in the body means that v8 failed to parse the script. We
    // resume execution on parser errors automatically.
    if (this.pauseOnExceptions_ && body.script) {
        var line = devtools.DebuggerAgent.v8ToWwebkitLineNumber_(body.sourceLine);
        this.createExceptionMessage_(body.script.name, line, body.exception.text);
        this.requestBacktrace_();

        // Force scripts panel to be shown.
        WebInspector.currentPanel = WebInspector.panels.scripts;
    } else
        this.resumeExecution();
};


/**
 * @param {devtools.DebuggerMessage} msg
 */
devtools.DebuggerAgent.prototype.handleScriptsResponse_ = function(msg)
{
    var scripts = msg.getBody();
    for (var i = 0; i < scripts.length; i++) {
        var script = scripts[i];

        // Skip scripts from other tabs.
        if (!this.isScriptFromInspectedContext_(script, msg))
            continue;

        // We may already have received the info in an afterCompile event.
        if (script.id in this.parsedScripts_)
            continue;
        this.addScriptInfo_(script, msg);
    }
};


/**
 * @param {Object} script Json object representing script.
 * @param {devtools.DebuggerMessage} msg Debugger response.
 */
devtools.DebuggerAgent.prototype.isScriptFromInspectedContext_ = function(script, msg)
{
    if (!script.context) {
        // Always ignore scripts from the utility context.
        return false;
    }
    var context = msg.lookup(script.context.ref);
    var scriptContextId = context.data;
    if (typeof scriptContextId === "undefined")
        return false; // Always ignore scripts from the utility context.
    if (this.contextId_ === null)
        return true;
    // Find the id from context data. The context data has the format "type,id".
    var comma = context.data.indexOf(",");
    if (comma < 0)
        return false;
    return (context.data.substring(comma + 1) == this.contextId_);
};


/**
 * @param {devtools.DebuggerMessage} msg
 */
devtools.DebuggerAgent.prototype.handleSetBreakpointResponse_ = function(msg)
{
    var requestSeq = msg.getRequestSeq();
    var breakpointInfo = this.requestNumberToBreakpointInfo_[requestSeq];
    if (!breakpointInfo) {
        // TODO(yurys): handle this case
        return;
    }
    delete this.requestNumberToBreakpointInfo_[requestSeq];
    if (!msg.isSuccess()) {
        // TODO(yurys): handle this case
        return;
    }
    var idInV8 = msg.getBody().breakpoint;
    breakpointInfo.setV8Id(idInV8);

    if (breakpointInfo.isRemoved())
        this.requestClearBreakpoint_(idInV8);
};


/**
 * @param {devtools.DebuggerMessage} msg
 */
devtools.DebuggerAgent.prototype.handleAfterCompileEvent_ = function(msg)
{
    if (!this.contextId_) {
        // Ignore scripts delta if main request has not been issued yet.
        return;
    }
    var script = msg.getBody().script;

    // Ignore scripts from other tabs.
    if (!this.isScriptFromInspectedContext_(script, msg))
        return;
    this.addScriptInfo_(script, msg);
};


/**
 * Adds the script info to the local cache. This method assumes that the script
 * is not in the cache yet.
 * @param {Object} script Script json object from the debugger message.
 * @param {devtools.DebuggerMessage} msg Debugger message containing the script
 *     data.
 */
devtools.DebuggerAgent.prototype.addScriptInfo_ = function(script, msg)
{
    var context = msg.lookup(script.context.ref);
    var contextType;
    // Find the type from context data. The context data has the format
    // "type,id".
    var comma = context.data.indexOf(",");
    if (comma < 0)
        return
    contextType = context.data.substring(0, comma);
    this.parsedScripts_[script.id] = new devtools.ScriptInfo(script.id, script.name, script.lineOffset, contextType);
    if (this.scriptsPanelInitialized_) {
        // Only report script as parsed after scripts panel has been shown.
        WebInspector.parsedScriptSource(script.id, script.name, script.source, script.lineOffset + 1);
        this.restoreBreakpoints_(script.id, script.name);
    }
};


/**
 * @param {devtools.DebuggerMessage} msg
 */
devtools.DebuggerAgent.prototype.handleClearBreakpointResponse_ = function(msg)
{
    // Do nothing.
};


/**
 * Handles response to "backtrace" command.
 * @param {devtools.DebuggerMessage} msg
 */
devtools.DebuggerAgent.prototype.handleBacktraceResponse_ = function(msg)
{
    if (this.waitingForInitialScriptsResponse_)
        this.pendingBacktraceResponseHandler_ = this.doHandleBacktraceResponse_.bind(this, msg);
    else
        this.doHandleBacktraceResponse_(msg);
};


/**
 * @param {devtools.DebuggerMessage} msg
 */
devtools.DebuggerAgent.prototype.doHandleBacktraceResponse_ = function(msg)
{
    var frames = msg.getBody().frames;
    this.callFrames_ = [];
    for (var i = 0; i <  frames.length; ++i)
        this.callFrames_.push(this.formatCallFrame_(frames[i]));
    WebInspector.pausedScript(this.callFrames_);
    this.showPendingExceptionMessage_();
    InspectorFrontendHost.bringToFront();
};


/**
 * Evaluates code on given callframe.
 */
devtools.DebuggerAgent.prototype.evaluateInCallFrame = function(callFrameId, code, callback)
{
    var callFrame = this.callFrames_[callFrameId];
    callFrame.evaluate_(code, callback);
};


/**
 * Handles response to a command by invoking its callback (if any).
 * @param {devtools.DebuggerMessage} msg
 * @return {boolean} Whether a callback for the given message was found and
 *     excuted.
 */
devtools.DebuggerAgent.prototype.invokeCallbackForResponse_ = function(msg)
{
    var callback = this.requestSeqToCallback_[msg.getRequestSeq()];
    if (!callback) {
        // It may happend if reset was called.
        return false;
    }
    delete this.requestSeqToCallback_[msg.getRequestSeq()];
    callback(msg);
    return true;
};


/**
 * @param {Object} stackFrame Frame json object from "backtrace" response.
 * @return {!devtools.CallFrame} Object containing information related to the
 *     call frame in the format expected by ScriptsPanel and its panes.
 */
devtools.DebuggerAgent.prototype.formatCallFrame_ = function(stackFrame)
{
    var func = stackFrame.func;
    var sourceId = func.scriptId;

    // Add service script if it does not exist.
    var existingScript = this.parsedScripts_[sourceId];
    if (!existingScript) {
        this.parsedScripts_[sourceId] = new devtools.ScriptInfo(sourceId, null /* name */, 0 /* line */, "unknown" /* type */, true /* unresolved */);
        WebInspector.parsedScriptSource(sourceId, null, null, 0);
    }

    var funcName = func.name || func.inferredName || "(anonymous function)";
    var line = devtools.DebuggerAgent.v8ToWwebkitLineNumber_(stackFrame.line);

    // Add basic scope chain info with scope variables.
    var scopeChain = [];
    var ScopeType = devtools.DebuggerAgent.ScopeType;
    for (var i = 0; i < stackFrame.scopes.length; i++) {
        var scope = stackFrame.scopes[i];
        scope.frameNumber = stackFrame.index;
        var scopeObjectProxy = new WebInspector.ObjectProxy(0, scope, [], "", true);
        scopeObjectProxy.isScope = true;
        switch(scope.type) {
            case ScopeType.Global:
                scopeObjectProxy.isDocument = true;
                break;
            case ScopeType.Local:
                scopeObjectProxy.isLocal = true;
                scopeObjectProxy.thisObject = devtools.DebuggerAgent.formatObjectProxy_(stackFrame.receiver);
                break;
            case ScopeType.With:
            // Catch scope is treated as a regular with scope by WebKit so we
            // also treat it this way.
            case ScopeType.Catch:
                scopeObjectProxy.isWithBlock = true;
                break;
            case ScopeType.Closure:
                scopeObjectProxy.isClosure = true;
                break;
        }
        scopeChain.push(scopeObjectProxy);
    }
    return new devtools.CallFrame(stackFrame.index, "function", funcName, sourceId, line, scopeChain);
};


/**
 * Restores breakpoints associated with the URL of a newly parsed script.
 * @param {number} sourceID The id of the script.
 * @param {string} scriptUrl URL of the script.
 */
devtools.DebuggerAgent.prototype.restoreBreakpoints_ = function(sourceID, scriptUrl)
{
    var breakpoints = this.urlToBreakpoints_[scriptUrl];
    for (var line in breakpoints) {
        if (parseInt(line) == line) {
            var v8Line = devtools.DebuggerAgent.v8ToWwebkitLineNumber_(parseInt(line));
            WebInspector.restoredBreakpoint(sourceID, scriptUrl, v8Line, breakpoints[line].enabled(), breakpoints[line].condition());
        }
    }
};


/**
 * Collects properties for an object from the debugger response.
 * @param {Object} object An object from the debugger protocol response.
 * @param {Array.<WebInspector.ObjectPropertyProxy>} result An array to put the
 *     properties into.
 * @param {boolean} noIntrinsic Whether intrinsic properties should be
 *     included.
 */
devtools.DebuggerAgent.formatObjectProperties_ = function(object, result, noIntrinsic)
{
    devtools.DebuggerAgent.propertiesToProxies_(object.properties, result);
    if (noIntrinsic)
        return;

    result.push(new WebInspector.ObjectPropertyProxy("__proto__", devtools.DebuggerAgent.formatObjectProxy_(object.protoObject)));
    result.push(new WebInspector.ObjectPropertyProxy("constructor", devtools.DebuggerAgent.formatObjectProxy_(object.constructorFunction)));
    // Don't add 'prototype' property since it is one of the regualar properties.
};


/**
 * For each property in "properties" creates its proxy representative.
 * @param {Array.<Object>} properties Receiver properties or locals array from
 *     "backtrace" response.
 * @param {Array.<WebInspector.ObjectPropertyProxy>} Results holder.
 */
devtools.DebuggerAgent.propertiesToProxies_ = function(properties, result)
{
    var map = {};
    for (var i = 0; i < properties.length; ++i) {
        var property = properties[i];
        var name = String(property.name);
        if (name in map)
            continue;
        map[name] = true;
        var value = devtools.DebuggerAgent.formatObjectProxy_(property.value);
        var propertyProxy = new WebInspector.ObjectPropertyProxy(name, value);
        result.push(propertyProxy);
    }
};


/**
 * @param {Object} v An object reference from the debugger response.
 * @return {*} The value representation expected by ScriptsPanel.
 */
devtools.DebuggerAgent.formatObjectProxy_ = function(v)
{
    var description;
    var hasChildren = false;
    if (v.type === "object") {
        description = v.className;
        hasChildren = true;
    } else if (v.type === "function") {
        if (v.source)
            description = v.source;
        else
            description = "function " + v.name + "()";
        hasChildren = true;
    } else if (v.type === "undefined")
        description = "undefined";
    else if (v.type === "null")
        description = "null";
    else if (typeof v.value !== "undefined") {
        // Check for undefined and null types before checking the value, otherwise
        // null/undefined may have blank value.
        description = v.value;
    } else
        description = "<unresolved ref: " + v.ref + ", type: " + v.type + ">";

    var proxy = new WebInspector.ObjectProxy(0, v, [], description, hasChildren);
    proxy.type = v.type;
    proxy.isV8Ref = true;
    return proxy;
};


/**
 * Converts line number from Web Inspector UI(1-based) to v8(0-based).
 * @param {number} line Resource line number in Web Inspector UI.
 * @return {number} The line number in v8.
 */
devtools.DebuggerAgent.webkitToV8LineNumber_ = function(line)
{
    return line - 1;
};


/**
 * Converts line number from v8(0-based) to Web Inspector UI(1-based).
 * @param {number} line Resource line number in v8.
 * @return {number} The line number in Web Inspector.
 */
devtools.DebuggerAgent.v8ToWwebkitLineNumber_ = function(line)
{
    return line + 1;
};


/**
 * @param {number} scriptId Id of the script.
 * @param {?string} url Script resource URL if any.
 * @param {number} lineOffset First line 0-based offset in the containing
 *     document.
 * @param {string} contextType Type of the script's context:
 *     "page" - regular script from html page
 *     "injected" - extension content script
 * @param {bool} opt_isUnresolved If true, script will not be resolved.
 * @constructor
 */
devtools.ScriptInfo = function(scriptId, url, lineOffset, contextType, opt_isUnresolved)
{
    this.scriptId_ = scriptId;
    this.lineOffset_ = lineOffset;
    this.contextType_ = contextType;
    this.url_ = url;
    this.isUnresolved_ = opt_isUnresolved;

    this.lineToBreakpointInfo_ = {};
};


/**
 * @return {number}
 */
devtools.ScriptInfo.prototype.getLineOffset = function()
{
    return this.lineOffset_;
};


/**
 * @return {string}
 */
devtools.ScriptInfo.prototype.getContextType = function()
{
    return this.contextType_;
};


/**
 * @return {?string}
 */
devtools.ScriptInfo.prototype.getUrl = function()
{
    return this.url_;
};


/**
 * @return {?bool}
 */
devtools.ScriptInfo.prototype.isUnresolved = function()
{
    return this.isUnresolved_;
};


/**
 * @param {number} line 0-based line number in the script.
 * @return {?devtools.BreakpointInfo} Information on a breakpoint at the
 *     specified line in the script or undefined if there is no breakpoint at
 *     that line.
 */
devtools.ScriptInfo.prototype.getBreakpointInfo = function(line)
{
    return this.lineToBreakpointInfo_[line];
};


/**
 * Adds breakpoint info to the script.
 * @param {devtools.BreakpointInfo} breakpoint
 */
devtools.ScriptInfo.prototype.addBreakpointInfo = function(breakpoint)
{
    this.lineToBreakpointInfo_[breakpoint.getLine()] = breakpoint;
};


/**
 * @param {devtools.BreakpointInfo} breakpoint Breakpoint info to be removed.
 */
devtools.ScriptInfo.prototype.removeBreakpointInfo = function(breakpoint)
{
    var line = breakpoint.getLine();
    delete this.lineToBreakpointInfo_[line];
};



/**
 * @param {number} line Breakpoint 0-based line number in the containing script.
 * @constructor
 */
devtools.BreakpointInfo = function(line, enabled, condition)
{
    this.line_ = line;
    this.enabled_ = enabled;
    this.condition_ = condition;
    this.v8id_ = -1;
    this.removed_ = false;
};


/**
 * @return {number}
 */
devtools.BreakpointInfo.prototype.getLine = function(n)
{
    return this.line_;
};


/**
 * @return {number} Unique identifier of this breakpoint in the v8 debugger.
 */
devtools.BreakpointInfo.prototype.getV8Id = function(n)
{
    return this.v8id_;
};


/**
 * Sets id of this breakpoint in the v8 debugger.
 * @param {number} id
 */
devtools.BreakpointInfo.prototype.setV8Id = function(id)
{
    this.v8id_ = id;
};


/**
 * Marks this breakpoint as removed from the front-end.
 */
devtools.BreakpointInfo.prototype.markAsRemoved = function()
{
    this.removed_ = true;
};


/**
 * @return {boolean} Whether this breakpoint has been removed from the
 *     front-end.
 */
devtools.BreakpointInfo.prototype.isRemoved = function()
{
    return this.removed_;
};


/**
 * @return {boolean} Whether this breakpoint is enabled.
 */
devtools.BreakpointInfo.prototype.enabled = function()
{
    return this.enabled_;
};


/**
 * @return {?string} Breakpoint condition.
 */
devtools.BreakpointInfo.prototype.condition = function()
{
    return this.condition_;
};


/**
 * Call stack frame data.
 * @param {string} id CallFrame id.
 * @param {string} type CallFrame type.
 * @param {string} functionName CallFrame type.
 * @param {string} sourceID Source id.
 * @param {number} line Source line.
 * @param {Array.<Object>} scopeChain Array of scoped objects.
 * @construnctor
 */
devtools.CallFrame = function(id, type, functionName, sourceID, line, scopeChain)
{
    this.id = id;
    this.type = type;
    this.functionName = functionName;
    this.sourceID = sourceID;
    this.line = line;
    this.scopeChain = scopeChain;
};


/**
 * This method issues asynchronous evaluate request, reports result to the
 * callback.
 * @param {string} expression An expression to be evaluated in the context of
 *     this call frame.
 * @param {function(Object):undefined} callback Callback to report result to.
 */
devtools.CallFrame.prototype.evaluate_ = function(expression, callback)
{
    devtools.tools.getDebuggerAgent().requestEvaluate({
            "expression": expression,
            "frame": this.id,
            "global": false,
            "disable_break": false,
            "compactFormat": true,
            "maxStringLength": -1
        },
        function(response) {
            var result = {};
            if (response.isSuccess())
                result.value = devtools.DebuggerAgent.formatObjectProxy_(response.getBody());
            else {
                result.value = response.getMessage();
                result.isException = true;
            }
            callback(result);
        });
};


/**
 * JSON based commands sent to v8 debugger.
 * @param {string} command Name of the command to execute.
 * @param {Object} opt_arguments Command-specific arguments map.
 * @constructor
 */
devtools.DebugCommand = function(command, opt_arguments)
{
    this.command_ = command;
    this.type_ = "request";
    this.seq_ = ++devtools.DebugCommand.nextSeq_;
    if (opt_arguments)
        this.arguments_ = opt_arguments;
};


/**
 * Next unique number to be used as debugger request sequence number.
 * @type {number}
 */
devtools.DebugCommand.nextSeq_ = 1;


/**
 * @return {number}
 */
devtools.DebugCommand.prototype.getSequenceNumber = function()
{
    return this.seq_;
};


/**
 * @return {string}
 */
devtools.DebugCommand.prototype.toJSONProtocol = function()
{
    var json = {
        "seq": this.seq_,
        "type": this.type_,
        "command": this.command_
    }
    if (this.arguments_)
        json.arguments = this.arguments_;
    return JSON.stringify(json);
};


/**
 * JSON messages sent from v8 debugger. See protocol definition for more
 * details: http://code.google.com/p/v8/wiki/DebuggerProtocol
 * @param {string} msg Raw protocol packet as JSON string.
 * @constructor
 */
devtools.DebuggerMessage = function(msg)
{
    this.packet_ = JSON.parse(msg);
    this.refs_ = [];
    if (this.packet_.refs) {
        for (var i = 0; i < this.packet_.refs.length; i++)
            this.refs_[this.packet_.refs[i].handle] = this.packet_.refs[i];
    }
};


/**
 * @return {string} The packet type.
 */
devtools.DebuggerMessage.prototype.getType = function()
{
    return this.packet_.type;
};


/**
 * @return {?string} The packet event if the message is an event.
 */
devtools.DebuggerMessage.prototype.getEvent = function()
{
    return this.packet_.event;
};


/**
 * @return {?string} The packet command if the message is a response to a
 *     command.
 */
devtools.DebuggerMessage.prototype.getCommand = function()
{
    return this.packet_.command;
};


/**
 * @return {number} The packet request sequence.
 */
devtools.DebuggerMessage.prototype.getRequestSeq = function()
{
    return this.packet_.request_seq;
};


/**
 * @return {number} Whether the v8 is running after processing the request.
 */
devtools.DebuggerMessage.prototype.isRunning = function()
{
    return this.packet_.running ? true : false;
};


/**
 * @return {boolean} Whether the request succeeded.
 */
devtools.DebuggerMessage.prototype.isSuccess = function()
{
    return this.packet_.success ? true : false;
};


/**
 * @return {string}
 */
devtools.DebuggerMessage.prototype.getMessage = function()
{
    return this.packet_.message;
};


/**
 * @return {Object} Parsed message body json.
 */
devtools.DebuggerMessage.prototype.getBody = function()
{
    return this.packet_.body;
};


/**
 * @param {number} handle Object handle.
 * @return {?Object} Returns the object with the handle if it was sent in this
 *    message(some objects referenced by handles may be missing in the message).
 */
devtools.DebuggerMessage.prototype.lookup = function(handle)
{
    return this.refs_[handle];
};
