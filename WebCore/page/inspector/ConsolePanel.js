/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

WebInspector.ConsolePanel = function()
{
    WebInspector.Panel.call(this);

    this.messages = [];

    this.commandHistory = [];
    this.commandOffset = 0;

    this.messageList = document.createElement("ol");
    this.messageList.className = "console-message-list";
    this.element.appendChild(this.messageList);

    this.messageList.addEventListener("click", this.messageListClicked.bind(this), true);

    this.consolePrompt = document.createElement("textarea");
    this.consolePrompt.className = "console-prompt";
    this.element.appendChild(this.consolePrompt);

    this.consolePrompt.addEventListener("keydown", this.promptKeyDown.bind(this), false);

    var clearButtonText = WebInspector.UIString("Clear");
    this.clearMessagesElement = document.createElement("button");
    this.clearMessagesElement.appendChild(document.createTextNode(clearButtonText));
    this.clearMessagesElement.title = clearButtonText;
    this.clearMessagesElement.addEventListener("click", this.clearButtonClicked.bind(this), false);
}

WebInspector.ConsolePanel.prototype = {
    show: function()
    {
        WebInspector.Panel.prototype.show.call(this);
        WebInspector.consoleListItem.select();

        this.clearMessagesElement.removeStyleClass("hidden");
        if (!this.clearMessagesElement.parentNode)
            document.getElementById("toolbarButtons").appendChild(this.clearMessagesElement);
    },

    hide: function()
    {
        WebInspector.Panel.prototype.hide.call(this);
        WebInspector.consoleListItem.deselect();
        this.clearMessagesElement.addStyleClass("hidden");
    },

    addMessage: function(msg)
    {
        if (msg.url in WebInspector.resourceURLMap) {
            msg.resource = WebInspector.resourceURLMap[msg.url];
            switch (msg.level) {
                case WebInspector.ConsoleMessage.MessageLevel.Warning:
                    ++msg.resource.warnings;
                    msg.resource.panel.addMessageToSource(msg);
                    break;
                case WebInspector.ConsoleMessage.MessageLevel.Error:
                    ++msg.resource.errors;
                    msg.resource.panel.addMessageToSource(msg);
                    break;
            }
        }
        this.messages.push(msg);

        var item = msg.toListItem();
        item.message = msg;
        this.messageList.appendChild(item);
        item.scrollIntoView(false);
    },

    clearMessages: function()
    {
        for (var i = 0; i < this.messages.length; ++i) {
            var resource = this.messages[i].resource;
            if (!resource)
                continue;

            resource.errors = 0;
            resource.warnings = 0;
        }

        this.messages = [];
        this.messageList.removeChildren();
    },

    clearButtonClicked: function()
    {
        this.clearMessages();
    },

    messageListClicked: function(event)
    {
        var link = event.target.firstParentOrSelfWithNodeName("a");
        if (link && link.representedNode) {
            WebInspector.updateFocusedNode(link.representedNode);
            return;
        }

        var item = event.target.firstParentOrSelfWithNodeName("li");
        if (!item)
            return;

        var resource = item.message.resource;
        if (!resource)
            return;

        if (link && link.hasStyleClass("console-message-url")) {
            WebInspector.navigateToResource(resource);
            resource.panel.showSourceLine(item.message.line);
        }

        event.stopPropagation();
        event.preventDefault();
    },

    promptKeyDown: function(event)
    {
        switch (event.keyIdentifier) {
            case "Enter":
                this._onEnterPressed(event);
                break;
            case "Up":
                this._onUpPressed(event);
                break;
            case "Down":
                this._onDownPressed(event);
                break;
        }
    },

    _onEnterPressed: function(event)
    {
        event.preventDefault();
        event.stopPropagation();

        var str = this.consolePrompt.value;
        if (!str.length)
            return;

        this.commandHistory.push(str);
        this.commandOffset = 0;

        this.consolePrompt.value = "";

        var result;
        var exception = false;
        try {
            // This with block is needed to work around http://bugs.webkit.org/show_bug.cgi?id=11399
            with (InspectorController.inspectedWindow()) {
                result = eval(str);
            }
        } catch(e) {
            result = e;
            exception = true;
        }

        var level = exception ? WebInspector.ConsoleMessage.MessageLevel.Error : WebInspector.ConsoleMessage.MessageLevel.Log;

        this.addMessage(new WebInspector.ConsoleCommand(str, this._format(result)));
    },

    _onUpPressed: function(event)
    {
        event.preventDefault();
        event.stopPropagation();

        if (this.commandOffset == this.commandHistory.length)
            return;

        if (this.commandOffset == 0)
            this.tempSavedCommand = this.consolePrompt.value;

        ++this.commandOffset;
        this.consolePrompt.value = this.commandHistory[this.commandHistory.length - this.commandOffset];
        this.consolePrompt.moveCursorToEnd();
    },

    _onDownPressed: function(event)
    {
        event.preventDefault();
        event.stopPropagation();

        if (this.commandOffset == 0)
            return;

        --this.commandOffset;

        if (this.commandOffset == 0) {
            this.consolePrompt.value = this.tempSavedCommand;
            this.consolePrompt.moveCursorToEnd();
            delete this.tempSavedCommand;
            return;
        }

        this.consolePrompt.value = this.commandHistory[this.commandHistory.length - this.commandOffset];
        this.consolePrompt.moveCursorToEnd();
    },

    _format: function(output)
    {
        var type = Object.type(output);
        if (type === "object") {
            if (output instanceof Node)
                type = "node";
        }

        // We don't perform any special formatting on these types, so we just
        // pass them through the simple _formatvalue function.
        var undecoratedTypes = {
            "undefined": 1,
            "null": 1,
            "boolean": 1,
            "number": 1,
            "date": 1,
            "function": 1,
        };

        var formatter;
        if (type in undecoratedTypes)
            formatter = "_formatvalue";
        else {
            formatter = "_format" + type;
            if (!(formatter in this)) {
                formatter = "_formatobject";
                type = "object";
            }
        }

        var span = document.createElement("span");
        span.addStyleClass("console-formatted-" + type);
        this[formatter](output, span);
        return span;
    },

    _formatvalue: function(val, elem)
    {
        elem.appendChild(document.createTextNode(val));
    },

    _formatstring: function(str, elem)
    {
        elem.appendChild(document.createTextNode("\"" + str + "\""));
    },

    _formatregexp: function(re, elem)
    {
        var formatted = String(re).replace(/([\\\/])/g, "\\$1").replace(/\\(\/[gim]*)$/, "$1").substring(1);
        elem.appendChild(document.createTextNode(formatted));
    },

    _formatarray: function(arr, elem)
    {
        elem.appendChild(document.createTextNode("["));
        for (var i = 0; i < arr.length; ++i) {
            elem.appendChild(this._format(arr[i]));
            if (i < arr.length - 1)
                elem.appendChild(document.createTextNode(", "));
        }
        elem.appendChild(document.createTextNode("]"));
    },

    _formatnode: function(node, elem)
    {
        var anchor = document.createElement("a");
        anchor.innerHTML = node.titleInfo().title;
        anchor.representedNode = node;
        elem.appendChild(anchor);
    },

    _formatobject: function(obj, elem)
    {
        elem.appendChild(document.createTextNode(Object.describe(obj)));
    },
}

WebInspector.ConsolePanel.prototype.__proto__ = WebInspector.Panel.prototype;

WebInspector.ConsoleMessage = function(source, level, message, line, url)
{
    this.source = source;
    this.level = level;
    this.message = message;
    this.line = line;
    this.url = url;
}

WebInspector.ConsoleMessage.prototype = {
    get shortURL()
    {
        if (this.resource)
            return this.resource.displayName;
        return this.url;
    },

    toListItem: function()
    {
        var item = document.createElement("li");
        item.className = "console-message";
        switch (this.source) {
            case WebInspector.ConsoleMessage.MessageSource.HTML:
                item.className += " console-html-source";
                break;
            case WebInspector.ConsoleMessage.MessageSource.XML:
                item.className += " console-xml-source";
                break;
            case WebInspector.ConsoleMessage.MessageSource.JS:
                item.className += " console-js-source";
                break;
            case WebInspector.ConsoleMessage.MessageSource.CSS:
                item.className += " console-css-source";
                break;
            case WebInspector.ConsoleMessage.MessageSource.Other:
                item.className += " console-other-source";
                break;
        }

        switch (this.level) {
            case WebInspector.ConsoleMessage.MessageLevel.Tip:
                item.className += " console-tip-level";
                break;
            case WebInspector.ConsoleMessage.MessageLevel.Log:
                item.className += " console-log-level";
                break;
            case WebInspector.ConsoleMessage.MessageLevel.Warning:
                item.className += " console-warning-level";
                break;
            case WebInspector.ConsoleMessage.MessageLevel.Error:
                item.className += " console-error-level";
        }

        var messageDiv = document.createElement("div");
        messageDiv.className = "console-message-message";
        messageDiv.textContent = this.message;
        item.appendChild(messageDiv);

        if (this.url && this.url !== "undefined") {
            var urlElement = document.createElement("a");
            urlElement.className = "console-message-url";

            if (this.line > 0)
                urlElement.textContent = WebInspector.UIString("%s (line %d)", this.url, this.line);
            else
                urlElement.textContent = this.url;

            item.appendChild(urlElement);
        }

        return item;
    },

    toString: function()
    {
        var sourceString;
        switch (this.source) {
            case WebInspector.ConsoleMessage.MessageSource.HTML:
                sourceString = "HTML";
                break;
            case WebInspector.ConsoleMessage.MessageSource.XML:
                sourceString = "XML";
                break;
            case WebInspector.ConsoleMessage.MessageSource.JS:
                sourceString = "JS";
                break;
            case WebInspector.ConsoleMessage.MessageSource.CSS:
                sourceString = "CSS";
                break;
            case WebInspector.ConsoleMessage.MessageSource.Other:
                sourceString = "Other";
                break;
        }

        var levelString;
        switch (this.level) {
            case WebInspector.ConsoleMessage.MessageLevel.Tip:
                levelString = "Tip";
                break;
            case WebInspector.ConsoleMessage.MessageLevel.Log:
                levelString = "Log";
                break;
            case WebInspector.ConsoleMessage.MessageLevel.Warning:
                levelString = "Warning";
                break;
            case WebInspector.ConsoleMessage.MessageLevel.Error:
                levelString = "Error";
                break;
        }

        return sourceString + " " + levelString + ": " + this.message + "\n" + this.url + " line " + this.line;
    }
}

// Note: Keep these constants in sync with the ones in Chrome.h
WebInspector.ConsoleMessage.MessageSource = {
    HTML: 0,
    XML: 1,
    JS: 2,
    CSS: 3,
    Other: 4,
};

WebInspector.ConsoleMessage.MessageLevel = {
    Tip: 0,
    Log: 1,
    Warning: 2,
    Error: 3,
};

WebInspector.ConsoleCommand = function(input, output)
{
    this.input = input;
    this.output = output;
}

WebInspector.ConsoleCommand.prototype = {
    toListItem: function()
    {
        var item = document.createElement("li");
        item.className = "console-command";

        var inputDiv = document.createElement("div");
        inputDiv.className = "console-command-input";
        inputDiv.textContent = this.input;
        item.appendChild(inputDiv);

        var outputDiv = document.createElement("div");
        outputDiv.className = "console-command-output";
        outputDiv.appendChild(this.output);
        item.appendChild(outputDiv);

        return item;
    }
}
