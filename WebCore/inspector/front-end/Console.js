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

    this.topGroup = new WebInspector.ConsoleGroup(null, 0);
    this.messagesElement.insertBefore(this.topGroup.element, this.promptElement);
    this.groupLevel = 0;
    this.currentGroup = this.topGroup;

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
        if (msg instanceof WebInspector.ConsoleMessage && !(msg instanceof WebInspector.ConsoleCommandResult)) {
            msg.totalRepeatCount = msg.repeatCount;
            msg.repeatDelta = msg.repeatCount;

            var messageRepeated = false;

            if (msg.isEqual && msg.isEqual(this.previousMessage)) {
                // Because sometimes we get a large number of repeated messages and sometimes
                // we get them one at a time, we need to know the difference between how many
                // repeats we used to have and how many we have now.
                msg.repeatDelta -= this.previousMessage.totalRepeatCount;

                if (!isNaN(this.repeatCountBeforeCommand))
                    msg.repeatCount -= this.repeatCountBeforeCommand;

                if (!this.commandSincePreviousMessage) {
                    // Recreate the previous message element to reset the repeat count.
                    var messagesElement = this.currentGroup.messagesElement;
                    messagesElement.removeChild(messagesElement.lastChild);
                    messagesElement.appendChild(msg.toMessageElement());

                    messageRepeated = true;
                }
            } else
                delete this.repeatCountBeforeCommand;

            // Increment the error or warning count
            switch (msg.level) {
            case WebInspector.ConsoleMessage.MessageLevel.Warning:
                WebInspector.warnings += msg.repeatDelta;
                break;
            case WebInspector.ConsoleMessage.MessageLevel.Error:
                WebInspector.errors += msg.repeatDelta;
                break;
            }

            // Add message to the resource panel
            if (msg.url in WebInspector.resourceURLMap) {
                msg.resource = WebInspector.resourceURLMap[msg.url];
                if (WebInspector.panels.resources)
                    WebInspector.panels.resources.addMessageToResource(msg.resource, msg);
            }

            this.commandSincePreviousMessage = false;
            this.previousMessage = msg;

            if (messageRepeated)
                return;
        } else if (msg instanceof WebInspector.ConsoleCommand) {
            if (this.previousMessage) {
                this.commandSincePreviousMessage = true;
                this.repeatCountBeforeCommand = this.previousMessage.totalRepeatCount;
            }
        }

        this.messages.push(msg);

        if (msg.level === WebInspector.ConsoleMessage.MessageLevel.EndGroup) {
            if (this.groupLevel < 1)
                return;

            this.groupLevel--;

            this.currentGroup = this.currentGroup.parentGroup;
        } else {
            if (msg.level === WebInspector.ConsoleMessage.MessageLevel.StartGroup) {
                this.groupLevel++;

                var group = new WebInspector.ConsoleGroup(this.currentGroup, this.groupLevel);
                this.currentGroup.messagesElement.appendChild(group.element);
                this.currentGroup = group;
            }

            this.currentGroup.addMessage(msg);
        }

        this.promptElement.scrollIntoView(false);
    },

    clearMessages: function(clearInspectorController)
    {
        if (clearInspectorController)
            InspectorController.clearMessages();
        if (WebInspector.panels.resources)
            WebInspector.panels.resources.clearMessages();

        this.messages = [];

        this.groupLevel = 0;
        this.currentGroup = this.topGroup;
        this.topGroup.messagesElement.removeChildren();

        WebInspector.errors = 0;
        WebInspector.warnings = 0;

        delete this.commandSincePreviousMessage;
        delete this.repeatCountBeforeCommand;
        delete this.previousMessage;
    },

    completions: function(wordRange, bestMatchOnly)
    {
        // Pass less stop characters to rangeOfWord so the range will be a more complete expression.
        const expressionStopCharacters = " =:{;";
        var expressionRange = wordRange.startContainer.rangeOfWord(wordRange.startOffset, expressionStopCharacters, this.promptElement, "backward");
        var expressionString = expressionRange.toString();
        var lastIndex = expressionString.length - 1;

        var dotNotation = (expressionString[lastIndex] === ".");
        var bracketNotation = (expressionString[lastIndex] === "[");

        if (dotNotation || bracketNotation)
            expressionString = expressionString.substr(0, lastIndex);

        var prefix = wordRange.toString();
        if (!expressionString && !prefix)
            return;

        var result;
        if (expressionString) {
            try {
                result = this._evalInInspectedWindow(expressionString);
            } catch(e) {
                // Do nothing, the prefix will be considered a window property.
            }
        } else {
            // There is no expressionString, so the completion should happen against global properties.
            // Or if the debugger is paused, against properties in scope of the selected call frame.
            if (WebInspector.panels.scripts && WebInspector.panels.scripts.paused)
                result = WebInspector.panels.scripts.variablesInScopeForSelectedCallFrame();
            else
                result = InspectorController.inspectedWindow();
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

            if (dotNotation && !/^[a-zA-Z_$][a-zA-Z0-9_$]*$/.test(property))
                continue;

            if (bracketNotation) {
                if (!/^[0-9]+$/.test(property))
                    property = quoteUsed + property.escapeCharacters(quoteUsed + "\\") + quoteUsed;
                property += "]";
            }

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
        this.clearMessages(true);
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
        if (WebInspector.panels.scripts && WebInspector.panels.scripts.paused)
            return WebInspector.panels.scripts.evaluateInSelectedCallFrame(expression);

        var inspectedWindow = InspectorController.inspectedWindow();
        if (!inspectedWindow._inspectorCommandLineAPI) {
            inspectedWindow.eval("window._inspectorCommandLineAPI = { \
                $: function() { return document.getElementById.apply(document, arguments) }, \
                $$: function() { return document.querySelectorAll.apply(document, arguments) }, \
                $x: function(xpath, context) { \
                    var nodes = []; \
                    try { \
                        var doc = context || document; \
                        var results = doc.evaluate(xpath, doc, null, XPathResult.ANY_TYPE, null); \
                        var node; \
                        while (node = results.iterateNext()) nodes.push(node); \
                    } catch (e) {} \
                    return nodes; \
                }, \
                dir: function() { return console.dir.apply(console, arguments) }, \
                dirxml: function() { return console.dirxml.apply(console, arguments) }, \
                keys: function(o) { var a = []; for (k in o) a.push(k); return a; }, \
                values: function(o) { var a = []; for (k in o) a.push(o[k]); return a; }, \
                profile: function() { return console.profile.apply(console, arguments) }, \
                profileEnd: function() { return console.profileEnd.apply(console, arguments) } \
            };");

            inspectedWindow._inspectorCommandLineAPI.clear = InspectorController.wrapCallback(this.clearMessages.bind(this));
        }

        // Surround the expression in with statements to inject our command line API so that
        // the window object properties still take more precedent than our API functions.
        expression = "with (window._inspectorCommandLineAPI) { with (window) { " + expression + " } }";

        return inspectedWindow.eval(expression);
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

        var commandMessage = new WebInspector.ConsoleCommand(str);
        this.addMessage(commandMessage);

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

        this.addMessage(new WebInspector.ConsoleCommandResult(result, exception, commandMessage));
    },

    _format: function(output, forceObjectFormat)
    {
        var type = (forceObjectFormat ? "object" : Object.type(output, InspectorController.inspectedWindow()));

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
        if (forceObjectFormat)
            formatter = "_formatobject";
        else if (type in undecoratedTypes)
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
        var treeOutline = new WebInspector.ElementsTreeOutline();
        treeOutline.rootDOMNode = node;
        treeOutline.element.addStyleClass("outline-disclosure");
        if (!treeOutline.children[0].hasChildren)
            treeOutline.element.addStyleClass("single-node");
        elem.appendChild(treeOutline.element);
    },

    _formatobject: function(obj, elem)
    {
        elem.appendChild(new WebInspector.ObjectPropertiesSection(obj, null, null, null, true).element);
    },

    _formaterror: function(obj, elem)
    {
        var messageElement = document.createElement("span");
        messageElement.className = "error-message";
        messageElement.textContent = obj.name + ": " + obj.message;
        elem.appendChild(messageElement);

        if (obj.sourceURL) {
            var urlElement = document.createElement("a");
            urlElement.className = "webkit-html-resource-link";
            urlElement.href = obj.sourceURL;
            urlElement.lineNumber = obj.line;
            urlElement.preferredPanel = "scripts";

            if (obj.line > 0)
                urlElement.textContent = WebInspector.displayNameForURL(obj.sourceURL) + ":" + obj.line;
            else
                urlElement.textContent = WebInspector.displayNameForURL(obj.sourceURL);

            elem.appendChild(document.createTextNode(" ("));
            elem.appendChild(urlElement);
            elem.appendChild(document.createTextNode(")"));
        }
    },
}

WebInspector.Console.prototype.__proto__ = WebInspector.View.prototype;

WebInspector.ConsoleMessage = function(source, level, line, url, groupLevel, repeatCount)
{
    this.source = source;
    this.level = level;
    this.line = line;
    this.url = url;
    this.groupLevel = groupLevel;
    this.repeatCount = repeatCount;

    switch (this.level) {
        case WebInspector.ConsoleMessage.MessageLevel.Trace:
            var span = document.createElement("span");
            span.addStyleClass("console-formatted-trace");
            var stack = Array.prototype.slice.call(arguments, 6);
            var funcNames = stack.map(function(f) {
                return f || WebInspector.UIString("(anonymous function)");
            });
            span.appendChild(document.createTextNode(funcNames.join("\n")));
            this.formattedMessage = span;
            break;
        case WebInspector.ConsoleMessage.MessageLevel.Object:
            this.formattedMessage = this._format(["%O", arguments[6]]);
            break;
        default:
            this.formattedMessage = this._format(Array.prototype.slice.call(arguments, 6));
            break;
    }

    // This is used for inline message bubbles in SourceFrames, or other plain-text representations.
    this.message = this.formattedMessage.textContent;
}

WebInspector.ConsoleMessage.prototype = {
    isErrorOrWarning: function()
    {
        return (this.level === WebInspector.ConsoleMessage.MessageLevel.Warning || this.level === WebInspector.ConsoleMessage.MessageLevel.Error);
    },

    _format: function(parameters)
    {
        var formattedResult = document.createElement("span");

        if (!parameters.length)
            return formattedResult;

        function formatForConsole(obj)
        {
            return WebInspector.console._format(obj);
        }

        function formatAsObjectForConsole(obj)
        {
            return WebInspector.console._format(obj, true);
        }

        if (Object.type(parameters[0], InspectorController.inspectedWindow()) === "string") {
            var formatters = {}
            for (var i in String.standardFormatters)
                formatters[i] = String.standardFormatters[i];

            // Firebug uses %o for formatting objects.
            formatters.o = formatForConsole;
            // Firebug allows both %i and %d for formatting integers.
            formatters.i = formatters.d;
            // Support %O to force object formating, instead of the type-based %o formatting.
            formatters.O = formatAsObjectForConsole;

            function append(a, b)
            {
                if (!(b instanceof Node))
                    a.appendChild(WebInspector.linkifyStringAsFragment(b.toString()));
                else
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
            if (typeof parameters[i] === "string")
                formattedResult.appendChild(WebInspector.linkifyStringAsFragment(parameters[i]));
            else
                formattedResult.appendChild(formatForConsole(parameters[i]));

            if (i < parameters.length - 1)
                formattedResult.appendChild(document.createTextNode(" "));
        }

        return formattedResult;
    },

    toMessageElement: function()
    {
        if (this.propertiesSection)
            return this.propertiesSection.element;

        var element = document.createElement("div");
        element.message = this;
        element.className = "console-message";

        switch (this.source) {
            case WebInspector.ConsoleMessage.MessageSource.HTML:
                element.addStyleClass("console-html-source");
                break;
            case WebInspector.ConsoleMessage.MessageSource.WML:
                element.addStyleClass("console-wml-source");
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
                break;
            case WebInspector.ConsoleMessage.MessageLevel.StartGroup:
                element.addStyleClass("console-group-title-level");
        }

        if (this.elementsTreeOutline) {
            element.addStyleClass("outline-disclosure");
            element.appendChild(this.elementsTreeOutline.element);
            return element;
        }

        if (this.repeatCount > 1) {
            var messageRepeatCountElement = document.createElement("span");
            messageRepeatCountElement.className = "bubble";
            messageRepeatCountElement.textContent = this.repeatCount;

            element.appendChild(messageRepeatCountElement);
            element.addStyleClass("repeated-message");
        }

        if (this.url && this.url !== "undefined") {
            var urlElement = document.createElement("a");
            urlElement.className = "console-message-url webkit-html-resource-link";
            urlElement.href = this.url;
            urlElement.lineNumber = this.line;

            if (this.source === WebInspector.ConsoleMessage.MessageSource.JS)
                urlElement.preferredPanel = "scripts";

            if (this.line > 0)
                urlElement.textContent = WebInspector.displayNameForURL(this.url) + ":" + this.line;
            else
                urlElement.textContent = WebInspector.displayNameForURL(this.url);

            element.appendChild(urlElement);
        }

        var messageTextElement = document.createElement("span");
        messageTextElement.className = "console-message-text";
        messageTextElement.appendChild(this.formattedMessage);
        element.appendChild(messageTextElement);

        return element;
    },

    toString: function()
    {
        var sourceString;
        switch (this.source) {
            case WebInspector.ConsoleMessage.MessageSource.HTML:
                sourceString = "HTML";
                break;
            case WebInspector.ConsoleMessage.MessageSource.WML:
                sourceString = "WML";
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
            case WebInspector.ConsoleMessage.MessageLevel.Object:
                levelString = "Object";
                break;
            case WebInspector.ConsoleMessage.MessageLevel.Trace:
                levelString = "Trace";
                break;
            case WebInspector.ConsoleMessage.MessageLevel.StartGroup:
                levelString = "Start Group";
                break;
            case WebInspector.ConsoleMessage.MessageLevel.EndGroup:
                levelString = "End Group";
                break;
        }

        return sourceString + " " + levelString + ": " + this.formattedMessage.textContent + "\n" + this.url + " line " + this.line;
    },

    isEqual: function(msg, disreguardGroup)
    {
        if (!msg)
            return false;

        var ret = (this.source == msg.source)
            && (this.level == msg.level)
            && (this.line == msg.line)
            && (this.url == msg.url)
            && (this.message == msg.message);

        return (disreguardGroup ? ret : (ret && (this.groupLevel == msg.groupLevel)));
    }
}

// Note: Keep these constants in sync with the ones in Console.h
WebInspector.ConsoleMessage.MessageSource = {
    HTML: 0,
    WML: 1,
    XML: 2,
    JS: 3,
    CSS: 4,
    Other: 5
}

WebInspector.ConsoleMessage.MessageLevel = {
    Tip: 0,
    Log: 1,
    Warning: 2,
    Error: 3,
    Object: 4,
    Trace: 5,
    StartGroup: 6,
    EndGroup: 7
}

WebInspector.ConsoleCommand = function(command)
{
    this.command = command;
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

        return element;
    }
}

WebInspector.ConsoleCommandResult = function(result, exception, originatingCommand)
{
    var level = (exception ? WebInspector.ConsoleMessage.MessageLevel.Error : WebInspector.ConsoleMessage.MessageLevel.Log);
    var message = (exception ? String(result) : result);
    var line = (exception ? result.line : -1);
    var url = (exception ? result.sourceURL : null);

    WebInspector.ConsoleMessage.call(this, WebInspector.ConsoleMessage.MessageSource.JS, level, line, url, null, 1, message);

    this.originatingCommand = originatingCommand;
}

WebInspector.ConsoleCommandResult.prototype = {
    toMessageElement: function()
    {
        var element = WebInspector.ConsoleMessage.prototype.toMessageElement.call(this);
        element.addStyleClass("console-user-command-result");
        return element;
    }
}

WebInspector.ConsoleCommandResult.prototype.__proto__ = WebInspector.ConsoleMessage.prototype;

WebInspector.ConsoleGroup = function(parentGroup, level)
{
    this.parentGroup = parentGroup;
    this.level = level;

    var element = document.createElement("div");
    element.className = "console-group";
    element.group = this;
    this.element = element;

    var messagesElement = document.createElement("div");
    messagesElement.className = "console-group-messages";
    element.appendChild(messagesElement);
    this.messagesElement = messagesElement;
}

WebInspector.ConsoleGroup.prototype = {
    addMessage: function(msg)
    {
        var element = msg.toMessageElement();
        
        if (msg.level === WebInspector.ConsoleMessage.MessageLevel.StartGroup) {
            this.messagesElement.parentNode.insertBefore(element, this.messagesElement);
            element.addEventListener("click", this._titleClicked.bind(this), true);
        } else
            this.messagesElement.appendChild(element);

        if (element.previousSibling && msg.originatingCommand && element.previousSibling.command === msg.originatingCommand)
            element.previousSibling.addStyleClass("console-adjacent-user-command-result");
    },

    _titleClicked: function(event)
    {
        var groupTitleElement = event.target.enclosingNodeOrSelfWithClass("console-group-title-level");
        if (groupTitleElement) {
            var groupElement = groupTitleElement.enclosingNodeOrSelfWithClass("console-group");
            if (groupElement)
                if (groupElement.hasStyleClass("collapsed"))
                    groupElement.removeStyleClass("collapsed");
                else
                    groupElement.addStyleClass("collapsed");
            groupTitleElement.scrollIntoViewIfNeeded(true);
        }

        event.stopPropagation();
        event.preventDefault();
    }
}
