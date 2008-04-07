/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
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

    this.messagesElement = document.createElement("div");
    this.messagesElement.id = "console-messages";
    this.messagesElement.addEventListener("selectstart", this._messagesSelectStart.bind(this), true);
    this.messagesElement.addEventListener("click", this._messagesClicked.bind(this), true);
    this.element.appendChild(this.messagesElement);

    this.promptElement = document.createElement("div");
    this.promptElement.id = "console-prompt";
    this.promptElement.addEventListener("keydown", this._promptKeyDown.bind(this), false);
    this.promptElement.appendChild(document.createElement("br"));
    this.messagesElement.appendChild(this.promptElement);

    this.prompt = new WebInspector.TextPrompt(this.promptElement, this.completions.bind(this), " .=:[({;");

    this.clearButton = document.createElement("button");
    this.clearButton.title = WebInspector.UIString("Clear");
    this.clearButton.textContent = WebInspector.UIString("Clear");
    this.clearButton.addEventListener("click", this._clearButtonClicked.bind(this), false);
}

WebInspector.ConsolePanel.prototype = {
    show: function()
    {
        WebInspector.Panel.prototype.show.call(this);
        WebInspector.consoleListItem.select();

        this.clearButton.removeStyleClass("hidden");
        if (!this.clearButton.parentNode)
            document.getElementById("toolbarButtons").appendChild(this.clearButton);

        WebInspector.currentFocusElement = document.getElementById("main");

        function focusPrompt()
        {
            if (!this.prompt.isCaretInsidePrompt())
                this.prompt.moveCaretToEndOfPrompt();
        }

        setTimeout(focusPrompt.bind(this), 0);
    },

    hide: function()
    {
        WebInspector.Panel.prototype.hide.call(this);
        WebInspector.consoleListItem.deselect();
        this.clearButton.addStyleClass("hidden");
    },

    addMessage: function(msg)
    {
        if (msg.url in WebInspector.resourceURLMap) {
            msg.resource = WebInspector.resourceURLMap[msg.url];
            switch (msg.level) {
                case WebInspector.ConsoleMessage.MessageLevel.Warning:
                    ++msg.resource.warnings;
                    if (msg.resource.panel.addMessageToSource)
                        msg.resource.panel.addMessageToSource(msg);
                    break;
                case WebInspector.ConsoleMessage.MessageLevel.Error:
                    ++msg.resource.errors;
                    if (msg.resource.panel.addMessageToSource)
                        msg.resource.panel.addMessageToSource(msg);
                    break;
            }
        }

        this.messages.push(msg);

        var element = msg.toMessageElement();
        this.messagesElement.insertBefore(element, this.promptElement);
        this.promptElement.scrollIntoView(false);
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

        while (this.messagesElement.firstChild != this.promptElement)
            this.messagesElement.removeChild(this.messagesElement.firstChild);
    },

    completions: function(wordRange, bestMatchOnly)
    {
        // Pass less characters to scanBackwards so the range will be a more complete expression.
        var expression = this.prompt.scanBackwards(" =:{;", wordRange.startContainer, wordRange.startOffset);
        var expressionString = expression.toString();
        var lastIndex = expressionString.length - 1;

        var dotNotation = (expressionString[lastIndex] === ".");
        var bracketNotation = (expressionString[lastIndex] === "[");

        if (dotNotation || bracketNotation)
            expressionString = expressionString.substr(0, lastIndex);

        var prefix = wordRange.toString();
        if (!expressionString && !prefix)
            return;

        var result = InspectorController.inspectedWindow();
        if (expressionString) {
            try {
                result = this._evalInInspectedWindow(expressionString);
            } catch(e) {
                // Do nothing, the prefix will be considered a window property.
            }
        }

        if (bracketNotation) {
            if (prefix.length && prefix[0] === "'")
                var quoteUsed = "'";
            else
                var quoteUsed = "\"";
        }

        var results = [];
        var properties = Object.sortedProperties(result);
        for (var i = 0; i < properties.length; ++i) {
            var property = properties[i];
            if (bracketNotation)
                property = quoteUsed + property.escapeCharacters(quoteUsed + "\\") + quoteUsed + "]";
            if (property.length < prefix.length)
                continue;
            if (property.indexOf(prefix) !== 0)
                continue;
            results.push(property);
            if (bestMatchOnly)
                break;
        }

        return results;
    },

    _clearButtonClicked: function()
    {
        this.clearMessages();
    },

    _messagesSelectStart: function(event)
    {
        if (this._selectionTimeout)
            clearTimeout(this._selectionTimeout);

        this.prompt.clearAutoComplete();

        function moveBackIfOutside()
        {
            delete this._selectionTimeout;
            if (!this.prompt.isCaretInsidePrompt() && window.getSelection().isCollapsed)
                this.prompt.moveCaretToEndOfPrompt();
            this.prompt.autoCompleteSoon();
        }

        this._selectionTimeout = setTimeout(moveBackIfOutside.bind(this), 100);
    },

    _messagesClicked: function(event)
    {
        var link = event.target.enclosingNodeOrSelfWithNodeName("a");
        if (link && link.representedNode) {
            WebInspector.updateFocusedNode(link.representedNode);
            return;
        }

        var messageElement = event.target.enclosingNodeOrSelfWithClass("console-message");
        if (!messageElement)
            return;

        if (!messageElement.message)
            return;

        var resource = messageElement.message.resource;
        if (!resource)
            return;

        if (link && link.hasStyleClass("console-message-url")) {
            WebInspector.navigateToResource(resource);
            resource.panel.showSourceLine(item.message.line);
        }

        event.stopPropagation();
        event.preventDefault();
    },

    _promptKeyDown: function(event)
    {
        switch (event.keyIdentifier) {
            case "Enter":
                this._enterKeyPressed(event);
                return;
        }

        this.prompt.handleKeyEvent(event);
    },

    _evalInInspectedWindow: function(expression)
    {
        return InspectorController.inspectedWindow().eval(expression);
    },

    _enterKeyPressed: function(event)
    {
        event.preventDefault();
        event.stopPropagation();

        this.prompt.clearAutoComplete(true);

        var str = this.prompt.text;
        if (!str.length)
            return;

        var result;
        var exception = false;
        try {
            result = this._evalInInspectedWindow(str);
        } catch(e) {
            result = e;
            exception = true;
        }

        this.prompt.history.push(str);
        this.prompt.historyOffset = 0;
        this.prompt.text = "";

        var level = exception ? WebInspector.ConsoleMessage.MessageLevel.Error : WebInspector.ConsoleMessage.MessageLevel.Log;
        this.addMessage(new WebInspector.ConsoleCommand(str, result, this._format(result), level));
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
        anchor.innerHTML = nodeTitleInfo.call(node).title;
        anchor.representedNode = node;
        anchor.addEventListener("mouseover", function() { InspectorController.highlightDOMNode(node) }, false);
        anchor.addEventListener("mouseout", function() { InspectorController.hideDOMNodeHighlight() }, false);
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

    toMessageElement: function()
    {
        var element = document.createElement("div");
        element.message = this;
        element.className = "console-message";

        switch (this.source) {
            case WebInspector.ConsoleMessage.MessageSource.HTML:
                element.addStyleClass("console-html-source");
                break;
            case WebInspector.ConsoleMessage.MessageSource.XML:
                element.addStyleClass("console-xml-source");
                break;
            case WebInspector.ConsoleMessage.MessageSource.JS:
                element.addStyleClass("console-js-source");
                break;
            case WebInspector.ConsoleMessage.MessageSource.CSS:
                element.addStyleClass("console-css-source");
                break;
            case WebInspector.ConsoleMessage.MessageSource.Other:
                element.addStyleClass("console-other-source");
                break;
        }

        switch (this.level) {
            case WebInspector.ConsoleMessage.MessageLevel.Tip:
                element.addStyleClass("console-tip-level");
                break;
            case WebInspector.ConsoleMessage.MessageLevel.Log:
                element.addStyleClass("console-log-level");
                break;
            case WebInspector.ConsoleMessage.MessageLevel.Warning:
                element.addStyleClass("console-warning-level");
                break;
            case WebInspector.ConsoleMessage.MessageLevel.Error:
                element.addStyleClass("console-error-level");
        }

        var messageTextElement = document.createElement("span");
        messageTextElement.className = "console-message-text";
        messageTextElement.textContent = this.message;
        element.appendChild(messageTextElement);

        element.appendChild(document.createTextNode(" "));

        if (this.url && this.url !== "undefined") {
            var urlElement = document.createElement("a");
            urlElement.className = "console-message-url";

            if (this.line > 0)
                urlElement.textContent = WebInspector.UIString("%s (line %d)", this.url, this.line);
            else
                urlElement.textContent = this.url;

            element.appendChild(urlElement);
        }

        return element;
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
}

WebInspector.ConsoleMessage.MessageLevel = {
    Tip: 0,
    Log: 1,
    Warning: 2,
    Error: 3
}

WebInspector.ConsoleCommand = function(command, result, formattedResultElement, level)
{
    this.command = command;
    this.formattedResultElement = formattedResultElement;
    this.level = level;
}

WebInspector.ConsoleCommand.prototype = {
    toMessageElement: function()
    {
        var element = document.createElement("div");
        element.command = this;
        element.className = "console-user-command";

        var commandTextElement = document.createElement("span");
        commandTextElement.className = "console-message-text";
        commandTextElement.textContent = this.command;
        element.appendChild(commandTextElement);

        var resultElement = document.createElement("div");
        resultElement.className = "console-message";
        element.appendChild(resultElement);

        switch (this.level) {
            case WebInspector.ConsoleMessage.MessageLevel.Log:
                resultElement.addStyleClass("console-log-level");
                break;
            case WebInspector.ConsoleMessage.MessageLevel.Warning:
                resultElement.addStyleClass("console-warning-level");
                break;
            case WebInspector.ConsoleMessage.MessageLevel.Error:
                resultElement.addStyleClass("console-error-level");
        }

        var resultTextElement = document.createElement("span");
        resultTextElement.className = "console-message-text";
        resultTextElement.appendChild(this.formattedResultElement);
        resultElement.appendChild(resultTextElement);

        return element;
    }
}
