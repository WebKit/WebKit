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

WebInspector.Drawer = function()
{
    WebInspector.View.call(this, document.getElementById("drawer"));

    this.mainStatusBar = document.getElementById("main-status-bar");
    this.mainStatusBar.addEventListener("mousedown", this._startStatusBarDragging.bind(this), true);
    this.viewStatusBar = document.getElementById("other-drawer-status-bar-items");
}

WebInspector.Drawer.prototype = {
    get visibleView()
    {
        return this._visibleView;
    },

    set visibleView(x)
    {
        if (this._visibleView === x) {
            this.visible = !this.visible;
            return;
        }

        var firstTime = !this._visibleView;
        if (this._visibleView)
            this._visibleView.hide();

        this._visibleView = x;

        if (x && !firstTime) {
            this._safelyRemoveChildren();
            this.viewStatusBar.removeChildren(); // optimize this? call old.detach()
            x.attach(this.element, this.viewStatusBar);
            x.show();
            this.visible = true;
        }
    },

    show: function()
    {
        if (this._animating || this.visible)
            return;

        if (this.visibleView)
            this.visibleView.show();

        WebInspector.View.prototype.show.call(this);

        this._animating = true;

        document.body.addStyleClass("drawer-visible");

        var anchoredItems = document.getElementById("anchored-status-bar-items");

        var animations = [
            {element: document.getElementById("main"), end: {bottom: this.element.offsetHeight}},
            {element: document.getElementById("main-status-bar"), start: {"padding-left": anchoredItems.offsetWidth - 1}, end: {"padding-left": 0}},
            {element: document.getElementById("other-drawer-status-bar-items"), start: {opacity: 0}, end: {opacity: 1}}
        ];

        var consoleStatusBar = document.getElementById("drawer-status-bar");
        consoleStatusBar.insertBefore(anchoredItems, consoleStatusBar.firstChild);

        function animationFinished()
        {
            if ("updateStatusBarItems" in WebInspector.currentPanel)
                WebInspector.currentPanel.updateStatusBarItems();
            if (this.visibleView.afterShow)
                this.visibleView.afterShow();
            delete this._animating;
        }

        WebInspector.animateStyle(animations, window.event && window.event.shiftKey ? 2000 : 250, animationFinished.bind(this));
    },

    hide: function()
    {
        if (this._animating || !this.visible)
            return;

        WebInspector.View.prototype.hide.call(this);
        
        if (this.visibleView)
            this.visibleView.hide();

        this._animating = true;

        if (this.element === WebInspector.currentFocusElement || this.element.isAncestor(WebInspector.currentFocusElement))
            WebInspector.currentFocusElement = WebInspector.previousFocusElement;

        var anchoredItems = document.getElementById("anchored-status-bar-items");

        // Temporally set properties and classes to mimic the post-animation values so panels
        // like Elements in their updateStatusBarItems call will size things to fit the final location.
        document.getElementById("main-status-bar").style.setProperty("padding-left", (anchoredItems.offsetWidth - 1) + "px");
        document.body.removeStyleClass("drawer-visible");
        if ("updateStatusBarItems" in WebInspector.currentPanel)
            WebInspector.currentPanel.updateStatusBarItems();
        document.body.addStyleClass("drawer-visible");

        var animations = [
            {element: document.getElementById("main"), end: {bottom: 0}},
            {element: document.getElementById("main-status-bar"), start: {"padding-left": 0}, end: {"padding-left": anchoredItems.offsetWidth - 1}},
            {element: document.getElementById("other-drawer-status-bar-items"), start: {opacity: 1}, end: {opacity: 0}}
        ];

        function animationFinished()
        {
            var mainStatusBar = document.getElementById("main-status-bar");
            mainStatusBar.insertBefore(anchoredItems, mainStatusBar.firstChild);
            mainStatusBar.style.removeProperty("padding-left");
            document.body.removeStyleClass("drawer-visible");
            delete this._animating;
        }

        WebInspector.animateStyle(animations, window.event && window.event.shiftKey ? 2000 : 250, animationFinished.bind(this));
    },

    _safelyRemoveChildren: function()
    {
        var child = this.element.firstChild;
        while (child) {
            if (child.id !== "drawer-status-bar") {
                var moveTo = child.nextSibling;
                this.element.removeChild(child);
                child = moveTo;
            } else
                child = child.nextSibling;
        }
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
    }
}

WebInspector.Drawer.prototype.__proto__ = WebInspector.View.prototype;
