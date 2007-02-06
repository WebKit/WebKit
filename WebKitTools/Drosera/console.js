/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 David Smith (catfish.man@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

var inputElement = null;
var mainWindow = window.opener;
var history = [""];
var historyIndex = 0;

function loaded()
{
    inputElement = document.getElementById("input");
    inputElement.addEventListener("keydown", inputKeyDown, false);
    inputElement.addEventListener("keyup", inputKeyUp, false);
    inputElement.focus();
}

function inputKeyDown(event)
{
    if (event.keyCode == 13 && !event.altKey) {
        if (mainWindow.isPaused() && mainWindow.currentStack) {
            history[history.length - 1] = inputElement.innerText;
            sendScript(inputElement.innerText);
            inputElement.innerText = "";
            history.push("");
            historyIndex = history.length - 1;
            inputElement.focus();
        } else
            alert("The debugger needs to be paused.\tIn order to evaluate your script input you need to pause the debugger in the context of another script.");
        event.preventDefault();
    } else if (event.keyCode == 38 && !event.altKey && historyIndex > 0) {
        historyIndex--;
        inputElement.innerText = history[historyIndex];
        inputElement.focus()
        event.preventDefault();
    } else if (event.keyCode == 40 && !event.altKey && historyIndex < (history.length - 1)) {
        historyIndex++;
        inputElement.innerText = history[historyIndex];
        inputElement.focus()
        event.preventDefault();
    }
}

function inputKeyUp(event)
{
    if (event.keyCode != 38 && event.keyCode != 40 && event.keyCode != 13) {
        history[historyIndex] = inputElement.innerText;
    }
}

function appendMessage(exp, msg)
{
    var historyDisplay = document.getElementById("history");
    var row = document.createElement("div");
    row.className = "row";
    if (historyDisplay.childNodes.length % 2)
        row.className += " alt";
    
    if (exp.length > 0) {
        var expression = document.createElement("div");
        expression.className = "expression";
        expression.innerText = exp;
        row.appendChild(expression);
    }
    
    var result = document.createElement("div");
    result.className = "result";
    result.innerText = msg;
    
    row.appendChild(result);
    
    historyDisplay.appendChild(row);
    historyDisplay.scrollTop = historyDisplay.scrollHeight;
}

function sendScript(script)
{
    appendMessage(script, mainWindow.DebuggerDocument.evaluateScript_inCallFrame_(script, mainWindow.currentCallFrame.index));

    if (script.indexOf("=") >= 0)
        mainWindow.currentCallFrame.loadVariables();
}
