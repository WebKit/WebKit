/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2009 Joseph Pecoraro
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

WebInspector.ConsoleView = function(drawer)
{
    WebInspector.View.call(this, document.getElementById("console-view"));

    this.messages = [];
    this.drawer = drawer;

    this.clearButton = document.getElementById("clear-console-status-bar-item");
    this.clearButton.title = WebInspector.UIString("Clear console log.");
    this.clearButton.addEventListener("click", this._clearButtonClicked.bind(this), false);

    this.messagesElement = document.getElementById("console-messages");
    this.messagesElement.addEventListener("selectstart", this._messagesSelectStart.bind(this), false);
    this.messagesElement.addEventListener("click", this._messagesClicked.bind(this), true);

    this.promptElement = document.getElementById("console-prompt");
    this.promptElement.handleKeyEvent = this._promptKeyDown.bind(this);
    this.prompt = new WebInspector.TextPrompt(this.promptElement, this.completions.bind(this), " .=:[({;");

    this.topGroup = new WebInspector.ConsoleGroup(null, 0);
    this.messagesElement.insertBefore(this.topGroup.element, this.promptElement);
    this.groupLevel = 0;
    this.currentGroup = this.topGroup;

    this.toggleConsoleButton = document.getElementById("console-status-bar-item");
    this.toggleConsoleButton.title = WebInspector.UIString("Show console.");
    this.toggleConsoleButton.addEventListener("click", this._toggleConsoleButtonClicked.bind(this), false);

    var anchoredStatusBar = document.getElementById("anchored-status-bar-items");
    anchoredStatusBar.appendChild(this.toggleConsoleButton);
    
    // Will hold the list of filter elements
    this.filterBarElement = document.getElementById("console-filter");
    
    function createDividerElement() {
        var dividerElement = document.createElement("div");
        
        dividerElement.addStyleClass("divider");
        
        this.filterBarElement.appendChild(dividerElement);
    }
    
    function createFilterElement(category) {
        var categoryElement = document.createElement("li");
        categoryElement.category = category;
     
        categoryElement.addStyleClass(categoryElement.category);
            
        var label = category.toString();
        categoryElement.appendChild(document.createTextNode(label));
     
        categoryElement.addEventListener("click", this._updateFilter.bind(this), false);
     
        this.filterBarElement.appendChild(categoryElement);
        return categoryElement;
    }
    
    this.allElement = createFilterElement.call(this, "All");
    
    createDividerElement.call(this);
    
    this.errorElement = createFilterElement.call(this, "Errors");
    this.warningElement = createFilterElement.call(this, "Warnings");
    this.logElement = createFilterElement.call(this, "Logs");

    this.filter(this.allElement);
}

WebInspector.ConsoleView.prototype = {
    
    _updateFilter: function(e)
    {
        this.filter(e.target);
    },
    
    filter: function(target)
    {
        if (target.category == "All") {
            if (target.hasStyleClass("selected")) {
                // We can't unselect all, so we break early here
                return;
            }
            
            this.errorElement.removeStyleClass("selected");
            this.warningElement.removeStyleClass("selected");
            this.logElement.removeStyleClass("selected");
            
            document.getElementById("console-messages").removeStyleClass("filter-errors");
            document.getElementById("console-messages").removeStyleClass("filter-warnings");
            document.getElementById("console-messages").removeStyleClass("filter-logs");
        } else {
            // Something other than all is being selected, so we want to unselect all
            if (this.allElement.hasStyleClass("selected")) {
                this.allElement.removeStyleClass("selected");
                document.getElementById("console-messages").removeStyleClass("filter-all");
            }
        }
        
        if (target.hasStyleClass("selected")) {
            target.removeStyleClass("selected");
            var newClass = "filter-" + target.category.toLowerCase();
            var filterElement = document.getElementById("console-messages");
            filterElement.removeStyleClass(newClass);
        } else {
            target.addStyleClass("selected");
            var newClass = "filter-" + target.category.toLowerCase();
            var filterElement = document.getElementById("console-messages");
            filterElement.addStyleClass(newClass);
        }
    },
    
    _toggleConsoleButtonClicked: function()
    {
        this.drawer.visibleView = this;
    },

    attach: function(mainElement, statusBarElement)
    {
        mainElement.appendChild(this.element);
        statusBarElement.appendChild(this.clearButton);
        statusBarElement.appendChild(this.filterBarElement);
    },

    show: function()
    {
        this.toggleConsoleButton.addStyleClass("toggled-on");
        this.toggleConsoleButton.title = WebInspector.UIString("Hide console.");
        if (!this.prompt.isCaretInsidePrompt())
            this.prompt.moveCaretToEndOfPrompt();
    },

    afterShow: function()
    {
        WebInspector.currentFocusElement = this.promptElement;  
    },

    hide: function()
    {
        this.toggleConsoleButton.removeStyleClass("toggled-on");
        this.toggleConsoleButton.title = WebInspector.UIString("Show console.");
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

        if (msg.type === WebInspector.ConsoleMessage.MessageType.EndGroup) {
            if (this.groupLevel < 1)
                return;

            this.groupLevel--;

            this.currentGroup = this.currentGroup.parentGroup;
        } else {
            if (msg.type === WebInspector.ConsoleMessage.MessageType.StartGroup) {
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
            InspectorController.clearMessages(false);
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

    completions: function(wordRange, bestMatchOnly, completionsReadyCallback)
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

        var reportCompletions = this._reportCompletions.bind(this, bestMatchOnly, completionsReadyCallback, dotNotation, bracketNotation, prefix);
        // Collect comma separated object properties for the completion.

        var includeInspectorCommandLineAPI = (!dotNotation && !bracketNotation);
        if (WebInspector.panels.scripts && WebInspector.panels.scripts.paused)
            var callFrameId = WebInspector.panels.scripts.selectedCallFrameId();
        InjectedScriptAccess.getCompletions(expressionString, includeInspectorCommandLineAPI, callFrameId, reportCompletions);
    },

    _reportCompletions: function(bestMatchOnly, completionsReadyCallback, dotNotation, bracketNotation, prefix, result, isException) {
        if (isException)
            return;

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
        setTimeout(completionsReadyCallback, 0, results);
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

        WebInspector.updateFocusedNode(link.representedNode.id);
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

    evalInInspectedWindow: function(expression, objectGroup, callback)
    {
        if (WebInspector.panels.scripts && WebInspector.panels.scripts.paused) {
            WebInspector.panels.scripts.evaluateInSelectedCallFrame(expression, false, objectGroup, callback);
            return;
        }
        this.doEvalInWindow(expression, objectGroup, callback);
    },

    doEvalInWindow: function(expression, objectGroup, callback)
    {
        if (!expression) {
            // There is no expression, so the completion should happen against global properties.
            expression = "this";
        }

        function evalCallback(result)
        {
            callback(result.value, result.isException);
        };
        InjectedScriptAccess.evaluate(expression, objectGroup, evalCallback);
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

        var self = this;
        function printResult(result, exception)
        {
            self.prompt.history.push(str);
            self.prompt.historyOffset = 0;
            self.prompt.text = "";
            self.addMessage(new WebInspector.ConsoleCommandResult(result, exception, commandMessage));
        }
        this.evalInInspectedWindow(str, "console", printResult);
    },

    _format: function(output, forceObjectFormat)
    {
        var isProxy = (output != null && typeof output === "object");

        if (forceObjectFormat)
            var type = "object";
        else
            var type = Object.proxyType(output);

        if (isProxy && type !== "object" && type !== "function" && type !== "array" && type !== "node") {
            // Unwrap primitive value, skip decoration.
            output = output.description;
            type = "undecorated"
        }

        // We don't perform any special formatting on these types, so we just
        // pass them through the simple _formatvalue function.
        var undecoratedTypes = {
            "undefined": 1,
            "null": 1,
            "boolean": 1,
            "number": 1,
            "undecorated": 1
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

    _formatfunction: function(func, elem)
    {
        elem.appendChild(document.createTextNode(func.description));
    },

    _formatdate: function(date, elem)
    {
        elem.appendChild(document.createTextNode(date));
    },

    _formatstring: function(str, elem)
    {
        elem.appendChild(document.createTextNode("\"" + str + "\""));
    },

    _formatregexp: function(re, elem)
    {
        var formatted = String(re.description).replace(/([\\\/])/g, "\\$1").replace(/\\(\/[gim]*)$/, "$1").substring(1);
        elem.appendChild(document.createTextNode(formatted));
    },

    _formatarray: function(arr, elem)
    {
        InjectedScriptAccess.getProperties(arr, false, this._printArray.bind(this, elem));
    },

    _printArray: function(elem, properties)
    {
        if (!properties)
            return;
        var elements = [];
        for (var i = 0; i < properties.length; ++i) {
            var name = properties[i].name;
            if (name == parseInt(name))
                elements[name] = this._format(properties[i].value);
        }

        elem.appendChild(document.createTextNode("["));
        for (var i = 0; i < elements.length; ++i) {
            var element = elements[i];
            if (element)
                elem.appendChild(element);
            else
                elem.appendChild(document.createTextNode("undefined"))
            if (i < elements.length - 1)
                elem.appendChild(document.createTextNode(", "));
        }
        elem.appendChild(document.createTextNode("]"));
    },

    _formatnode: function(object, elem)
    {
        function printNode(nodeId)
        {
            if (!nodeId)
                return;
            var treeOutline = new WebInspector.ElementsTreeOutline();
            treeOutline.rootDOMNode = WebInspector.domAgent.nodeForId(nodeId);
            treeOutline.element.addStyleClass("outline-disclosure");
            if (!treeOutline.children[0].hasChildren)
                treeOutline.element.addStyleClass("single-node");
            elem.appendChild(treeOutline.element);
        }
        InjectedScriptAccess.pushNodeToFrontend(object, printNode);
    },

    _formatobject: function(obj, elem)
    {
        elem.appendChild(new WebInspector.ObjectPropertiesSection(obj, obj.description, null, true).element);
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
    }
}

WebInspector.ConsoleView.prototype.__proto__ = WebInspector.View.prototype;

WebInspector.ConsoleMessage = function(source, type, level, line, url, groupLevel, repeatCount)
{
    this.source = source;
    this.type = type;
    this.level = level;
    this.line = line;
    this.url = url;
    this.groupLevel = groupLevel;
    this.repeatCount = repeatCount;
    if (arguments.length > 7)
        this.setMessageBody(Array.prototype.slice.call(arguments, 7));
}

WebInspector.ConsoleMessage.prototype = {
    setMessageBody: function(args)
    {
        switch (this.type) {
            case WebInspector.ConsoleMessage.MessageType.Trace:
                var span = document.createElement("span");
                span.addStyleClass("console-formatted-trace");
                var stack = Array.prototype.slice.call(args);
                var funcNames = stack.map(function(f) {
                    return f || WebInspector.UIString("(anonymous function)");
                });
                span.appendChild(document.createTextNode(funcNames.join("\n")));
                this.formattedMessage = span;
                break;
            case WebInspector.ConsoleMessage.MessageType.Object:
                this.formattedMessage = this._format(["%O", args[0]]);
                break;
            default:
                this.formattedMessage = this._format(args);
                break;
        }

        // This is used for inline message bubbles in SourceFrames, or other plain-text representations.
        this.message = this.formattedMessage.textContent;
    },

    isErrorOrWarning: function()
    {
        return (this.level === WebInspector.ConsoleMessage.MessageLevel.Warning || this.level === WebInspector.ConsoleMessage.MessageLevel.Error);
    },

    _format: function(parameters)
    {
        var formattedResult = document.createElement("span");

        if (!parameters.length)
            return formattedResult;

        // Formatting code below assumes that parameters are all wrappers whereas frontend console
        // API allows passing arbitrary values as messages (strings, numberts, etc.). Wrap them here.
        for (var i = 0; i < parameters.length; ++i) {
            if (typeof parameters[i] !== "object" && typeof parameters[i] !== "function")
                parameters[i] = WebInspector.ObjectProxy.wrapPrimitiveValue(parameters[i]);
        }

        function formatForConsole(obj)
        {
            return WebInspector.console._format(obj);
        }

        function formatAsObjectForConsole(obj)
        {
            return WebInspector.console._format(obj, true);
        }

        if (Object.proxyType(parameters[0]) === "string") {
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

            var result = String.format(parameters[0].description, parameters.slice(1), formatters, formattedResult, append);
            formattedResult = result.formattedResult;
            parameters = result.unusedSubstitutions;
            if (parameters.length)
                formattedResult.appendChild(document.createTextNode(" "));
        }

        for (var i = 0; i < parameters.length; ++i) {
            if (Object.proxyType(parameters[i]) === "string")
                formattedResult.appendChild(WebInspector.linkifyStringAsFragment(parameters[i].description));
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
            case WebInspector.ConsoleMessage.MessageLevel.Debug:
                element.addStyleClass("console-debug-level");
                break;
            case WebInspector.ConsoleMessage.MessageLevel.Warning:
                element.addStyleClass("console-warning-level");
                break;
            case WebInspector.ConsoleMessage.MessageLevel.Error:
                element.addStyleClass("console-error-level");
                break;
        }
        
        if (this.type === WebInspector.ConsoleMessage.MessageType.StartGroup) {
            element.addStyleClass("console-group-title");
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

        var typeString;
        switch (this.type) {
            case WebInspector.ConsoleMessage.MessageType.Log:
                typeString = "Log";
                break;
            case WebInspector.ConsoleMessage.MessageType.Object:
                typeString = "Object";
                break;
            case WebInspector.ConsoleMessage.MessageType.Trace:
                typeString = "Trace";
                break;
            case WebInspector.ConsoleMessage.MessageType.StartGroup:
                typeString = "Start Group";
                break;
            case WebInspector.ConsoleMessage.MessageType.EndGroup:
                typeString = "End Group";
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
            case WebInspector.ConsoleMessage.MessageLevel.Debug:
                levelString = "Debug";
                break;
            case WebInspector.ConsoleMessage.MessageLevel.Error:
                levelString = "Error";
                break;
        }

        return sourceString + " " + typeString + " " + levelString + ": " + this.formattedMessage.textContent + "\n" + this.url + " line " + this.line;
    },

    isEqual: function(msg, disreguardGroup)
    {
        if (!msg)
            return false;

        var ret = (this.source == msg.source)
            && (this.type == msg.type)
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

WebInspector.ConsoleMessage.MessageType = {
    Log: 0,
    Object: 1,
    Trace: 2,
    StartGroup: 3,
    EndGroup: 4
}

WebInspector.ConsoleMessage.MessageLevel = {
    Tip: 0,
    Log: 1,
    Warning: 2,
    Error: 3,
    Debug: 4
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

WebInspector.ConsoleTextMessage = function(text, level)
{
    level = level || WebInspector.ConsoleMessage.MessageLevel.Log;
    WebInspector.ConsoleMessage.call(this, WebInspector.ConsoleMessage.MessageSource.JS, WebInspector.ConsoleMessage.MessageType.Log, level, 0, null, null, 1, text);
}

WebInspector.ConsoleTextMessage.prototype.__proto__ = WebInspector.ConsoleMessage.prototype;

WebInspector.ConsoleCommandResult = function(result, exception, originatingCommand)
{
    var level = (exception ? WebInspector.ConsoleMessage.MessageLevel.Error : WebInspector.ConsoleMessage.MessageLevel.Log);
    var message = result;
    var line = (exception ? result.line : -1);
    var url = (exception ? result.sourceURL : null);

    WebInspector.ConsoleMessage.call(this, WebInspector.ConsoleMessage.MessageSource.JS, WebInspector.ConsoleMessage.MessageType.Log, level, line, url, null, 1, message);

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
        
        if (msg.type === WebInspector.ConsoleMessage.MessageType.StartGroup) {
            this.messagesElement.parentNode.insertBefore(element, this.messagesElement);
            element.addEventListener("click", this._titleClicked.bind(this), true);
        } else
            this.messagesElement.appendChild(element);

        if (element.previousSibling && msg.originatingCommand && element.previousSibling.command === msg.originatingCommand)
            element.previousSibling.addStyleClass("console-adjacent-user-command-result");
    },

    _titleClicked: function(event)
    {
        var groupTitleElement = event.target.enclosingNodeOrSelfWithClass("console-group-title");
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
