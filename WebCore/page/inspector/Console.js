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

    this.commandHistory = [];
    this.commandOffset = 0;

    this.messagesElement = document.createElement("div");
    this.messagesElement.id = "console-messages";
    this.messagesElement.addEventListener("selectstart", this.messagesSelectStart.bind(this), true);
    this.messagesElement.addEventListener("click", this.messagesClicked.bind(this), true);
    this.element.appendChild(this.messagesElement);

    this.promptElement = document.createElement("div");
    this.promptElement.id = "console-prompt";
    this.promptElement.addEventListener("keydown", this.promptKeyDown.bind(this), false);
    this.promptElement.appendChild(document.createElement("br"));
    this.messagesElement.appendChild(this.promptElement);

    this.clearButton = document.createElement("button");
    this.clearButton.title = WebInspector.UIString("Clear");
    this.clearButton.textContent = WebInspector.UIString("Clear");
    this.clearButton.addEventListener("click", this.clearButtonClicked.bind(this), false);
}

WebInspector.ConsolePanel.prototype = {
    get promptText()
    {
        return this.promptElement.textContent;
    },

    set promptText(x)
    {
        if (!x) {
            // Append a break element instead of setting textContent to make sure the selection is inside the prompt.
            this.promptElement.removeChildren();
            this.promptElement.appendChild(document.createElement("br"));
        } else
            this.promptElement.textContent = x;

        this._moveCaretToEndOfPrompt();
    },

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
            if (!this._caretInsidePrompt())
                this._moveCaretToEndOfPrompt();
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

    acceptAutoComplete: function()
    {
        if (!this.autoCompleteElement || !this.autoCompleteElement.parentNode)
            return false;

        var text = this.autoCompleteElement.textContent;
        var textNode = document.createTextNode(text);
        this.autoCompleteElement.parentNode.replaceChild(textNode, this.autoCompleteElement);
        delete this.autoCompleteElement;

        var finalSelectionRange = document.createRange();
        finalSelectionRange.setStart(textNode, text.length);
        finalSelectionRange.setEnd(textNode, text.length);

        var selection = window.getSelection();
        selection.removeAllRanges();
        selection.addRange(finalSelectionRange);

        return true;
    },

    clearAutoComplete: function(includeTimeout)
    {
        if (includeTimeout && "completeTimeout" in this) {
            clearTimeout(this.completeTimeout);
            delete this.completeTimeout;
        }

        if (!this.autoCompleteElement)
            return;

        if (this.autoCompleteElement.parentNode)
            this.autoCompleteElement.parentNode.removeChild(this.autoCompleteElement);
        delete this.autoCompleteElement;
    },

    autoCompleteSoon: function()
    {
        if (!("completeTimeout" in this))
            this.completeTimeout = setTimeout(this.complete.bind(this, true), 250);
    },

    complete: function(auto)
    {
        this.clearAutoComplete(true);

        var selection = window.getSelection();
        if (!selection.rangeCount)
            return;

        var selectionRange = selection.getRangeAt(0);
        if (!selectionRange.commonAncestorContainer.isDescendant(this.promptElement))
            return;
        if (auto && !this._caretAtEndOfPrompt())
            return;

        // Pass more characters to _backwardsRange so the range will be as short as possible.
        var wordPrefixRange = this._backwardsRange(" .=:[({;", selectionRange.startContainer, selectionRange.startOffset, this.promptElement);

        var completions = this.completions(wordPrefixRange, auto);

        if (!completions || !completions.length)
            return;

        var fullWordRange = document.createRange();
        fullWordRange.setStart(wordPrefixRange.startContainer, wordPrefixRange.startOffset);
        fullWordRange.setEnd(selectionRange.endContainer, selectionRange.endOffset);

        if (completions.length === 1 || selection.isCollapsed || auto) {
            var completionText = completions[0];
        } else {
            var currentText = fullWordRange.toString().trimTrailingWhitespace();

            var foundIndex = null;
            for (var i = 0; i < completions.length; ++i) {
                if (completions[i] === currentText)
                    foundIndex = i;
            }

            if (foundIndex === null || (foundIndex + 1) >= completions.length)
                var completionText = completions[0];
            else
                var completionText = completions[foundIndex + 1];
        }

        var wordPrefixLength = wordPrefixRange.toString().length;

        fullWordRange.deleteContents();

        var finalSelectionRange = document.createRange();

        if (auto) {
            var prefixText = completionText.substring(0, wordPrefixLength);
            var suffixText = completionText.substring(wordPrefixLength);

            var prefixTextNode = document.createTextNode(prefixText);
            fullWordRange.insertNode(prefixTextNode);           

            this.autoCompleteElement = document.createElement("span");
            this.autoCompleteElement.className = "auto-complete-text";
            this.autoCompleteElement.textContent = suffixText;

            prefixTextNode.parentNode.insertBefore(this.autoCompleteElement, prefixTextNode.nextSibling);

            finalSelectionRange.setStart(prefixTextNode, wordPrefixLength);
            finalSelectionRange.setEnd(prefixTextNode, wordPrefixLength);
        } else {
            var completionTextNode = document.createTextNode(completionText);
            fullWordRange.insertNode(completionTextNode);           

            if (completions.length > 1)
                finalSelectionRange.setStart(completionTextNode, wordPrefixLength);
            else
                finalSelectionRange.setStart(completionTextNode, completionText.length);

            finalSelectionRange.setEnd(completionTextNode, completionText.length);
        }

        selection.removeAllRanges();
        selection.addRange(finalSelectionRange);
    },

    completions: function(wordRange, bestMatchOnly)
    {
        // Pass less characters to _backwardsRange so the range will be a more complete expression.
        var expression = this._backwardsRange(" =:{;", wordRange.startContainer, wordRange.startOffset, this.promptElement);
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

    clearButtonClicked: function()
    {
        this.clearMessages();
    },

    messagesSelectStart: function(event)
    {
        if (this._selectionTimeout)
            clearTimeout(this._selectionTimeout);

        this.clearAutoComplete();

        function moveBackIfOutside()
        {
            delete this._selectionTimeout;
            if (!this._caretInsidePrompt() && window.getSelection().isCollapsed)
                this._moveCaretToEndOfPrompt();
            this.autoCompleteSoon();
        }

        this._selectionTimeout = setTimeout(moveBackIfOutside.bind(this), 100);
    },

    messagesClicked: function(event)
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
            case "U+0009": // Tab
                this._onTabPressed(event);
                break;
            case "Right":
                if (!this.acceptAutoComplete())
                    this.autoCompleteSoon();
                break;
            default:
                this.clearAutoComplete();
                this.autoCompleteSoon();
                break;
        }
    },

    _backwardsRange: function(stopCharacters, endNode, endOffset, stayWithinElement)
    {
        var startNode;
        var startOffset = 0;
        var node = endNode;

        while (node) {
            if (node === stayWithinElement) {
                if (!startNode)
                    startNode = stayWithinElement;
                break;
            }

            if (node.nodeType === Node.TEXT_NODE) {
                var start = (node === endNode ? endOffset : node.nodeValue.length);
                for (var i = (start - 1); i >= 0; --i) {
                    var character = node.nodeValue[i];
                    if (stopCharacters.indexOf(character) !== -1) {
                        startNode = node;
                        startOffset = i + 1;
                        break;
                    }
                }
            }

            if (startNode)
                break;

            node = node.traversePreviousNode();
        }

        var result = document.createRange();
        result.setStart(startNode, startOffset);
        result.setEnd(endNode, endOffset);

        return result;
    },

    _evalInInspectedWindow: function(expression)
    {
        // This with block is needed to work around http://bugs.webkit.org/show_bug.cgi?id=11399
        with (InspectorController.inspectedWindow()) {
            return eval(expression);
        }
    },

    _caretInsidePrompt: function()
    {
        var selection = window.getSelection();
        if (!selection.rangeCount || !selection.isCollapsed)
            return false;
        var selectionRange = selection.getRangeAt(0);
        return selectionRange.startContainer === this.promptElement || selectionRange.startContainer.isDescendant(this.promptElement);
    },

    _caretAtEndOfPrompt: function()
    {
        var selection = window.getSelection();
        if (!selection.rangeCount || !selection.isCollapsed)
            return false;

        var selectionRange = selection.getRangeAt(0);
        var node = selectionRange.startContainer;
        if (node !== this.promptElement && !node.isDescendant(this.promptElement))
            return false;

        if (node.nodeType === Node.TEXT_NODE && selectionRange.startOffset < node.nodeValue.length)
            return false;

        var foundNextText = false;
        while (node) {
            if (node.nodeType === Node.TEXT_NODE && node.nodeValue.length) {
                if (foundNextText)
                    return false;
                foundNextText = true;
            }

            node = node.traverseNextNode(false, this.promptElement);
        }

        return true;
    },

    _moveCaretToEndOfPrompt: function()
    {
        var selection = window.getSelection();
        var selectionRange = document.createRange();

        var offset = this.promptElement.childNodes.length;
        selectionRange.setStart(this.promptElement, offset);
        selectionRange.setEnd(this.promptElement, offset);

        selection.removeAllRanges();
        selection.addRange(selectionRange);
    },

    _onTabPressed: function(event)
    {
        event.preventDefault();
        event.stopPropagation();
        this.complete();
    },

    _onEnterPressed: function(event)
    {
        event.preventDefault();
        event.stopPropagation();

        this.clearAutoComplete(true);

        var str = this.promptText;
        if (!str.length)
            return;

        this.commandHistory.push(str);
        this.commandOffset = 0;

        this.promptText = "";

        var result;
        var exception = false;
        try {
            result = this._evalInInspectedWindow(str);
        } catch(e) {
            result = e;
            exception = true;
        }

        var level = exception ? WebInspector.ConsoleMessage.MessageLevel.Error : WebInspector.ConsoleMessage.MessageLevel.Log;
        this.addMessage(new WebInspector.ConsoleCommand(str, result, this._format(result), level));
    },

    _onUpPressed: function(event)
    {
        event.preventDefault();
        event.stopPropagation();

        if (this.commandOffset == this.commandHistory.length)
            return;

        if (this.commandOffset == 0)
            this.tempSavedCommand = this.promptText;

        ++this.commandOffset;
        this.promptText = this.commandHistory[this.commandHistory.length - this.commandOffset];
    },

    _onDownPressed: function(event)
    {
        event.preventDefault();
        event.stopPropagation();

        if (this.commandOffset == 0)
            return;

        --this.commandOffset;

        if (this.commandOffset == 0) {
            this.promptText = this.tempSavedCommand;
            delete this.tempSavedCommand;
            return;
        }

        this.promptText = this.commandHistory[this.commandHistory.length - this.commandOffset];
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
