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

WebInspector.SourcePanel = function(resource, views)
{
    var allViews = [{ title: WebInspector.UIString("Source"), name: "source" }];
    if (views)
        allViews = allViews.concat(views);

    WebInspector.ResourcePanel.call(this, resource, allViews);

    this.currentView = this.views.source;

    var sourceView = this.views.source;

    sourceView.messages = [];
    sourceView.frameNeedsSetup = true;

    sourceView.frameElement = document.createElement("iframe");
    sourceView.frameElement.setAttribute("viewsource", "true");
    sourceView.contentElement.appendChild(sourceView.frameElement);
}

WebInspector.SourcePanel.prototype = {
    show: function()
    {
        WebInspector.ResourcePanel.prototype.show.call(this);
        this.setupSourceFrameIfNeeded();
    },

    setupSourceFrameIfNeeded: function()
    {
        if (this.views.source.frameNeedsSetup) {
            this.attach();

            InspectorController.addSourceToFrame(this.resource.identifier, this.views.source.frameElement);
            WebInspector.addMainEventListeners(this.views.source.frameElement.contentDocument);

            var length = this.views.source.messages;
            for (var i = 0; i < length; ++i)
                this._addMessageToSource(this.views.source.messages[i]);

            delete this.views.source.frameNeedsSetup;
        }
    },

    sourceRow: function(lineNumber)
    {
        this.setupSourceFrameIfNeeded();

        var doc = this.views.source.frameElement.contentDocument;
        var rows = doc.getElementsByTagName("table")[0].rows;

        // Line numbers are a 1-based index, but the rows collection is 0-based.
        --lineNumber;
        if (lineNumber >= rows.length)
            lineNumber = rows.length - 1;

        return rows[lineNumber];
    },

    showSourceLine: function(lineNumber)
    {
        var row = this.sourceRow(lineNumber);
        if (!row)
            return;
        this.currentView = this.views.source;
        row.scrollIntoView(true);
    },

    addMessageToSource: function(msg)
    {
        this.views.source.messages.push(msg);
        if (!this.views.source.frameNeedsSetup)
            this._addMessageToSource(msg);
    },

    _addMessageToSource: function(msg)
    {
        var row = this.sourceRow(msg.line);
        if (!row)
            return;

        var doc = this.views.source.frameElement.contentDocument;
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

WebInspector.SourcePanel.prototype.__proto__ = WebInspector.ResourcePanel.prototype;
