/*
 * Copyright (C) 2007, 2008, 2013 Apple Inc.  All rights reserved.
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

WebInspector.ConsoleGroup = function(parentGroup)
{
    WebInspector.Object.call(this);

    this.parentGroup = parentGroup;
};

WebInspector.ConsoleGroup.prototype = {
    constructor: WebInspector.ConsoleGroup,

    // Public

    render: function(message)
    {
        var groupElement = document.createElement("div");
        groupElement.className = "console-group";
        groupElement.group = this;
        this.element = groupElement;

        var titleElement = message.toMessageElement();
        titleElement.classList.add(WebInspector.LogContentView.ItemWrapperStyleClassName);
        titleElement.addEventListener("click", this._titleClicked.bind(this));
        titleElement.addEventListener("mousedown", this._titleMouseDown.bind(this));
        if (groupElement && message.type === WebInspector.LegacyConsoleMessage.MessageType.StartGroupCollapsed)
            groupElement.classList.add("collapsed");

        groupElement.appendChild(titleElement);

        var messagesElement = document.createElement("div");
        this._messagesElement = messagesElement;
        messagesElement.className = "console-group-messages";
        groupElement.appendChild(messagesElement);

        return groupElement;
    },

    addMessage: function(message)
    {
        var element = message.toMessageElement();
        element.classList.add(WebInspector.LogContentView.ItemWrapperStyleClassName);
        this.append(element);
    },

    append: function(messageElement)
    {
        this._messagesElement.appendChild(messageElement);
    },

    // Private

    _titleMouseDown: function(event)
    {
        event.preventDefault();
    },

    _titleClicked: function(event)
    {
        var groupTitleElement = event.target.enclosingNodeOrSelfWithClass("console-group-title");
        if (groupTitleElement) {
            var groupElement = groupTitleElement.enclosingNodeOrSelfWithClass("console-group");
            if (groupElement)
                if (groupElement.classList.contains("collapsed"))
                    groupElement.classList.remove("collapsed");
                else
                    groupElement.classList.add("collapsed");
            groupTitleElement.scrollIntoViewIfNeeded(true);
        }
    }
};

WebInspector.ConsoleGroup.prototype.__proto__ = WebInspector.Object.prototype;
