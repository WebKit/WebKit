WebInspector.ConsolePanel = function()
{
    WebInspector.Panel.call(this);

    this.messages = [];

    this.commandHistory = [];
    this.commandOffset = 0;

    this.messageList = document.createElement("ol");
    this.messageList.className = "console-message-list";
    this.element.appendChild(this.messageList);

    var console = this;
    this.messageList.addEventListener("click", function(event) { console.messageListClicked(event) }, true);

    this.consolePrompt = document.createElement("textarea");
    this.consolePrompt.className = "console-prompt";
    this.element.appendChild(this.consolePrompt);

    this.consolePrompt.addEventListener("keydown", function(event) { console.promptKeypress(event) }, false);
}

WebInspector.ConsolePanel.prototype = {
    show: function()
    {
        WebInspector.consoleListItem.item.select();
        WebInspector.Panel.prototype.show.call(this);
    },

    hide: function()
    {
        WebInspector.consoleListItem.item.deselect();
        WebInspector.Panel.prototype.hide.call(this);
    },

    addMessage: function(msg)
    {
        if (msg.url in WebInspector.resourceURLMap) {
            msg.resource = WebInspector.resourceURLMap[msg.url];
            switch (msg.level) {
                case WebInspector.ConsoleMessage.WarningMessageLevel:
                    ++msg.resource.warnings;
                    msg.resource.panel.addMessageToSource(msg);
                    break;
                case WebInspector.ConsoleMessage.ErrorMessageLevel:
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

    messageListClicked: function(event)
    {
        var link = event.target.firstParentOrSelfWithNodeName("a");
        if (link) {
            WebInspector.updateFocusedNode(link.representedNode);
            return;
        }

        var item = event.target.firstParentOrSelfWithNodeName("li");
        if (!item)
            return;

        var resource = item.message.resource;
        if (!resource)
            return;

        resource.panel.showSourceLine(item.message.line);

        event.stopPropagation();
        event.preventDefault();
    },

    promptKeypress: function(event)
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

        var level = exception ? WebInspector.ConsoleMessage.ErrorMessageLevel : WebInspector.ConsoleMessage.LogMessageLevel;

        this.addMessage(new WebInspector.ConsoleCommand(str, this._outputToNode(result)));
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

    _outputToNode: function(output)
    {
        if (output instanceof Node) {
            var anchor = document.createElement("a");
            anchor.innerHTML = output.titleInfo().title;
            anchor.representedNode = output;
            return anchor;
        }
        return document.createTextNode(Object.describe(output));
    }
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
            case WebInspector.ConsoleMessage.HTMLMessageSource:
                item.className += " console-html-source";
                break;
            case WebInspector.ConsoleMessage.XMLMessageSource:
                item.className += " console-xml-source";
                break;
            case WebInspector.ConsoleMessage.JSMessageSource:
                item.className += " console-js-source";
                break;
            case WebInspector.ConsoleMessage.CSSMessageSource:
                item.className += " console-css-source";
                break;
            case WebInspector.ConsoleMessage.OtherMessageSource:
                item.className += " console-other-source";
                break;
        }

        switch (this.level) {
            case WebInspector.ConsoleMessage.TipMessageLevel:
                item.className += " console-tip-level";
                break;
            case WebInspector.ConsoleMessage.LogMessageLevel:
                item.className += " console-log-level";
                break;
            case WebInspector.ConsoleMessage.WarningMessageLevel:
                item.className += " console-warning-level";
                break;
            case WebInspector.ConsoleMessage.ErrorMessageLevel:
                item.className += " console-error-level";
        }


        var messageDiv = document.createElement("div");
        messageDiv.className = "console-message-message";
        messageDiv.innerText = this.message;
        item.appendChild(messageDiv);

        var urlDiv = document.createElement("div");
        urlDiv.className = "console-message-url";
        urlDiv.innerText = this.url;
        item.appendChild(urlDiv);

        if (this.line >= 0) {
            var lineDiv = document.createElement("div");
            lineDiv.className = "console-message-line";
            lineDiv.innerText = this.line;
            item.appendChild(lineDiv);
        }

        return item;
    },

    toString: function()
    {
        var sourceString;
        switch (this.source) {
            case WebInspector.ConsoleMessage.HTMLMessageSource:
                sourceString = "HTML";
                break;
            case WebInspector.ConsoleMessage.XMLMessageSource:
                sourceString = "XML";
                break;
            case WebInspector.ConsoleMessage.JSMessageSource:
                sourceString = "JS";
                break;
            case WebInspector.ConsoleMessage.CSSMessageSource:
                sourceString = "CSS";
                break;
            case WebInspector.ConsoleMessage.OtherMessageSource:
                sourceString = "Other";
                break;
        }

        var levelString;
        switch (this.level) {
            case WebInspector.ConsoleMessage.TipMessageLevel:
                levelString = "Tip";
                break;
            case WebInspector.ConsoleMessage.LogMessageLevel:
                levelString = "Log";
                break;
            case WebInspector.ConsoleMessage.WarningMessageLevel:
                levelString = "Warning";
                break;
            case WebInspector.ConsoleMessage.ErrorMessageLevel:
                levelString = "Error";
                break;
        }

        return sourceString + " " + levelString + ": " + this.message + "\n" + this.url + " line " + this.line;
    }
}

// Note: Keep these constants in sync with the ones in Chrome.h
WebInspector.ConsoleMessage.HTMLMessageSource = 0;
WebInspector.ConsoleMessage.XMLMessageSource  = 1;
WebInspector.ConsoleMessage.JSMessageSource   = 2;
WebInspector.ConsoleMessage.CSSMessageSource  = 3;
WebInspector.ConsoleMessage.OtherMessageSource = 4;

WebInspector.ConsoleMessage.TipMessageLevel     = 0;
WebInspector.ConsoleMessage.LogMessageLevel     = 1;
WebInspector.ConsoleMessage.WarningMessageLevel = 2;
WebInspector.ConsoleMessage.ErrorMessageLevel   = 3;

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
        inputDiv.innerText = this.input;
        item.appendChild(inputDiv);

        var outputDiv = document.createElement("div");
        outputDiv.className = "console-command-output";
        outputDiv.appendChild(this.output);
        item.appendChild(outputDiv);

        return item;
    }
}
