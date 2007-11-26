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

WebInspector.Panel = function(views)
{
    this._visible = false;

    this.element = document.createElement("div");
    this.element.className = "panel";

    this.views = {};
    this.viewButtons = [];

    if (views) {
        var selectViewFunction = function(event)
        {
            var clickedView = event.currentTarget.view;
            clickedView.panel.currentView = clickedView;
        };

        for (var i = 0; i < views.length; ++i) {
            var view = views[i];
            view.panel = this;

            view.buttonElement = document.createElement("button");
            view.buttonElement.title = view.title;
            view.buttonElement.addEventListener("click", selectViewFunction, false);
            view.buttonElement.appendChild(document.createElement("img"));
            view.buttonElement.view = view;

            view.contentElement = document.createElement("div");
            view.contentElement.className = "content " + view.name;

            this.views[view.name] = view;
            this.viewButtons.push(view.buttonElement);
            this.element.appendChild(view.contentElement);
        }
    }
}

WebInspector.Panel.prototype = {
    show: function()
    {
        this._visible = true;
        if (!this.element.parentNode)
            this.attach();
        this.element.addStyleClass("selected");
        this.updateToolbar();
        if (this.currentView && this.currentView.show)
            this.currentView.show();
    },

    hide: function()
    {
        if (this.currentView && this.currentView.hide)
            this.currentView.hide();
        document.getElementById("toolbarButtons").removeChildren();
        this.element.removeStyleClass("selected");
        this._visible = false;
    },

    updateToolbar: function()
    {
        var buttonContainer = document.getElementById("toolbarButtons");
        buttonContainer.removeChildren();

        var buttons = this.viewButtons;
        if (buttons.length < 2)
            return;

        for (var i = 0; i < buttons.length; ++i) {
            var button = buttons[i];

            if (i === 0)
                button.addStyleClass("first");
            else if (i === (buttons.length - 1))
                button.addStyleClass("last");

            if (i) {
                var divider = document.createElement("img");
                divider.className = "split-button-divider";
                buttonContainer.appendChild(divider);
            }

            button.addStyleClass("split-button");
            button.addStyleClass("view-button-" + button.title.toLowerCase());

            buttonContainer.appendChild(button);
        }
    },

    attach: function()
    {
        document.getElementById("panels").appendChild(this.element);
    },

    detach: function()
    {
        if (WebInspector.currentPanel === this)
            WebInspector.currentPanel = null;
        if (this.element && this.element.parentNode)
            this.element.parentNode.removeChild(this.element);
    },

    get currentView()
    {
        return this._currentView;
    },

    set currentView(x)
    {
        if (typeof x === "string" || x instanceof String)
            x = this.views[x];

        if (this._currentView === x)
            return;

        if (this !== x.panel) {
            console.error("Set currentView to a view " + x.title + " whose panel is not this panel");
            return;
        }

        if (this._currentView) {
            this._currentView.buttonElement.removeStyleClass("selected");
            this._currentView.contentElement.removeStyleClass("selected");
            if (this._currentView.hide)
                this._currentView.hide();
        }

        this._currentView = x;

        if (x) {
            x.buttonElement.addStyleClass("selected");
            x.contentElement.addStyleClass("selected");
            if (x.show)
                x.show();
        }
    },

    get visible()
    {
        return this._visible;
    },

    set visible(x)
    {
        if (this._visible === x)
            return;

        if (x)
            this.show();
        else
            this.hide();
    }
}
