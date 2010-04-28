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

function debuggerScriptConstructor() {

var DebuggerScript = {};
DebuggerScript._breakpoints = {};

DebuggerScript.PauseOnExceptionsState = {
    DontPauseOnExceptions : 0,
    PauseOnAllExceptions : 1,
    PauseOnUncaughtExceptions: 2
};

DebuggerScript._pauseOnExceptionsState = DebuggerScript.PauseOnExceptionsState.DontPauseOnExceptions;
Debug.clearBreakOnException();
Debug.clearBreakOnUncaughtException();

DebuggerScript.getAfterCompileScript = function(execState, args)
{
    return DebuggerScript._formatScript(args.eventData.script_.script_);
}

DebuggerScript.getScripts = function(contextData)
{
    var scripts = Debug.scripts();
    var result = [];
    for (var i = 0; i < scripts.length; ++i) {
        var script = scripts[i];
        if (contextData === script.context_data)
            result.push(DebuggerScript._formatScript(script));
    }
    return result;
}

DebuggerScript._formatScript = function(script)
{
    return {
        id: script.id,
        name: script.name,
        source: script.source,
        lineOffset: script.line_offset,
        lineCount: script.lineCount(),
        contextData: script.context_data
    };
}

DebuggerScript.setBreakpoint = function(execState, args)
{
    args.lineNumber = DebuggerScript._webkitToV8LineNumber(args.lineNumber);
    var key = args.scriptId + ":" + args.lineNumber;
    var breakId = DebuggerScript._breakpoints[key];
    if (breakId) {
        if (args.enabled)
            Debug.enableScriptBreakPoint(breakId);
        else
            Debug.disableScriptBreakPoint(breakId);
        Debug.changeScriptBreakPointCondition(breakId, args.condition);
        return breakId;
    }

    breakId = Debug.setScriptBreakPointById(args.scriptId, args.lineNumber, 0 /* column */, args.condition);
    DebuggerScript._breakpoints[key] = breakId;
    if (!args.enabled)
        Debug.disableScriptBreakPoint(breakId);
    return breakId;
}

DebuggerScript.removeBreakpoint = function(execState, args)
{
    args.lineNumber = DebuggerScript._webkitToV8LineNumber(args.lineNumber);
    var key = args.scriptId + ":" + args.lineNumber;
    var breakId = DebuggerScript._breakpoints[key];
    if (breakId)
        Debug.findBreakPoint(breakId, true);
    delete DebuggerScript._breakpoints[key];
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

DebuggerScript.stepIntoStatement = function(execState, args)
{
    execState.prepareStep(Debug.StepAction.StepIn, 1);
}

DebuggerScript.stepOverStatement = function(execState, args)
{
    execState.prepareStep(Debug.StepAction.StepNext, 1);
}

DebuggerScript.stepOutOfFunction = function(execState, args)
{
    execState.prepareStep(Debug.StepAction.StepOut, 1);
}

DebuggerScript.clearBreakpoints = function(execState, args)
{
    for (var key in DebuggerScript._breakpoints) {
        var breakId = DebuggerScript._breakpoints[key];
        Debug.findBreakPoint(breakId, true);
    }
    DebuggerScript._breakpoints = {};
}

DebuggerScript.setBreakpointsActivated = function(execState, args)
{
    for (var key in DebuggerScript._breakpoints) {
        var breakId = DebuggerScript._breakpoints[key];
        if (args.enabled)
            Debug.enableScriptBreakPoint(breakId);
        else
            Debug.disableScriptBreakPoint(breakId);
    }
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
    if (!functionName)
        functionName = "[anonymous]";
        
    // Get script ID.
    var script = func.script();
    var sourceID = script && script.id();
    
    // Get line number.
    var line = DebuggerScript._v8ToWebkitLineNumber(frameMirror.sourceLine());
    
    // Get this object.
    var thisObject = frameMirror.details_.receiver();

    // Get scope chain array in format: [<scope type>, <scope object>, <scope type>, <scope object>,...]
    var scopeChain = [];
    var scopeType = [];
    for (var i = 0; i < frameMirror.scopeCount(); i++) {
        var scopeMirror = frameMirror.scope(i);
        var scopeObjectMirror = scopeMirror.scopeObject();
        var properties = scopeObjectMirror.properties();
        var scopeObject = {};
        for (var j = 0; j < properties.length; j++)
            scopeObject[properties[j].name()] = properties[j].value_;
        scopeType.push(scopeMirror.scopeType());
        scopeChain.push(scopeObject);
    }
    
    function evaluate(expression) {
        return frameMirror.evaluate(expression, false).value();
    }
    
    return {
        "sourceID": sourceID,
        "line": line,
        "functionName": functionName,
        "type": "function",
        "thisObject": thisObject,
        "scopeChain": scopeChain,
        "scopeType": scopeType,
        "evaluate": evaluate,
        "caller": callerFrame
    };
}

DebuggerScript._webkitToV8LineNumber = function(line)
{
    return line - 1;
};

DebuggerScript._v8ToWebkitLineNumber = function(line)
{
    return line + 1;
};

return DebuggerScript;

}
