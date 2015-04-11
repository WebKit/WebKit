/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 * Copyright (C) 2007, 2008, 2013-2015 Apple Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

WebInspector.ConsoleMessageView = class ConsoleMessageView extends WebInspector.Object
{
    constructor(message)
    {
        super();

        console.assert(message instanceof WebInspector.ConsoleMessage);

        this._message = message;

        this._element = document.createElement("div");
        this._element.classList.add("console-message");

        // FIXME: <https://webkit.org/b/143545> Web Inspector: LogContentView should use higher level objects
        this._element.__message = this._message;
        this._element.__messageView = this;

        switch (this._message.level) {
        case WebInspector.ConsoleMessage.MessageLevel.Log:
            this._element.classList.add("console-log-level");
            this._element.setAttribute("data-labelprefix", WebInspector.UIString("Log: "));
            break;
        case WebInspector.ConsoleMessage.MessageLevel.Debug:
            this._element.classList.add("console-debug-level");
            this._element.setAttribute("data-labelprefix", WebInspector.UIString("Debug: "));
            break;
        case WebInspector.ConsoleMessage.MessageLevel.Warning:
            this._element.classList.add("console-warning-level");
            this._element.setAttribute("data-labelprefix", WebInspector.UIString("Warning: "));
            break;
        case WebInspector.ConsoleMessage.MessageLevel.Error:
            this._element.classList.add("console-error-level");
            this._element.setAttribute("data-labelprefix", WebInspector.UIString("Error: "));
            break;
        }

        if (this._message.type === WebInspector.ConsoleMessage.MessageType.Result) {
            this._element.classList.add("console-user-command-result");
            if (!this._element.getAttribute("data-labelprefix"))
                this._element.setAttribute("data-labelprefix", WebInspector.UIString("Output: "));
        }

        if (this._message.type === WebInspector.ConsoleMessage.MessageType.StartGroup || this._message.type === WebInspector.ConsoleMessage.MessageType.StartGroupCollapsed)
            this._element.classList.add("console-group-title");

        // These are the parameters unused by the messages's optional format string.
        // Any extra parameters will be displayed as children of this message.
        this._extraParameters = this._message.parameters;

        // FIXME: The location link should include stack trace information.
        this._appendLocationLink();

        this._messageTextElement = this._element.appendChild(document.createElement("span"));
        this._messageTextElement.classList.add("console-top-level-message");
        this._messageTextElement.classList.add("console-message-text");
        this._appendMessageTextAndArguments(this._messageTextElement);
        this._appendSavedResultIndex();

        this._appendExtraParameters();
        this._appendStackTrace();

        this.repeatCount = this._message.repeatCount;
    }

    // Public

    get element()
    {
        return this._element;
    }

    get message()
    {
        return this._message;
    }

    get repeatCount()
    {
        return this._repeatCount;
    }

    set repeatCount(count)
    {
        console.assert(typeof count === "number");

        if (this._repeatCount === count)
            return;

        this._repeatCount = count;

        if (count <= 1) {
            if (this._repeatCountElement) {
                this._repeatCountElement.remove();
                this._repeatCountElement = null;
            }
            return;
        }

        if (!this._repeatCountElement) {
            this._repeatCountElement = document.createElement("span");
            this._repeatCountElement.classList.add("repeat-count");
            this._element.insertBefore(this._repeatCountElement, this._element.firstChild);
        }

        this._repeatCountElement.textContent = count;
    }

    get expandable()
    {
        return this._element.classList.contains("expandable");
    }

    set expandable(x)
    {
        if (x === this.expandable)
            return;

        var becameExpandable = this._element.classList.toggle("expandable", x);

        if (becameExpandable)
            this._messageTextElement.addEventListener("click", this.toggle);
        else
            this._messageTextElement.removeEventListener("click", this.toggle);
    }

    expand()
    {
        this._element.classList.add("expanded");

        // Auto-expand an inner object tree if there is a single object.
        if (this._objectTree && this._extraParameters.length === 1)
            this._objectTree.expand();
    }

    collapse()
    {
        this._element.classList.remove("expanded");
    }

    toggle()
    {
        if (this._element.classList.contains("expanded"))
            this.collapse();
        else
            this.expand();
    }

    toClipboardString(isPrefixOptional)
    {
        var clipboardString = this._messageTextElement.innerText;

        if (this._message.type === WebInspector.ConsoleMessage.MessageType.Trace)
            clipboardString = "console.trace()";

        var hasStackTrace = this._shouldShowStackTrace();
        if (hasStackTrace) {
            this._message.stackTrace.forEach(function(frame) {
                clipboardString += "\n\t" + (frame.functionName || WebInspector.UIString("(anonymous function)"));
                if (frame.url)
                    clipboardString += " (" + WebInspector.displayNameForURL(frame.url) + ", line " + frame.lineNumber + ")";
            });
        } else {
            var repeatString = this.repeatCount > 1 ? "x" + this.repeatCount : "";
            var urlLine = "";
            if (this._message.url) {
                var components = [WebInspector.displayNameForURL(this._message.url), "line " + this._message.line];
                if (repeatString)
                    components.push(repeatString);
                urlLine = " (" + components.join(", ") + ")";
            } else if (repeatString)
                urlLine = " (" + repeatString + ")";

            if (urlLine) {
                var lines = clipboardString.split("\n");
                lines[0] += urlLine;
                clipboardString = lines.join("\n");
            }
        }

        if (!isPrefixOptional || this._enforcesClipboardPrefixString())
            return this._clipboardPrefixString() + clipboardString;
        return clipboardString;
    }

    // Private

    _appendMessageTextAndArguments(element)
    {
        if (this._message.source === WebInspector.ConsoleMessage.MessageSource.ConsoleAPI) {
            switch (this._message.type) {
            case WebInspector.ConsoleMessage.MessageType.Trace:
                // FIXME: We should use a better string then console.trace.
                element.appendChild(document.createTextNode("console.trace()"));
                break;

            case WebInspector.ConsoleMessage.MessageType.Assert:
                var args = [WebInspector.UIString("Assertion failed:")];
                if (this._message.parameters)
                    args.concat(this._message.parameters);
                this._appendFormattedArguments(element, args);
                break;

            case WebInspector.ConsoleMessage.MessageType.Dir:
                var obj = this._message.parameters ? this._message.parameters[0] : undefined;
                this._appendFormattedArguments(element, ["%O", obj]);
                break;

            case WebInspector.ConsoleMessage.MessageType.Table:
                // FIXME: Remove messageText?
                var args = this._message.parameters || [this._message.messageText];
                element.appendChild(this._formatParameterAsTable(args));
                break;

            default:
                var args = this._message.parameters || [this._message.messageText];
                this._appendFormattedArguments(element, args);
                break;
            }
            return;
        }

        // FIXME: Better handle WebInspector.ConsoleMessage.MessageSource.Network.

        var args = this._message.parameters || [this._message.messageText];
        this._appendFormattedArguments(element, args);
    }

    _appendSavedResultIndex(element)
    {
        if (!this._message.savedResultIndex)
            return;

        console.assert(this._message instanceof WebInspector.ConsoleCommandResultMessage);
        console.assert(this._message.type === WebInspector.ConsoleMessage.MessageType.Result);

        var savedVariableElement = document.createElement("span");
        savedVariableElement.classList.add("console-saved-variable");
        savedVariableElement.textContent = " = $" + this._message.savedResultIndex;

        if (this._objectTree)
            this._objectTree.appendTitleSuffix(savedVariableElement);
        else
            this._messageTextElement.appendChild(savedVariableElement);
    }

    _appendLocationLink()
    {
        // FIXME: Better handle WebInspector.ConsoleMessage.MessageSource.Network.
        if (this._message.source === WebInspector.ConsoleMessage.MessageSource.Network || this._message.request)
            return;

        var firstNonNativeCallFrame = this._firstNonNativeCallFrame();
        if (firstNonNativeCallFrame) {
            var urlElement = this._linkifyCallFrame(firstNonNativeCallFrame);
            this._element.appendChild(urlElement);
        } else if (this._message.url && !this._shouldHideURL(this._message.url)) {
            var urlElement = this._linkifyLocation(this._message.url, this._message.line, this._message.column);
            this._element.appendChild(urlElement);
        }
    }

    _appendExtraParameters()
    {
        if (!this._extraParameters || !this._extraParameters.length)
            return;

        this.expandable = true;

        // Auto-expand if there are multiple objects.
        if (this._extraParameters.length > 1)
            this.expand();

        this._extraElementsList = this._element.appendChild(document.createElement("ol"));
        this._extraElementsList.classList.add("console-message-extra-parameters-container");

        for (var parameter of this._extraParameters) {
            var listItemElement = this._extraElementsList.appendChild(document.createElement("li"));
            var forceObjectExpansion = parameter.type === "object" && (parameter.subtype !== "null" && parameter.subtype !== "regexp" && parameter.subtype !== "node");
            listItemElement.classList.add("console-message-extra-parameter");
            listItemElement.appendChild(this._formatParameter(parameter, forceObjectExpansion));
        }
    }

    _appendStackTrace()
    {
        if (!this._shouldShowStackTrace())
            return;

        this.expandable = true;

        // Auto-expand for console.trace.
        if (this._message.type === WebInspector.ConsoleMessage.MessageType.Trace)
            this.expand();

        this._stackTraceElement = this._element.appendChild(document.createElement("ul"));
        this._stackTraceElement.classList.add("console-message-stack-trace-container");
        this._stackTraceElement.classList.add("console-message-text");

        for (var callFrame of this._message.stackTrace) {
            var callFrameElement = this._stackTraceElement.appendChild(document.createElement("li"));
            callFrameElement.classList.add("console-message-stack-trace-call-frame");
            callFrameElement.textContent = callFrame.functionName || WebInspector.UIString("(anonymous function)");
            if (callFrame.url && !this._shouldHideURL(callFrame.url))
                callFrameElement.appendChild(this._linkifyCallFrame(callFrame));
        }
    }

    _appendFormattedArguments(element, parameters)
    {
        if (!parameters.length)
            return;

        // FIXME: Only pass RemoteObjects here so we can avoid this work.
        for (var i = 0; i < parameters.length; ++i) {
            if (parameters[i] instanceof WebInspector.RemoteObject)
                continue;

            if (typeof parameters[i] === "object")
                parameters[i] = WebInspector.RemoteObject.fromPayload(parameters[i]);
            else
                parameters[i] = WebInspector.RemoteObject.fromPrimitiveValue(parameters[i]);
        }

        var builderElement = element.appendChild(document.createElement("span"));
        var shouldFormatWithStringSubstitution = WebInspector.RemoteObject.type(parameters[0]) === "string" && this._message.type !== WebInspector.ConsoleMessage.MessageType.Result;

        // Single object (e.g. console result or logging a non-string object).
        if (parameters.length === 1 && !shouldFormatWithStringSubstitution) {
            this._extraParameters = null;
            builderElement.appendChild(this._formatParameter(parameters[0], false));
            return;
        }

        console.assert(this._message.type !== WebInspector.ConsoleMessage.MessageType.Result);

        // Format string / message / default message.
        if (shouldFormatWithStringSubstitution) {
            var result = this._formatWithSubstitutionString(parameters, builderElement);
            parameters = result.unusedSubstitutions;
            this._extraParameters = parameters;
        } else {
            var defaultMessage = WebInspector.UIString("No message");
            builderElement.appendChild(document.createTextNode(defaultMessage));
        }

        // Trailing parameters.
        if (parameters.length) {
            if (parameters.length === 1) {
                // Single object. Show a preview.
                var enclosedElement = builderElement.appendChild(document.createElement("span"));
                enclosedElement.classList.add("console-message-preview-divider");
                enclosedElement.textContent = " \u2013 ";

                var previewContainer = builderElement.appendChild(document.createElement("span"));
                previewContainer.classList.add("console-message-preview");

                var parameter = parameters[0];
                var preview = WebInspector.FormattedValue.createObjectPreviewOrFormattedValueForRemoteObject(parameter, WebInspector.ObjectPreviewView.Mode.Brief);
                var isPreviewView = preview instanceof WebInspector.ObjectPreviewView;
                var previewElement = isPreviewView ? preview.element : preview;
                previewContainer.appendChild(previewElement);

                // If this preview is effectively lossless, we can avoid making this console message expandable.
                if ((isPreviewView && preview.lossless) || (!isPreviewView && this._shouldConsiderObjectLossless(parameter)))
                    this._extraParameters = null;
            } else {
                // Multiple objects. Show an indicator.
                builderElement.appendChild(document.createTextNode(" "));
                var enclosedElement = builderElement.appendChild(document.createElement("span"));
                enclosedElement.classList.add("console-message-enclosed");
                enclosedElement.textContent = "(" + parameters.length + ")";
            }
        }
    }

    _shouldConsiderObjectLossless(object)
    {
        return object.type !== "object" || object.subtype === "null" || object.subtype === "regexp";
    }

    _formatParameter(parameter, forceObjectFormat)
    {
        var type;
        if (forceObjectFormat)
            type = "object";
        else if (parameter instanceof WebInspector.RemoteObject)
            type = parameter.subtype || parameter.type;
        else {
            console.assert(false, "no longer reachable");
            type = typeof parameter;
        }

        var formatters = {
            "object": this._formatParameterAsObject,
            "error": this._formatParameterAsObject,
            "map": this._formatParameterAsObject,
            "set": this._formatParameterAsObject,
            "weakmap": this._formatParameterAsObject,
            "iterator": this._formatParameterAsObject,
            "class": this._formatParameterAsObject,
            "array": this._formatParameterAsArray,
            "node": this._formatParameterAsNode,
            "string": this._formatParameterAsString,
        };

        var formatter = formatters[type] || this._formatParameterAsValue;

        var span = document.createElement("span");
        formatter.call(this, parameter, span, forceObjectFormat);
        return span;
    }

    _formatParameterAsValue(value, element)
    {
        element.appendChild(WebInspector.FormattedValue.createElementForRemoteObject(value));
    }

    _formatParameterAsString(object, element)
    {
        element.appendChild(WebInspector.FormattedValue.createLinkifiedElementString(object.description));
    }

    _formatParameterAsNode(object, element)
    {
        element.appendChild(WebInspector.FormattedValue.createElementForNode(object));
    }

    _formatParameterAsObject(object, element, forceExpansion)
    {
        // FIXME: Should have a better ObjectTreeView mode for classes (static methods and methods).
        // FIXME: Only need to assign to objectTree if this is a ConsoleMessageResult. We should assert that.
        this._objectTree = new WebInspector.ObjectTreeView(object, null, this._rootPropertyPathForObject(object), forceExpansion);
        element.appendChild(this._objectTree.element);
    }

    _formatParameterAsArray(array, element)
    {
        this._objectTree = new WebInspector.ObjectTreeView(array, WebInspector.ObjectTreeView.Mode.Properties, this._rootPropertyPathForObject(array));
        element.appendChild(this._objectTree.element);
    }

    _rootPropertyPathForObject(object)
    {
        if (!this._message.savedResultIndex)
            return null;

        return new WebInspector.PropertyPath(object, "$" + this._message.savedResultIndex);
    }

    _formatWithSubstitutionString(parameters, formattedResult)
    {
        function parameterFormatter(force, obj)
        {
            return this._formatParameter(obj, force);
        }

        function stringFormatter(obj)
        {
            return obj.description;
        }

        function floatFormatter(obj)
        {
            if (typeof obj.value !== "number")
                return parseFloat(obj.description);
            return obj.value;
        }

        function integerFormatter(obj)
        {
            if (typeof obj.value !== "number")
                return parseInt(obj.description);
            return Math.floor(obj.value);
        }

        var currentStyle = null;
        function styleFormatter(obj)
        {
            currentStyle = {};
            var buffer = document.createElement("span");
            buffer.setAttribute("style", obj.description);
            for (var i = 0; i < buffer.style.length; i++) {
                var property = buffer.style[i];
                if (isWhitelistedProperty(property))
                    currentStyle[property] = buffer.style[property];
            }
        }

        function isWhitelistedProperty(property)
        {
            for (var prefix of ["background", "border", "color", "font", "line", "margin", "padding", "text"]) {
                if (property.startsWith(prefix) || property.startsWith("-webkit-" + prefix))
                    return true;
            }
            return false;
        }

        // Firebug uses %o for formatting objects.
        var formatters = {};
        formatters.o = parameterFormatter.bind(this, false);
        formatters.s = stringFormatter;
        formatters.f = floatFormatter;

        // Firebug allows both %i and %d for formatting integers.
        formatters.i = integerFormatter;
        formatters.d = integerFormatter;

        // Firebug uses %c for styling the message.
        formatters.c = styleFormatter;

        // Support %O to force object formatting, instead of the type-based %o formatting.
        formatters.O = parameterFormatter.bind(this, true);

        function append(a, b)
        {
            if (b instanceof Node)
                a.appendChild(b);
            else if (b) {
                var toAppend = WebInspector.linkifyStringAsFragment(b.toString());
                if (currentStyle) {
                    var wrapper = document.createElement("span");
                    for (var key in currentStyle)
                        wrapper.style[key] = currentStyle[key];
                    wrapper.appendChild(toAppend);
                    toAppend = wrapper;
                }

                var span = document.createElement("span");
                span.className = "type-string";
                span.appendChild(toAppend);
                a.appendChild(span);
            }
            return a;
        }

        // String.format does treat formattedResult like a Builder, result is an object.
        return String.format(parameters[0].description, parameters.slice(1), formatters, formattedResult, append);
    }

    _shouldShowStackTrace()
    {
        if (!this._message.stackTrace || !this._message.stackTrace.length)
            return false;

        return this._message.source === WebInspector.ConsoleMessage.MessageSource.Network
            || this._message.level === WebInspector.ConsoleMessage.MessageLevel.Error
            || this._message.type === WebInspector.ConsoleMessage.MessageType.Trace;
    }

    _shouldHideURL(url)
    {
        return url === "undefined" || url === "[native code]";
    }

    _firstNonNativeCallFrame()
    {
        if (!this._message.stackTrace)
            return null;

        for (var i = 0; i < this._message.stackTrace.length; i++) {
            var frame = this._message.stackTrace[i];
            if (!frame.url || frame.url === "[native code]")
                continue;
            return frame;
        }

        return null;
    }

    _linkifyLocation(url, lineNumber, columnNumber)
    {
        // ConsoleMessage stack trace line numbers are one-based.
        lineNumber = lineNumber ? lineNumber - 1 : 0;
        columnNumber = columnNumber ? columnNumber - 1 : 0;
        return WebInspector.linkifyLocation(url, lineNumber, columnNumber, "console-message-url");
    }

    _linkifyCallFrame(callFrame)
    {
        return this._linkifyLocation(callFrame.url, callFrame.lineNumber, callFrame.columnNumber);
    }

    _userProvidedColumnNames(columnNamesArgument)
    {
        if (!columnNamesArgument)
            return null;

        console.assert(columnNamesArgument instanceof WebInspector.RemoteObject);

        // Single primitive argument.
        if (columnNamesArgument.type === "string" || columnNamesArgument.type === "number")
            return [String(columnNamesArgument.value)];

        // Ignore everything that is not an array with property previews.
        if (columnNamesArgument.type !== "object" || columnNamesArgument.subtype !== "array" || !columnNamesArgument.preview || !columnNamesArgument.preview.propertyPreviews)
            return null;

        // Array. Look into the preview and get string values.
        var extractedColumnNames = [];
        for (var propertyPreview of columnNamesArgument.preview.propertyPreviews) {
            if (propertyPreview.type === "string" || propertyPreview.type === "number")
                extractedColumnNames.push(String(propertyPreview.value));
        }

        return extractedColumnNames.length ? extractedColumnNames : null;
    }

    _formatParameterAsTable(parameters)
    {
        var element = document.createElement("span");
        var table = parameters[0];
        if (!table || !table.preview)
            return element;

        var rows = [];
        var columnNames = [];
        var flatValues = [];
        var preview = table.preview;
        var userProvidedColumnNames = false;

        // User provided columnNames.
        var extractedColumnNames = this._userProvidedColumnNames(parameters[1]);
        if (extractedColumnNames) {
            userProvidedColumnNames = true;
            columnNames = extractedColumnNames;
        }

        // Check first for valuePreviews in the properties meaning this was an array of objects.
        if (preview.propertyPreviews) {
            for (var i = 0; i < preview.propertyPreviews.length; ++i) {
                var rowProperty = preview.propertyPreviews[i];
                var rowPreview = rowProperty.valuePreview;
                if (!rowPreview)
                    continue;

                var rowValue = {};
                var maxColumnsToRender = 10;
                for (var j = 0; j < rowPreview.propertyPreviews.length; ++j) {
                    var cellProperty = rowPreview.propertyPreviews[j];
                    var columnRendered = columnNames.includes(cellProperty.name);
                    if (!columnRendered) {
                        if (userProvidedColumnNames || columnNames.length === maxColumnsToRender)
                            continue;
                        columnRendered = true;
                        columnNames.push(cellProperty.name);
                    }

                    rowValue[cellProperty.name] = WebInspector.FormattedValue.createElementForPropertyPreview(cellProperty);
                }
                rows.push([rowProperty.name, rowValue]);
            }
        }

        // If there were valuePreviews, convert to a flat list.
        if (rows.length) {
            var emDash = "\u2014";
            columnNames.unshift(WebInspector.UIString("(Index)"));
            for (var i = 0; i < rows.length; ++i) {
                var rowName = rows[i][0];
                var rowValue = rows[i][1];
                flatValues.push(rowName);
                for (var j = 1; j < columnNames.length; ++j) {
                    var columnName = columnNames[j];
                    if (!(columnName in rowValue))
                        flatValues.push(emDash);
                    else
                        flatValues.push(rowValue[columnName]);
                }
            }
        }

        // If there were no value Previews, then check for an array of values.
        if (!flatValues.length && preview.propertyPreviews) {
            for (var i = 0; i < preview.propertyPreviews.length; ++i) {
                var rowProperty = preview.propertyPreviews[i];
                if (!("value" in rowProperty))
                    continue;

                if (!columnNames.length) {
                    columnNames.push(WebInspector.UIString("Index"));
                    columnNames.push(WebInspector.UIString("Value"));
                }

                flatValues.push(rowProperty.name);
                flatValues.push(WebInspector.FormattedValue.createElementForPropertyPreview(rowProperty));
            }
        }

        // If lossless or not table data, output the object so full data can be gotten.
        if (!preview.lossless || !flatValues.length) {
            element.appendChild(this._formatParameter(table));
            if (!flatValues.length)
                return element;
        }

        var dataGrid = WebInspector.DataGrid.createSortableDataGrid(columnNames, flatValues);
        dataGrid.element.classList.add("inline");
        element.appendChild(dataGrid.element);

        return element;
    }

    _levelString()
    {
        switch (this._message.level) {
        case WebInspector.ConsoleMessage.MessageLevel.Log:
            return "Log";
        case WebInspector.ConsoleMessage.MessageLevel.Warning:
            return "Warning";
        case WebInspector.ConsoleMessage.MessageLevel.Debug:
            return "Debug";
        case WebInspector.ConsoleMessage.MessageLevel.Error:
            return "Error";
        }
    }

    _enforcesClipboardPrefixString()
    {
        return this._message.type !== WebInspector.ConsoleMessage.MessageType.Result;
    }

    _clipboardPrefixString()
    {
        if (this._message.type === WebInspector.ConsoleMessage.MessageType.Result)
            return "< ";

        return "[" + this._levelString() + "] ";
    }
};
