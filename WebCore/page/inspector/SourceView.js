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

WebInspector.SourceView = function(resource)
{
    WebInspector.ResourceView.call(this, resource);

    this.element.addStyleClass("source");

    this.messages = [];
    this._frameNeedsSetup = true;

    this.frameElement = document.createElement("iframe");
    this.frameElement.className = "source-view-frame";
    this.frameElement.setAttribute("viewsource", "true");
    this.contentElement.appendChild(this.frameElement);
}

WebInspector.SourceView.prototype = {
    show: function()
    {
        WebInspector.ResourceView.prototype.show.call(this);
        this.setupSourceFrameIfNeeded();
    },

    setupSourceFrameIfNeeded: function()
    {
        if (this._frameNeedsSetup) {
            this.attach();

            InspectorController.addSourceToFrame(this.resource.identifier, this.frameElement);
            WebInspector.addMainEventListeners(this.frameElement.contentDocument);

            var length = this.messages;
            for (var i = 0; i < length; ++i)
                this._addMessageToSource(this.messages[i]);

            delete this._frameNeedsSetup;
        }
    },

    sourceRow: function(lineNumber)
    {
        if (!lineNumber)
            return;

        this.setupSourceFrameIfNeeded();

        var doc = this.frameElement.contentDocument;
        var rows = doc.getElementsByTagName("table")[0].rows;

        // Line numbers are a 1-based index, but the rows collection is 0-based.
        --lineNumber;
        if (lineNumber >= rows.length)
            lineNumber = rows.length - 1;

        return rows[lineNumber];
    },

    showLine: function(lineNumber)
    {
        var row = this.sourceRow(lineNumber);
        if (!row)
            return;
        row.scrollIntoViewIfNeeded(true);
    },

    addMessageToSource: function(msg)
    {
        this.messages.push(msg);
        if (!this._frameNeedsSetup)
            this._addMessageToSource(msg);
    },

    _addMessageToSource: function(msg)
    {
        var row = this.sourceRow(msg.line);
        if (!row)
            return;

        var doc = this.frameElement.contentDocument;
        var cell = row.getElementsByTagName("td")[1];

        var errorDiv = cell.lastChild;
        if (!errorDiv || errorDiv.nodeName.toLowerCase() !== "div" || !errorDiv.hasStyleClass("webkit-html-message-bubble")) {
            errorDiv = doc.createElement("div");
            errorDiv.className = "webkit-html-message-bubble";
            cell.appendChild(errorDiv);
        }

        var imageURL;
        switch (msg.level) {
            case WebInspector.ConsoleMessage.MessageLevel.Error:
                errorDiv.addStyleClass("webkit-html-error-message");
                imageURL = "Images/errorIcon.png";
                break;
            case WebInspector.ConsoleMessage.MessageLevel.Warning:
                errorDiv.addStyleClass("webkit-html-warning-message");
                imageURL = "Images/warningIcon.png";
                break;
        }

        var lineDiv = doc.createElement("div");
        lineDiv.className = "webkit-html-message-line";
        errorDiv.appendChild(lineDiv);

        var image = doc.createElement("img");
        image.src = imageURL;
        image.className = "webkit-html-message-icon";
        lineDiv.appendChild(image);

        lineDiv.appendChild(doc.createTextNode(msg.message));
    }
}

WebInspector.SourceView.prototype.__proto__ = WebInspector.ResourceView.prototype;
