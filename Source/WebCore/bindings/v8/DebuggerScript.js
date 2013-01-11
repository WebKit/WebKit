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

(function () {

var DebuggerScript = {};

DebuggerScript.PauseOnExceptionsState = {
    DontPauseOnExceptions : 0,
    PauseOnAllExceptions : 1,
    PauseOnUncaughtExceptions: 2
};

DebuggerScript._pauseOnExceptionsState = DebuggerScript.PauseOnExceptionsState.DontPauseOnExceptions;
Debug.clearBreakOnException();
Debug.clearBreakOnUncaughtException();

DebuggerScript.getAfterCompileScript = function(eventData)
{
    return DebuggerScript._formatScript(eventData.script_.script_);
}

DebuggerScript.getWorkerScripts = function()
{
    var result = [];
    var scripts = Debug.scripts();
    for (var i = 0; i < scripts.length; ++i) {
        var script = scripts[i];
        // Workers don't share same V8 heap now so there is no need to complicate stuff with
        // the context id like we do to discriminate between scripts from different pages.
        // However we need to filter out v8 native scripts.
        if (script.context_data && script.context_data === "worker")
            result.push(DebuggerScript._formatScript(script));
    }
    return result;
}

DebuggerScript.getFunctionScopes = function(fun)
{
    var mirror = MakeMirror(fun);
    var count = mirror.scopeCount();
    if (count == 0)
        return null;
    var result = [];
    for (var i = 0; i < count; i++) {
        var scopeMirror = mirror.scope(i);
        result[i] = {
            type: scopeMirror.scopeType(),
            object: DebuggerScript._buildScopeObject(scopeMirror)
        };
    }
    return result;
}

DebuggerScript.getInternalProperties = function(value)
{
    var properties = ObjectMirror.GetInternalProperties(value);
    var result = [];
    for (var i = 0; i < properties.length; i++) {
        var mirror = properties[i];
        result.push({
            name: mirror.name(),
            value: mirror.value().value()
        }); 
    }
    return result;
}

DebuggerScript.getScripts = function(contextData)
{
    var result = [];

    if (!contextData)
        return result;
    var comma = contextData.indexOf(",");
    if (comma === -1)
        return result;
    // Context data is a string in the following format:
    // ("page"|"injected")","<page id>
    var idSuffix = contextData.substring(comma); // including the comma

    var scripts = Debug.scripts();
    for (var i = 0; i < scripts.length; ++i) {
        var script = scripts[i];
        if (script.context_data && script.context_data.lastIndexOf(idSuffix) != -1)
            result.push(DebuggerScript._formatScript(script));
    }
    return result;
}

DebuggerScript._formatScript = function(script)
{
    var lineEnds = script.line_ends;
    var lineCount = lineEnds.length;
    var endLine = script.line_offset + lineCount - 1;
    var endColumn;
    // V8 will not count last line if script source ends with \n.
    if (script.source[script.source.length - 1] === '\n') {
        endLine += 1;
        endColumn = 0;
    } else {
        if (lineCount === 1)
            endColumn = script.source.length + script.column_offset;
        else
            endColumn = script.source.length - (lineEnds[lineCount - 2] + 1);
    }

    return {
        id: script.id,
        name: script.nameOrSourceURL(),
        source: script.source,
        startLine: script.line_offset,
        startColumn: script.column_offset,
        endLine: endLine,
        endColumn: endColumn,
        isContentScript: !!script.context_data && script.context_data.indexOf("injected") == 0
    };
}

DebuggerScript.setBreakpoint = function(execState, args)
{
    var breakId = Debug.setScriptBreakPointById(args.sourceID, args.lineNumber, args.columnNumber, args.condition);

    var locations = Debug.findBreakPointActualLocations(breakId);
    if (!locations.length)
        return undefined;
    args.lineNumber = locations[0].line;
    args.columnNumber = locations[0].column;
    return breakId.toString();
}

DebuggerScript.removeBreakpoint = function(execState, args)
{
    Debug.findBreakPoint(args.breakpointId, true);
}

DebuggerScript.pauseOnExceptionsState = function()
{
    return DebuggerScript._pauseOnExceptionsState;
}

DebuggerScript.setPauseOnExceptionsState = function(newState)
{
    DebuggerScript._pauseOnExceptionsState = newState;

    if (DebuggerScript.PauseOnExceptionsState.PauseOnAllExceptions === newState)
        Debug.setBreakOnException();
    else
        Debug.clearBreakOnException();

    if (DebuggerScript.PauseOnExceptionsState.PauseOnUncaughtExceptions === newState)
        Debug.setBreakOnUncaughtException();
    else
        Debug.clearBreakOnUncaughtException();
}

DebuggerScript.currentCallFrame = function(execState, args)
{
    var frameCount = execState.frameCount();
    if (frameCount === 0)
        return undefined;

    var topFrame;
    for (var i = frameCount - 1; i >= 0; i--) {
        var frameMirror = execState.frame(i);
        topFrame = DebuggerScript._frameMirrorToJSCallFrame(frameMirror, topFrame);
    }
    return topFrame;
}

DebuggerScript.stepIntoStatement = function(execState)
{
    execState.prepareStep(Debug.StepAction.StepIn, 1);
}

DebuggerScript.stepOverStatement = function(execState)
{
    execState.prepareStep(Debug.StepAction.StepNext, 1);
}

DebuggerScript.stepOutOfFunction = function(execState)
{
    execState.prepareStep(Debug.StepAction.StepOut, 1);
}

DebuggerScript.liveEditScriptSource = function(scriptId, newSource, preview)
{
    var scripts = Debug.scripts();
    var scriptToEdit = null;
    for (var i = 0; i < scripts.length; i++) {
        if (scripts[i].id == scriptId) {
            scriptToEdit = scripts[i];
            break;
        }
    }
    if (!scriptToEdit)
        throw("Script not found");

    var changeLog = [];
    return Debug.LiveEdit.SetScriptSource(scriptToEdit, newSource, preview, changeLog);
}

DebuggerScript.clearBreakpoints = function(execState, args)
{
    Debug.clearAllBreakPoints();
}

DebuggerScript.setBreakpointsActivated = function(execState, args)
{
    Debug.debuggerFlags().breakPointsActive.setValue(args.enabled);
}

DebuggerScript.getScriptSource = function(eventData)
{
    return eventData.script().source();
}

DebuggerScript.setScriptSource = function(eventData, source)
{
    if (eventData.script().data() === "injected-script")
        return;
    eventData.script().setSource(source);
}

DebuggerScript.getScriptName = function(eventData)
{
    return eventData.script().script_.nameOrSourceURL();
}

DebuggerScript._frameMirrorToJSCallFrame = function(frameMirror, callerFrame)
{
    // Get function name.
    var func;
    try {
        func = frameMirror.func();
    } catch(e) {
    }
    var functionName;
    if (func)
        functionName = func.name() || func.inferredName();

    // Get script ID.
    var script = func.script();
    var sourceID = script && script.id();

    // Get location.
    var location  = frameMirror.sourceLocation();

    // Get this object.
    var thisObject = frameMirror.details_.receiver();

    // Get scope chain array in format: [<scope type>, <scope object>, <scope type>, <scope object>,...]
    var scopeChain = [];
    var scopeType = [];
    for (var i = 0; i < frameMirror.scopeCount(); i++) {
        var scopeMirror = frameMirror.scope(i);
        scopeType.push(scopeMirror.scopeType());
        scopeChain.push(DebuggerScript._buildScopeObject(scopeMirror));
    }

    function evaluate(expression)
    {
        return frameMirror.evaluate(expression, false).value();
    }

    function restart()
    {
        return Debug.LiveEdit.RestartFrame(frameMirror);
    }

    return {
        "sourceID": sourceID,
        "line": location ? location.line : 0,
        "column": location ? location.column : 0,
        "functionName": functionName,
        "thisObject": thisObject,
        "scopeChain": scopeChain,
        "scopeType": scopeType,
        "evaluate": evaluate,
        "caller": callerFrame,
        "restart": restart
    };
}

DebuggerScript._buildScopeObject = function(scopeMirror) {
    var scopeObject;
    switch (scopeMirror.scopeType()) {
    case ScopeType.Local:
    case ScopeType.Closure:
    case ScopeType.Catch:
        // For transient objects we create a "persistent" copy that contains
        // the same properties.
        scopeObject = {};
        // Reset scope object prototype to null so that the proto properties
        // don't appear in the local scope section.
        scopeObject.__proto__ = null;
        var scopeObjectMirror = scopeMirror.scopeObject();
        var properties = scopeObjectMirror.properties();
        for (var j = 0; j < properties.length; j++) {
            var name = properties[j].name();
            if (name.charAt(0) === ".")
                continue; // Skip internal variables like ".arguments"
            scopeObject[name] = properties[j].value_;
        }
        break;
    case ScopeType.Global:
    case ScopeType.With:
        scopeObject = scopeMirror.details_.object();
        break;
    case ScopeType.Block:
        // Unsupported yet. Mustn't be reachable.
        break;
    }
    return scopeObject;
}

return DebuggerScript;
})();
