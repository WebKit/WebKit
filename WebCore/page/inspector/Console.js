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

WebInspector.Console = function()
{
    this.messages = [];

    WebInspector.View.call(this, document.getElementById("console"));

    this.messagesElement = document.getElementById("console-messages");
    this.messagesElement.addEventListener("selectstart", this._messagesSelectStart.bind(this), false);
    this.messagesElement.addEventListener("click", this._messagesClicked.bind(this), true);

    this.promptElement = document.getElementById("console-prompt");
    this.promptElement.handleKeyEvent = this._promptKeyDown.bind(this);
    this.prompt = new WebInspector.TextPrompt(this.promptElement, this.completions.bind(this), " .=:[({;");

    this.toggleButton = document.getElementById("console-status-bar-item");
    this.toggleButton.title = WebInspector.UIString("Show console.");
    this.toggleButton.addEventListener("click", this._toggleButtonClicked.bind(this), false);

    this.clearButton = document.getElementById("clear-console-status-bar-item");
    this.clearButton.title = WebInspector.UIString("Clear console log.");
    this.clearButton.addEventListener("click", this._clearButtonClicked.bind(this), false);

    document.getElementById("main-status-bar").addEventListener("mousedown", this._startStatusBarDragging.bind(this), true);
}

WebInspector.Console.prototype = {
    show: function()
    {
        if (this._animating || this.visible)
            return;

        WebInspector.View.prototype.show.call(this);

        this._animating = true;

        this.toggleButton.addStyleClass("toggled-on");
        this.toggleButton.title = WebInspector.UIString("Hide console.");

        document.body.addStyleClass("console-visible");

        var anchoredItems = document.getElementById("anchored-status-bar-items");

        var animations = [
            {element: document.getElementById("main"), end: {bottom: this.element.offsetHeight}},
            {element: document.getElementById("main-status-bar"), start: {"padding-left": anchoredItems.offsetWidth - 1}, end: {"padding-left": 0}},
            {element: document.getElementById("other-console-status-bar-items"), start: {opacity: 0}, end: {opacity: 1}}
        ];

        var consoleStatusBar = document.getElementById("console-status-bar");
        consoleStatusBar.insertBefore(anchoredItems, consoleStatusBar.firstChild);

        function animationFinished()
        {
            if ("updateStatusBarItems" in WebInspector.currentPanel)
                WebInspector.currentPanel.updateStatusBarItems();
            WebInspector.currentFocusElement = this.promptElement;
            delete this._animating;
        }

        WebInspector.animateStyle(animations, window.event && window.event.shiftKey ? 2000 : 250, animationFinished.bind(this));

        if (!this.prompt.isCaretInsidePrompt())
            this.prompt.moveCaretToEndOfPrompt();
    },

    hide: function()
    {
        if (this._animating || !this.visible)
            return;

        WebInspector.View.prototype.hide.call(this);

        this._animating = true;

        this.toggleButton.removeStyleClass("toggled-on");
        this.toggleButton.title = WebInspector.UIString("Show console.");

        if (this.element === WebInspector.currentFocusElement || this.element.isAncestor(WebInspector.currentFocusElement))
            WebInspector.currentFocusElement = WebInspector.previousFocusElement;

        var anchoredItems = document.getElementById("anchored-status-bar-items");

        // Temporally set properties and classes to mimic the post-animation values so panels
        // like Elements in their updateStatusBarItems call will size things to fit the final location.
        document.getElementById("main-status-bar").style.setProperty("padding-left", (anchoredItems.offsetWidth - 1) + "px");
        document.body.removeStyleClass("console-visible");
        if ("updateStatusBarItems" in WebInspector.currentPanel)
            WebInspector.currentPanel.updateStatusBarItems();
        document.body.addStyleClass("console-visible");

        var animations = [
            {element: document.getElementById("main"), end: {bottom: 0}},
            {element: document.getElementById("main-status-bar"), start: {"padding-left": 0}, end: {"padding-left": anchoredItems.offsetWidth - 1}},
            {element: document.getElementById("other-console-status-bar-items"), start: {opacity: 1}, end: {opacity: 0}}
        ];

        function animationFinished()
        {
            var mainStatusBar = document.getElementById("main-status-bar");
            mainStatusBar.insertBefore(anchoredItems, mainStatusBar.firstChild);
            mainStatusBar.style.removeProperty("padding-left");
            document.body.removeStyleClass("console-visible");
            delete this._animating;
        }

        WebInspector.animateStyle(animations, window.event && window.event.shiftKey ? 2000 : 250, animationFinished.bind(this));
    },

    addMessage: function(msg)
    {
        if (msg.url in WebInspector.resourceURLMap) {
            msg.resource = WebInspector.resourceURLMap[msg.url];
            WebInspector.panels.resources.addMessageToResource(msg.resource, msg);
        }

        this.messages.push(msg);

        var element = msg.toMessageElement();
        this.messagesElement.insertBefore(element, this.promptElement);
        this.promptElement.scrollIntoView(false);
    },

    clearMessages: function()
    {
        WebInspector.panels.resources.clearMessages();

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

    _toggleButtonClicked: function()
    {
        this.visible = !this.visible;
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
        if (!link || !link.representedNode)
            return;

        WebInspector.updateFocusedNode(link.representedNode);
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

    _startStatusBarDragging: function(event)
    {
        if (!this.visible || event.target !== document.getElementById("main-status-bar"))
            return;

        WebInspector.elementDragStart(document.getElementById("main-status-bar"), this._statusBarDragging.bind(this), this._endStatusBarDragging.bind(this), event, "row-resize");

        this._statusBarDragOffset = event.pageY - this.element.totalOffsetTop;

        event.stopPropagation();
    },

    _statusBarDragging: function(event)
    {
        var mainElement = document.getElementById("main");

        var height = window.innerHeight - event.pageY + this._statusBarDragOffset;
        height = Number.constrain(height, Preferences.minConsoleHeight, window.innerHeight - mainElement.totalOffsetTop - Preferences.minConsoleHeight);

        mainElement.style.bottom = height + "px";
        this.element.style.height = height + "px";

        event.preventDefault();
        event.stopPropagation();
    },

    _endStatusBarDragging: function(event)
    {
        WebInspector.elementDragEnd(event);

        delete this._statusBarDragOffset;

        event.stopPropagation();
    },

    _evalInInspectedWindow: function(expression)
    {
        if (WebInspector.panels.scripts.paused)
            return WebInspector.panels.scripts.evaluateInSelectedCallFrame(expression);
        return InspectorController.inspectedWindow().eval(expression);
    },

    _enterKeyPressed: function(event)
    {
        if (event.altKey)
            return;

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

    _format: function(output, plainText)
    {
        var type = Object.type(output, InspectorController.inspectedWindow());
        if (type === "object") {
            if (output instanceof InspectorController.inspectedWindow().Node)
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
        this[formatter](output, span, plainText);
        return span;
    },

    _formatvalue: function(val, elem, plainText)
    {
        // Honor the plainText argument, if the textContent output doesn't make sense.
        elem.appendChild(document.createTextNode(val));
    },

    _formatstring: function(str, elem, plainText)
    {
        // Honor the plainText argument, if the textContent output doesn't make sense.
        elem.appendChild(document.createTextNode("\"" + str + "\""));
    },

    _formatregexp: function(re, elem, plainText)
    {
        // Honor the plainText argument, if the textContent output doesn't make sense.
        var formatted = String(re).replace(/([\\\/])/g, "\\$1").replace(/\\(\/[gim]*)$/, "$1").substring(1);
        elem.appendChild(document.createTextNode(formatted));
    },

    _formatarray: function(arr, elem, plainText)
    {
        // Honor the plainText argument, if the textContent output doesn't make sense.
        // Especially if expanding array values is added.
        elem.appendChild(document.createTextNode("["));
        for (var i = 0; i < arr.length; ++i) {
            elem.appendChild(this._format(arr[i]));
            if (i < arr.length - 1)
                elem.appendChild(document.createTextNode(", "));
        }
        elem.appendChild(document.createTextNode("]"));
    },

    _formatnode: function(node, elem, plainText)
    {
        // Honor the plainText argument, if the textContent output doesn't make sense.
        // Especially if expanding to show children is added.
        var anchor = document.createElement("a");
        anchor.innerHTML = nodeTitleInfo.call(node).title;
        anchor.representedNode = node;
        anchor.addEventListener("mouseover", function() { InspectorController.highlightDOMNode(node) }, false);
        anchor.addEventListener("mouseout", function() { InspectorController.hideDOMNodeHighlight() }, false);
        elem.appendChild(anchor);
    },

    _formatobject: function(obj, elem, plainText)
    {
        // Honor the plainText argument, if the textContent output doesn't make sense.
        // Especially if object properties are added.
        elem.appendChild(document.createTextNode(Object.describe(obj)));
    },

    _formaterror: function(obj, elem, plainText)
    {
        // Honor the plainText argument, if the textContent output doesn't make sense.
        elem.appendChild(document.createTextNode(obj.name + ": " + obj.message + " "));

        if (obj.sourceURL) {
            var urlElement = document.createElement("a");
            urlElement.className = "console-message-url webkit-html-resource-link";
            urlElement.href = obj.sourceURL;
            urlElement.lineNumber = obj.line;
            urlElement.preferredPanel = "scripts";

            if (obj.line > 0)
                urlElement.textContent = WebInspector.UIString("%s (line %d)", obj.sourceURL, obj.line);
            else
                urlElement.textContent = obj.sourceURL;

            elem.appendChild(urlElement);
        }
    },
}

WebInspector.Console.prototype.__proto__ = WebInspector.View.prototype;

WebInspector.ConsoleMessage = function(source, level, line, url)
{
    this.source = source;
    this.level = level;
    this.line = line;
    this.url = url;

    // This _format call passes in true for the plainText argument. The result's textContent is
    // used for inline message bubbles in SourceFrames, or other plain-text representations.
    this.message = this._format(Array.prototype.slice.call(arguments, 4), true).textContent;

    // The formatedMessage property is used for the rich and interactive console.
    this.formattedMessage = this._format(Array.prototype.slice.call(arguments, 4));
}

WebInspector.ConsoleMessage.prototype = {
    _format: function(parameters, plainText)
    {
        var formattedResult = document.createElement("span");

        if (!parameters.length)
            return formattedResult;

        function formatForConsole(obj)
        {
            return WebInspector.console._format(obj, plainText);
        }

        if (Object.type(parameters[0], InspectorController.inspectedWindow()) === "string") {
            var formatters = {}
            for (var i in String.standardFormatters)
                formatters[i] = String.standardFormatters[i];

            // Firebug uses %o for formatting objects.
            formatters.o = formatForConsole;
            // Firebug allows both %i and %d for formatting integers.
            formatters.i = formatters.d;

            function append(a, b)
            {
                if (!(b instanceof Node))
                    b = document.createTextNode(b);
                a.appendChild(b);
                return a;
            }

            var result = String.format(parameters[0], parameters.slice(1), formatters, formattedResult, append);
            formattedResult = result.formattedResult;
            parameters = result.unusedSubstitutions;
            if (parameters.length)
                formattedResult.appendChild(document.createTextNode(" "));
        }

        for (var i = 0; i < parameters.length; ++i) {
            formattedResult.appendChild(formatForConsole(parameters[i]));
            if (i < parameters.length - 1)
                formattedResult.appendChild(document.createTextNode(" "));
        }

        return formattedResult;
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
        messageTextElement.appendChild(this.formattedMessage);
        element.appendChild(messageTextElement);

        element.appendChild(document.createTextNode(" "));

        if (this.url && this.url !== "undefined") {
            var urlElement = document.createElement("a");
            urlElement.className = "console-message-url webkit-html-resource-link";
            urlElement.href = this.url;
            urlElement.lineNumber = this.line;

            if (this.source === WebInspector.ConsoleMessage.MessageSource.JS)
                urlElement.preferredPanel = "scripts";

            if (this.line > 0)
                urlElement.textContent = WebInspector.UIString("%s (line %d)", WebInspector.displayNameForURL(this.url), this.line);
            else
                urlElement.textContent = WebInspector.displayNameForURL(this.url);

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

        return sourceString + " " + levelString + ": " + this.formattedMessage.textContent + "\n" + this.url + " line " + this.line;
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
