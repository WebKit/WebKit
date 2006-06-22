/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

var sourceFiles = new Array();
var currentSourceId = -1;
var currentRow = null;
var currentStack = null;
var previousFiles = new Array();
var nextFiles = new Array();

function sleep(numberMillis) {
    var now = new Date();
    var exitTime = now.getTime() + numberMillis;
    while (true) {
        now = new Date();
        if (now.getTime() > exitTime)
            return;
    }
}

function keyPressed(event) {
    if (event.charCode == 112) pause();
    else if (event.charCode == 99) resume();
    else if (event.charCode == 115) step();
}

function dividerDragStart(element, dividerDrag, dividerDragEnd, event) {
    element.dragging = true;
    element.dragLastX = event.clientX + window.scrollX;
    document.addEventListener("mousemove", dividerDrag, true);
    document.addEventListener("mouseup", dividerDragEnd, true);
    event.preventDefault();
}

function sourceDividerDragStart(event) {
    dividerDragStart(document.getElementById("divider"), dividerDrag, sourceDividerDragEnd, event);
}

function infoDividerDragStart(event) {
    dividerDragStart(document.getElementById("infoDivider"), infoDividerDrag, infoDividerDragEnd, event);
}

function infoDividerDragEnd(event) {
    dividerDragEnd(document.getElementById("infoDivider"), infoDividerDrag, infoDividerDragEnd);
}

function sourceDividerDragEnd(event) {
    dividerDragEnd(document.getElementById("divider"), dividerDrag, sourceDividerDragEnd);
}

function dividerDragEnd(element, dividerDrag, dividerDragEnd, event) {
    element.dragging = false;
    document.removeEventListener("mousemove", dividerDrag, true);
    document.removeEventListener("mouseup", dividerDragEnd, true);
}

function infoDividerDrag(event) {
    var element = document.getElementById("infoDivider");
    if (document.getElementById("infoDivider").dragging == true) {
      var main = document.getElementById("main");
      var leftPane = document.getElementById("leftPane");
      var rightPane = document.getElementById("rightPane");
      var x = event.clientX + window.scrollX;
       
      if (x < main.clientWidth * 0.25)
        x = main.clientWidth * 0.25;
        else if (x > main.clientWidth * 0.75)
         x = main.clientWidth * 0.75;

        leftPane.style.width = x + "px";
        rightPane.style.left = x + "px";
        element.dragLastX = x;
        event.preventDefault();
    }
}

function dividerDrag(event) {
    var element = document.getElementById("divider");
    if (document.getElementById("divider").dragging == true) {
        var main = document.getElementById("main");
        var top = document.getElementById("info");
        var bottom = document.getElementById("body");
        var y = event.clientY + window.scrollY;
        var delta = element.dragLastY - y;

        var newHeight = top.clientHeight - delta;
        if (newHeight < main.clientHeight * 0.25)
            newHeight = main.clientHeight * 0.25;
        else if (newHeight > main.clientHeight * 0.75)
            newHeight = main.clientHeight * 0.75;

        top.style.height = newHeight + "px";
        bottom.style.top = newHeight + "px";

        element.dragLastY = y;
        event.preventDefault();
    }
}

function loaded() {
    window.addEventListener("keypress", keyPressed, false);
    document.getElementById("divider").addEventListener("mousedown", sourceDividerDragStart, false);
    document.getElementById("infoDivider").addEventListener("mousedown", infoDividerDragStart, false);
}

function isPaused() {
    return DebuggerDocument.isPaused();
}

function pause() {
    DebuggerDocument.pause();
}

function resume()
{
    if (currentRow) {
        removeStyleClass(currentRow, "current");
        currentRow = null;
    }

    if (currentStack) {
        var stackframeTable = document.getElementById("stackframeTable");
        stackframeTable.innerHTML = ""; // clear the content
        currentStack = null;
    }

    DebuggerDocument.resume();
}

function step()
{
    DebuggerDocument.step();
}

function hasStyleClass(element,className)
{
    return ( element.className.indexOf(className) != -1 );
}

function addStyleClass(element,className)
{
    if (!hasStyleClass(element,className))
        element.className += ( element.className.length ? " " + className : className );
}

function removeStyleClass(element,className)
{
    if (hasStyleClass(element,className))
        element.className = element.className.replace(className, "");
}

function addBreakPoint(event)
{
    var row = event.target.parentNode;
    if (hasStyleClass(row, "breakpoint")) {
        if (hasStyleClass(row, "disabled")) {
            removeStyleClass(row, "disabled");
            sourceFiles[currentSourceId].breakpoints[parseInt(event.target.title)] = 1;
        } else {
            addStyleClass(row, "disabled");
            sourceFiles[currentSourceId].breakpoints[parseInt(event.target.title)] = -1;
        }
    } else {
        addStyleClass(row, "breakpoint");
        removeStyleClass(row, "disabled");
        sourceFiles[currentSourceId].breakpoints[parseInt(event.target.title)] = 1;
    }
}

function totalOffsetTop(element,stop)
{
    var currentTop = 0;
    if (element.offsetParent) {
        while (element.offsetParent) {
            currentTop += element.offsetTop
            element = element.offsetParent;
            if (element == stop)
                break;
        }
    }
    return currentTop;
}

function switchFile()
{
    var files = document.getElementById("files");
    loadSource(files.options[files.selectedIndex].value,true);
}

function syntaxHighlight(code)
{
    var keywords = { 'abstract': 1, 'boolean': 1, 'break': 1, 'byte': 1, 'case': 1, 'catch': 1, 'char': 1, 'class': 1, 'const': 1, 'continue': 1, 'debugger': 1, 'default': 1, 'delete': 1, 'do': 1, 'double': 1, 'else': 1, 'enum': 1, 'export': 1, 'extends': 1, 'false': 1, 'final': 1, 'finally': 1, 'float': 1, 'for': 1, 'function': 1, 'goto': 1, 'if': 1, 'implements': 1, 'import': 1, 'in': 1, 'instanceof': 1, 'int': 1, 'interface': 1, 'long': 1, 'native': 1, 'new': 1, 'null': 1, 'package': 1, 'private': 1, 'protected': 1, 'public': 1, 'return': 1, 'short': 1, 'static': 1, 'super': 1, 'switch': 1, 'synchronized': 1, 'this': 1, 'throw': 1, 'throws': 1, 'transient': 1, 'true': 1, 'try': 1, 'typeof': 1, 'var': 1, 'void': 1, 'volatile': 1, 'while': 1, 'with': 1 };

    function echoChar(c) {
        if (c == '<')
            result += '&lt;';
        else if (c == '>')
            result += '&gt;';
        else if (c == '&')
            result += '&amp;';
        else if (c == '\t')
            result += '    ';
        else
            result += c;
    }

    function isDigit(number) {
        var string = "1234567890";
        if (string.indexOf(number) != -1)
            return true;
        return false;
    }

    function isHex(hex) {
        var string = "1234567890abcdefABCDEF";
        if (string.indexOf(hex) != -1)
            return true;
        return false;
    }

    function isLetter(letter) {
        var string = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        if (string.indexOf(letter) != -1)
            return true;
        return false;
    }

    var result = "";
    var cPrev = "";
    var c = "";
    var cNext = "";
    for (var i = 0; i < code.length; i++) {
        cPrev = c;
        c = code.charAt(i);
        cNext = code.charAt(i + 1);

        if (c == "/" && cNext == "*") {
            result += "<span class=\"comment\">";
            echoChar(c);
            echoChar(cNext);
            for (i += 2; i < code.length; i++) {
                c = code.charAt(i);
                if (c == "\n")
                    result += "</span>";
                echoChar(c);
                if (c == "\n")
                    result += "<span class=\"comment\">";
                if (cPrev == "*" && c == "/")
                    break;
                cPrev = c;
            }
            result += "</span>";
            continue;
        } else if (c == "/" && cNext == "/") {
            result += "<span class=\"comment\">";
            echoChar(c);
            echoChar(cNext);
            for (i += 2; i < code.length; i++) {
                c = code.charAt(i);
                if (c == "\n")
                    break;
                echoChar(c);
            }
            result += "</span>";
            echoChar(c);
            continue;
        } else if (c == "\"" || c == "'") {
            var instringtype = c;
            var stringstart = i;
            result += "<span class=\"string\">";
            echoChar(c);
            for (i += 1; i < code.length; i++) {
                c = code.charAt(i);
                if (stringstart < (i - 1) && cPrev == instringtype && code.charAt(i - 2) != "\\")
                    break;
                echoChar(c);
                cPrev = c;
            }
            result += "</span>";
            echoChar(c);
            continue;
        } else if (c == "0" && cNext == "x" && (i == 0 || (!isLetter(cPrev) && !isDigit(cPrev)))) {
            result += "<span class=\"number\">";
            echoChar(c);
            echoChar(cNext);
            for (i += 2; i < code.length; i++) {
                c = code.charAt(i);
                if (!isHex(c))
                    break;
                echoChar(c);
            }
            result += "</span>";
            echoChar(c);
            continue;
        } else if ((isDigit(c) || ((c == "-" || c == ".") && isDigit(cNext))) && (i == 0 || (!isLetter(cPrev) && !isDigit(cPrev)))) {
            result += "<span class=\"number\">";
            echoChar(c);
            for (i += 1; i < code.length; i++) {
                c = code.charAt(i);
                if (!isDigit(c) && c != ".")
                    break;
                echoChar(c);
            }
            result += "</span>";
            echoChar(c);
            continue;
        } else if(isLetter(c) && (i == 0 || !isLetter(cPrev))) {
            var keyword = c;
            var cj = "";
            for (var j = i + 1; j < i + 12 && j < code.length; j++) {
                cj = code.charAt(j);
                if (!isLetter(cj))
                    break;
                keyword += cj;
            }

            if (keywords[keyword]) {
                result += "<span class=\"keyword\">" + keyword + "</span>";
                i += keyword.length - 1;
                continue;
            }
        }

        echoChar(c);
    }

    return result;
}

function navFilePrevious(element)
{
    if (element.disabled)
        return;
    var lastSource = previousFiles.pop();
    if (currentSourceId != -1)
        nextFiles.unshift(currentSourceId);
    loadSource(lastSource, false);
}

function navFileNext(element)
{
    if (element.disabled)
        return;
    var lastSource = nextFiles.shift();
    if (currentSourceId != -1)
        previousFiles.push(currentSourceId);
    loadSource(lastSource, false);
}

function updateFunctionStack()
{
    var stackframeTable = document.getElementById("stackframeTable");
    stackframeTable.innerHTML = ""; // clear the content

    currentStack = DebuggerDocument.currentFunctionStack();
    for(var i = 0; i < currentStack.length; i++) {
        var tr = document.createElement("tr");
        var td = document.createElement("td");
        td.className = "stackNumber";
        td.innerText = i;
        tr.appendChild(td);

        td = document.createElement("td");
        td.innerText = currentStack[i];
        tr.appendChild(td);

        stackframeTable.appendChild(tr);
    }

    var tr = document.createElement("tr");
    var td = document.createElement("td");
    td.className = "stackNumber";
    td.innerText = i;
    tr.appendChild(td);

    td = document.createElement("td");
    td.innerText = "Global";
    tr.appendChild(td);

    stackframeTable.appendChild(tr);
}

function loadSource(sourceId,manageNavLists)
{
    if (!sourceFiles[sourceId])
        return;

    if (currentSourceId != -1 && sourceFiles[currentSourceId] && sourceFiles[currentSourceId].element)
        sourceFiles[currentSourceId].element.style.display = "none";

    if (!sourceFiles[sourceId].loaded) {
        sourceFiles[sourceId].lines = sourceFiles[sourceId].source.split("\n");

        var sourcesDocument = document.getElementById("sources").contentDocument;
        var sourcesDiv = sourcesDocument.body;
        var sourceDiv = sourcesDocument.createElement("div");
        sourceDiv.id = "source" + sourceId;
        sourcesDiv.appendChild(sourceDiv);
        sourceFiles[sourceId].element = sourceDiv;

        var table = sourcesDocument.createElement("table");
        sourceDiv.appendChild(table);

        var lines = syntaxHighlight(sourceFiles[sourceId].source).split("\n");
        for( var i = 0; i < lines.length; i++ ) {
            var tr = sourcesDocument.createElement("tr");
            var td = sourcesDocument.createElement("td");
            td.className = "gutter";
            td.title = (i + 1);
            td.addEventListener("click", addBreakPoint, true);
            tr.appendChild(td);

            td = sourcesDocument.createElement("td");
            td.className = "source";
            td.innerHTML = lines[i];
            tr.appendChild(td);
            table.appendChild(tr);
        }

        sourceFiles[sourceId].loaded = true;
    }

    sourceFiles[sourceId].element.style.display = null;

    document.getElementById("filesPopupButtonContent").innerText = sourceFiles[sourceId].url;
    
    var files = document.getElementById("files");
    for (var i = 0; i < files.childNodes.length; i++) {
        if (files.childNodes[i].value == sourceId) {
            files.selectedIndex = i;
            break;
        }
    }

    if (manageNavLists) {
        nextFiles = new Array();
        if (currentSourceId != -1)
            previousFiles.push(currentSourceId);
    }

    document.getElementById("navFileLeftButton").disabled = (previousFiles.length == 0);
    document.getElementById("navFileRightButton").disabled = (nextFiles.length == 0);

    currentSourceId = sourceId;
}

function didParseScript(source,url,sourceId)
{
    sourceFiles[sourceId] = new Object();
    sourceFiles[sourceId].source = source;
    sourceFiles[sourceId].url = url;
    sourceFiles[sourceId].loaded = false;
    sourceFiles[sourceId].breakpoints = new Array();

    var files = document.getElementById("files");
    var option = document.createElement("option");
    sourceFiles[sourceId].menuOption = option;
    option.value = sourceId;
    option.text = url;
    files.appendChild(option);

    if (currentSourceId == -1)
        loadSource(sourceId,true);
    return true;
}

function willExecuteStatement(sourceId,line)
{
    if (line <= 0 || !sourceFiles[sourceId])
        return;

    if (sourceFiles[sourceId].breakpoints[line] == 1)
        pause();

    if (isPaused()) {
        if (currentSourceId != sourceId)
            loadSource(sourceId,true);
        if (currentRow)
            removeStyleClass(currentRow, "current");
        if (!sourceFiles[sourceId].element)
            return;
        if (sourceFiles[sourceId].element.firstChild.childNodes.length < line)
            return;

        updateFunctionStack();

        currentRow = sourceFiles[sourceId].element.firstChild.childNodes.item(line - 1);
        addStyleClass(currentRow, "current");

        var sourcesDiv = document.getElementById("sources");
        var sourcesDocument = document.getElementById("sources").contentDocument;
        var parent = sourcesDocument.body;
        var offset = totalOffsetTop(currentRow, parent);
        if (offset < (parent.scrollTop + 20) || offset > (parent.scrollTop + sourcesDiv.clientHeight - 20))
            parent.scrollTop = totalOffsetTop(currentRow, parent) - (sourcesDiv.clientHeight / 2) + 10;
    }
}

function didEnterCallFrame(sourceId,line)
{
    willExecuteStatement(sourceId,line);
}

function willLeaveCallFrame(sourceId,line)
{
    willExecuteStatement(sourceId,line);
}
