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

        this._expandable = false;

        this._element = document.createElement("div");
        this._element.classList.add("console-message");

        // FIXME: <https://webkit.org/b/143545> Web Inspector: LogContentView should use higher level objects
        this._element.__message = this._message;
        this._element.__messageView = this;

        if (this._message.type === WebInspector.ConsoleMessage.MessageType.Result) {
            this._element.classList.add("console-user-command-result");
            this._element.setAttribute("data-labelprefix", WebInspector.UIString("Output: "));
        } else if (this._message.type === WebInspector.ConsoleMessage.MessageType.StartGroup || this._message.type === WebInspector.ConsoleMessage.MessageType.StartGroupCollapsed)
            this._element.classList.add("console-group-title");

        switch (this._message.level) {
        case WebInspector.ConsoleMessage.MessageLevel.Log:
            this._element.classList.add("console-log-level");
            this._element.setAttribute("data-labelprefix", WebInspector.UIString("Log: "));
            break;
        case WebInspector.ConsoleMessage.MessageLevel.Info:
            this._element.classList.add("console-info-level");
            this._element.setAttribute("data-labelprefix", WebInspector.UIString("Info: "));
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
        // There are extra arguments or a call stack that can be shown.
        if (this._expandable)
            return true;

        // There is an object tree that could be expanded.
        if (this._objectTree)
            return true;

        return false;
    }

    expand()
    {
        if (this._expandable)
            this._element.classList.add("expanded");

        // Auto-expand an inner object tree if there is a single object.
        if (this._objectTree) {
            if (!this._extraParameters || this._extraParameters.length <= 1)
                this._objectTree.expand();
        }
    }

    collapse()
    {
        if (this._expandable)
            this._element.classList.remove("expanded");

        // Collapse the object tree just in cases where it was autoexpanded.
        if (this._objectTree) {
            if (!this._extraParameters || this._extraParameters.length <= 1)
                this._objectTree.collapse();
        }
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
        let clipboardString = this._messageTextElement.innerText.removeWordBreakCharacters();
        if (this._message.savedResultIndex)
            clipboardString = clipboardString.replace(/\s*=\s*(\$\d+)$/, " = $1");

        if (this._message.type === WebInspector.ConsoleMessage.MessageType.Trace)
            clipboardString = "console.trace()";

        let hasStackTrace = this._shouldShowStackTrace();
        if (!hasStackTrace) {
            let repeatString = this.repeatCount > 1 ? "x" + this.repeatCount : "";
            let urlLine = "";
            if (this._message.url) {
                let components = [WebInspector.displayNameForURL(this._message.url), "line " + this._message.line];
                if (repeatString)
                    components.push(repeatString);
                urlLine = " (" + components.join(", ") + ")";
            } else if (repeatString)
                urlLine = " (" + repeatString + ")";

            if (urlLine) {
                let lines = clipboardString.split("\n");
                lines[0] += urlLine;
                clipboardString = lines.join("\n");
            }
        }

        if (this._extraElementsList)
            clipboardString += "\n" + this._extraElementsList.innerText.removeWordBreakCharacters().trim();

        if (hasStackTrace) {
            this._message.stackTrace.callFrames.forEach(function(frame) {
                clipboardString += "\n\t" + (frame.functionName || WebInspector.UIString("(anonymous function)"));
                if (frame.sourceCodeLocation)
                    clipboardString += " (" + frame.sourceCodeLocation.originalLocationString() + ")";
            });
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
                element.append("console.trace()");
                break;

            case WebInspector.ConsoleMessage.MessageType.Assert:
                var args = [WebInspector.UIString("Assertion Failed")];
                if (this._message.parameters) {
                    if (this._message.parameters[0].type === "string")
                        args = [WebInspector.UIString("Assertion Failed: %s")].concat(this._message.parameters);
                    else
                        args = args.concat(this._message.parameters);
                }
                this._appendFormattedArguments(element, args);
                break;

            case WebInspector.ConsoleMessage.MessageType.Dir:
                var obj = this._message.parameters ? this._message.parameters[0] : undefined;
                this._appendFormattedArguments(element, ["%O", obj]);
                break;

            case WebInspector.ConsoleMessage.MessageType.Table:
                var args = this._message.parameters;
                element.appendChild(this._formatParameterAsTable(args));
                this._extraParameters = null;
                break;

            case WebInspector.ConsoleMessage.MessageType.StartGroup:
            case WebInspector.ConsoleMessage.MessageType.StartGroupCollapsed:
                var args = this._message.parameters || [this._message.messageText || WebInspector.UIString("Group")];
                this._formatWithSubstitutionString(args, element);
                this._extraParameters = null;
                break;

            default:
                var args = this._message.parameters || [this._message.messageText];
                this._appendFormattedArguments(element, args);
                break;
            }
            return;
        }

        // FIXME: Better handle WebInspector.ConsoleMessage.MessageSource.Network once it has request info.

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
        if (this._message.source === WebInspector.ConsoleMessage.MessageSource.Network) {
            if (this._message.url) {
                var anchor = WebInspector.linkifyURLAsNode(this._message.url, this._message.url, "console-message-url");
                anchor.classList.add("console-message-location");
                this._element.appendChild(anchor);                
            }
            return;
        }

        var firstNonNativeCallFrame = this._message.stackTrace.firstNonNativeCallFrame;

        var callFrame;
        if (firstNonNativeCallFrame) {
            // JavaScript errors and console.* methods.
            callFrame = firstNonNativeCallFrame;
        } else if (this._message.url && !this._shouldHideURL(this._message.url)) {
            // CSS warnings have no stack traces.
            callFrame = WebInspector.CallFrame.fromPayload({
                url: this._message.url,
                lineNumber: this._message.line,
                columnNumber: this._message.column
            });
        }

        if (callFrame) {
            const showFunctionName = !!callFrame.functionName;
            var locationElement = new WebInspector.CallFrameView(callFrame, showFunctionName);
            locationElement.classList.add("console-message-location");
            this._element.appendChild(locationElement);

            return;
        }

        if (this._message.parameters && this._message.parameters.length === 1) {
            var parameter = this._createRemoteObjectIfNeeded(this._message.parameters[0]);

            parameter.findFunctionSourceCodeLocation().then(function(result) {
                if (result === WebInspector.RemoteObject.SourceCodeLocationPromise.NoSourceFound || result === WebInspector.RemoteObject.SourceCodeLocationPromise.MissingObjectId)
                    return;

                var link = this._linkifyLocation(result.sourceCode.url, result.lineNumber, result.columnNumber);
                link.classList.add("console-message-location");

                if (this._element.hasChildNodes())
                    this._element.insertBefore(link, this._element.firstChild);
                else
                    this._element.appendChild(link);
            }.bind(this));
        }
    }

    _appendExtraParameters()
    {
        if (!this._extraParameters || !this._extraParameters.length)
            return;

        this._makeExpandable();

        // Auto-expand if there are multiple objects.
        if (this._extraParameters.length > 1)
            this.expand();

        this._extraElementsList = this._element.appendChild(document.createElement("ol"));
        this._extraElementsList.classList.add("console-message-extra-parameters-container");

        for (var parameter of this._extraParameters) {
            var listItemElement = this._extraElementsList.appendChild(document.createElement("li"));
            const forceObjectFormat = parameter.type === "object" && (parameter.subtype !== "null" && parameter.subtype !== "regexp" && parameter.subtype !== "node" && parameter.subtype !== "error");
            listItemElement.classList.add("console-message-extra-parameter");
            listItemElement.appendChild(this._formatParameter(parameter, forceObjectFormat));
        }
    }

    _appendStackTrace()
    {
        if (!this._shouldShowStackTrace())
            return;

        this._makeExpandable();

        // Auto-expand for console.trace.
        if (this._message.type === WebInspector.ConsoleMessage.MessageType.Trace)
            this.expand();

        this._stackTraceElement = this._element.appendChild(document.createElement("div"));
        this._stackTraceElement.classList.add("console-message-text", "console-message-stack-trace-container");

        var callFramesElement = new WebInspector.StackTraceView(this._message.stackTrace).element;
        this._stackTraceElement.appendChild(callFramesElement);
    }

    _createRemoteObjectIfNeeded(parameter)
    {
        // FIXME: Only pass RemoteObjects here so we can avoid this work.
        if (parameter instanceof WebInspector.RemoteObject)
            return parameter;

        if (typeof parameter === "object")
            return WebInspector.RemoteObject.fromPayload(parameter);

        return WebInspector.RemoteObject.fromPrimitiveValue(parameter);
    }

    _appendFormattedArguments(element, parameters)
    {
        if (!parameters.length)
            return;

        for (var i = 0; i < parameters.length; ++i)
            parameters[i] = this._createRemoteObjectIfNeeded(parameters[i]);

        var builderElement = element.appendChild(document.createElement("span"));
        var shouldFormatWithStringSubstitution = WebInspector.RemoteObject.type(parameters[0]) === "string" && this._message.type !== WebInspector.ConsoleMessage.MessageType.Result;

        // Single object (e.g. console result or logging a non-string object).
        if (parameters.length === 1 && !shouldFormatWithStringSubstitution) {
            this._extraParameters = null;
            builderElement.appendChild(this._formatParameter(parameters[0], false));
            return;
        }

        console.assert(this._message.type !== WebInspector.ConsoleMessage.MessageType.Result);

        if (shouldFormatWithStringSubstitution && this._isStackTrace(parameters[0]))
            shouldFormatWithStringSubstitution = false;

        // Format string / message / default message.
        if (shouldFormatWithStringSubstitution) {
            var result = this._formatWithSubstitutionString(parameters, builderElement);
            parameters = result.unusedSubstitutions;
            this._extraParameters = parameters;
        } else {
            var defaultMessage = WebInspector.UIString("No message");
            builderElement.append(defaultMessage);
        }

        // Trailing parameters.
        if (parameters.length) {
            let enclosedElement = document.createElement("span");

            if (parameters.length === 1 && !this._isStackTrace(parameters[0])) {
                let parameter = parameters[0];

                // Single object. Show a preview.
                builderElement.append(enclosedElement);
                enclosedElement.classList.add("console-message-preview-divider");
                enclosedElement.textContent = " \u2013 ";

                var previewContainer = builderElement.appendChild(document.createElement("span"));
                previewContainer.classList.add("console-message-preview");

                var preview = WebInspector.FormattedValue.createObjectPreviewOrFormattedValueForRemoteObject(parameter, WebInspector.ObjectPreviewView.Mode.Brief);
                var isPreviewView = preview instanceof WebInspector.ObjectPreviewView;

                if (isPreviewView)
                    preview.setOriginatingObjectInfo(parameter, null);

                var previewElement = isPreviewView ? preview.element : preview;
                previewContainer.appendChild(previewElement);

                // If this preview is effectively lossless, we can avoid making this console message expandable.
                if ((isPreviewView && preview.lossless) || (!isPreviewView && this._shouldConsiderObjectLossless(parameter)))
                    this._extraParameters = null;
            } else {
                // Multiple objects. Show an indicator.
                builderElement.append(" ", enclosedElement);
                enclosedElement.classList.add("console-message-enclosed");
                enclosedElement.textContent = "(" + parameters.length + ")";
            }
        }
    }

    _isStackTrace(parameter)
    {
        console.assert(parameter instanceof WebInspector.RemoteObject);

        if (WebInspector.RemoteObject.type(parameter) !== "string")
            return false;

        return WebInspector.StackTrace.isLikelyStackTrace(parameter.description);
    }

    _shouldConsiderObjectLossless(object)
    {
        if (object.type === "string") {
            const description = object.description;
            const maxLength = WebInspector.FormattedValue.MAX_PREVIEW_STRING_LENGTH;
            const longOrMultiLineString = description.length > maxLength || description.slice(0, maxLength).includes("\n");
            return !longOrMultiLineString;
        }

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
            "error": this._formatParameterAsError,
            "map": this._formatParameterAsObject,
            "set": this._formatParameterAsObject,
            "weakmap": this._formatParameterAsObject,
            "weakset": this._formatParameterAsObject,
            "iterator": this._formatParameterAsObject,
            "class": this._formatParameterAsObject,
            "array": this._formatParameterAsArray,
            "node": this._formatParameterAsNode,
            "string": this._formatParameterAsString,
        };

        var formatter = formatters[type] || this._formatParameterAsValue;

        const fragment = document.createDocumentFragment();
        formatter.call(this, parameter, fragment, forceObjectFormat);
        return fragment;
    }

    _formatParameterAsValue(value, fragment)
    {
        fragment.appendChild(WebInspector.FormattedValue.createElementForRemoteObject(value));
    }

    _formatParameterAsString(object, fragment)
    {
        if (this._isStackTrace(object)) {
            let stackTrace = WebInspector.StackTrace.fromString(object.description);
            if (stackTrace.callFrames.length) {
                let stackView = new WebInspector.StackTraceView(stackTrace);
                fragment.appendChild(stackView.element);
                return;
            }
        }

        fragment.appendChild(WebInspector.FormattedValue.createLinkifiedElementString(object.description));
    }

    _formatParameterAsNode(object, fragment)
    {
        fragment.appendChild(WebInspector.FormattedValue.createElementForNode(object));
    }

    _formatParameterAsObject(object, fragment, forceExpansion)
    {
        // FIXME: Should have a better ObjectTreeView mode for classes (static methods and methods).
        this._objectTree = new WebInspector.ObjectTreeView(object, null, this._rootPropertyPathForObject(object), forceExpansion);
        fragment.appendChild(this._objectTree.element);
    }

    _formatParameterAsError(object, fragment)
    {
        this._objectTree = new WebInspector.ErrorObjectView(object);
        fragment.appendChild(this._objectTree.element);
    }

    _formatParameterAsArray(array, fragment)
    {
        this._objectTree = new WebInspector.ObjectTreeView(array, WebInspector.ObjectTreeView.Mode.Properties, this._rootPropertyPathForObject(array));
        fragment.appendChild(this._objectTree.element);
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
            else if (b !== undefined) {
                var toAppend = WebInspector.linkifyStringAsFragment(b.toString());
                if (currentStyle) {
                    var wrapper = document.createElement("span");
                    for (var key in currentStyle)
                        wrapper.style[key] = currentStyle[key];
                    wrapper.appendChild(toAppend);
                    toAppend = wrapper;
                }

                a.appendChild(toAppend);
            }
            return a;
        }

        // String.format does treat formattedResult like a Builder, result is an object.
        return String.format(parameters[0].description, parameters.slice(1), formatters, formattedResult, append);
    }

    _shouldShowStackTrace()
    {
        if (!this._message.stackTrace.callFrames.length)
            return false;

        return this._message.source === WebInspector.ConsoleMessage.MessageSource.Network
            || this._message.level === WebInspector.ConsoleMessage.MessageLevel.Error
            || this._message.type === WebInspector.ConsoleMessage.MessageType.Trace;
    }

    _shouldHideURL(url)
    {
        return url === "undefined" || url === "[native code]";
    }

    _linkifyLocation(url, lineNumber, columnNumber)
    {
        return WebInspector.linkifyLocation(url, lineNumber, columnNumber, "console-message-url");
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

        // If no table data show nothing.
        if (!flatValues.length)
            return element;

        // FIXME: Should we output something extra if the preview is lossless?

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
        case WebInspector.ConsoleMessage.MessageLevel.Info:
            return "Info";
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

    _makeExpandable()
    {
        if (this._expandable)
            return;

        this._expandable = true;

        this._element.classList.add("expandable");

        this._boundClickHandler = this.toggle.bind(this);
        this._messageTextElement.addEventListener("click", this._boundClickHandler);
    }
};
