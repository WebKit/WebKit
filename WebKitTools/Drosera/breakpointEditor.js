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

var mainWindow = window.opener;
var inputElement = null;
var lineNumber = 0;
var file = 0;

function loaded()
{
    file = mainWindow.currentFile;
    lineNumber = mainWindow.files[file].breakpointEditorWindows.indexOf(window);
    
    document.getElementsByTagName("title")[0].textContents = "Edit Breakpoint on Line " + lineNumber;
    
    var functionBody = mainWindow.files[file].breakpoints[lineNumber];
    if(functionBody == "break")
        functionBody = "return 1;";
    else {
        var startIndex = functionBody.indexOf("{") + 1;
        var endIndex = functionBody.lastIndexOf("}");
        functionBody = functionBody.substring(startIndex, endIndex);
    }
        
    inputElement = document.getElementById("input");
    inputElement.innerText = functionBody;
    inputElement.focus();
}

function saveBreakpoint()
{
    mainWindow.finishEditingBreakpoint(lineNumber, file, "__drosera_breakpoint_conditional_func = function() { " + inputElement.innerText + " }; __drosera_breakpoint_conditional_func();");
}