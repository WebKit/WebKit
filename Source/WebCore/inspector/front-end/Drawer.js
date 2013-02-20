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

/**
 * @constructor
 */
WebInspector.Drawer = function()
{
    this.element = document.getElementById("drawer");
    this._savedHeight = 200; // Default.
    this._mainElement = document.getElementById("main");
    this._toolbarElement = document.getElementById("toolbar");

    this._floatingStatusBarContainer = document.getElementById("floating-status-bar-container");
    WebInspector.installDragHandle(this._floatingStatusBarContainer, this._startStatusBarDragging.bind(this), this._statusBarDragging.bind(this), this._endStatusBarDragging.bind(this), "row-resize");

    this._drawerContentsElement = document.createElement("div");
    this._drawerContentsElement.id = "drawer-contents";
    this._drawerContentsElement.className = "drawer-contents";
    this.element.appendChild(this._drawerContentsElement);
    this._viewStatusBar = document.createElement("div");
    this._bottomStatusBar = document.getElementById("bottom-status-bar-container");
}

WebInspector.Drawer.AnimationType = {
        Immediately: 0,
        Normal: 1,
        Slow: 2
}

WebInspector.Drawer.prototype = {
    get visible()
    {
        return !!this._view;
    },

    _constrainHeight: function(height)
    {
        return Number.constrain(height, Preferences.minConsoleHeight, window.innerHeight - this._mainElement.totalOffsetTop() - Preferences.minConsoleHeight);
    },

    show: function(view, animationType)
    {
        this.immediatelyFinishAnimation();

        var drawerWasVisible = this.visible;

        if (this._view) {
            this._view.detach();
            this._drawerContentsElement.removeChildren();
        }

        this._view = view;

        var statusBarItems = this._view.statusBarItems || [];
        this._viewStatusBar.removeChildren();
        for (var i = 0; i < statusBarItems.length; ++i)
            this._viewStatusBar.appendChild(statusBarItems[i]);

        document.body.addStyleClass("drawer-visible");
        this._floatingStatusBarContainer.insertBefore(document.getElementById("panel-status-bar"), this._floatingStatusBarContainer.firstElementChild);
        this._bottomStatusBar.appendChild(this._viewStatusBar);
        this._view.detach();
        this._view.markAsRoot();
        this._view.show(this._drawerContentsElement);

        if (drawerWasVisible)
            return;
        
        var height = this._constrainHeight(this._savedHeight || this.element.offsetHeight);
        var animations = [
            {element: this.element, end: {height: height}},
            {element: this._mainElement, end: {bottom: height}},
            {element: this._floatingStatusBarContainer, start: {"padding-left": this._bottomStatusBar.offsetLeft}, end: {"padding-left": 0}},
            {element: this._viewStatusBar, start: {opacity: 0}, end: {opacity: 1}}
        ];

        function animationFinished()
        {
            WebInspector.inspectorView.currentPanel().doResize();
            if (this._view && this._view.afterShow)
                this._view.afterShow();
            delete this._currentAnimation;
        }

        this._currentAnimation = WebInspector.animateStyle(animations, this._animationDuration(animationType), animationFinished.bind(this));
        if (animationType === WebInspector.Drawer.AnimationType.Immediately)
            this._currentAnimation.forceComplete();
    },

    hide: function(animationType)
    {
        this.immediatelyFinishAnimation();
        if (!this.visible)
            return;

        this._savedHeight = this.element.offsetHeight;

        WebInspector.restoreFocusFromElement(this.element);

        // Temporarily set properties and classes to mimic the post-animation values so panels
        // like Elements in their updateStatusBarItems call will size things to fit the final location.
        document.body.removeStyleClass("drawer-visible");
        WebInspector.inspectorView.currentPanel().statusBarResized();
        document.body.addStyleClass("drawer-visible");

        var animations = [
            {element: this._mainElement, end: {bottom: 0}},
            {element: this.element, end: {height: 0}},
            {element: this._floatingStatusBarContainer, start: {"padding-left": 0}, end: {"padding-left": this._bottomStatusBar.offsetLeft} },
            {element: this._viewStatusBar, start: {opacity: 1}, end: {opacity: 0}}
        ];

        function animationFinished()
        {
            WebInspector.inspectorView.currentPanel().doResize();
            this._view.detach();
            delete this._view;
            this._bottomStatusBar.removeChildren();
            this._bottomStatusBar.appendChild(document.getElementById("panel-status-bar"));
            this._drawerContentsElement.removeChildren();
            document.body.removeStyleClass("drawer-visible");
            delete this._currentAnimation;
        }

        this._currentAnimation = WebInspector.animateStyle(animations, this._animationDuration(animationType), animationFinished.bind(this));
        if (animationType === WebInspector.Drawer.AnimationType.Immediately)
            this._currentAnimation.forceComplete();
    },

    resize: function()
    {
        if (!this.visible)
            return;

        this._view.storeScrollPositions();
        var height = this._constrainHeight(parseInt(this.element.style.height, 10));
        this._mainElement.style.bottom = height + "px";
        this.element.style.height = height + "px";
        this._view.doResize();
    },

    immediatelyFinishAnimation: function()
    {
        if (this._currentAnimation)
            this._currentAnimation.forceComplete();
    },

    _animationDuration: function(animationType)
    {
        switch (animationType) {
        case WebInspector.Drawer.AnimationType.Slow:
            return 2000;
        case WebInspector.Drawer.AnimationType.Normal:
            return 250;
        default:
            return 0;
        }
    },

    /**
     * @return {boolean}
     */
    _startStatusBarDragging: function(event)
    {
        if (!this.visible || event.target !== this._floatingStatusBarContainer)
            return false;

        this._view.storeScrollPositions();
        this._statusBarDragOffset = event.pageY - this.element.totalOffsetTop();
        return true;
    },

    _statusBarDragging: function(event)
    {
        var height = window.innerHeight - event.pageY + this._statusBarDragOffset;
        height = Number.constrain(height, Preferences.minConsoleHeight, window.innerHeight - this._mainElement.totalOffsetTop() - Preferences.minConsoleHeight);

        this._mainElement.style.bottom = height + "px";
        this.element.style.height = height + "px";
        if (WebInspector.inspectorView.currentPanel())
            WebInspector.inspectorView.currentPanel().doResize();
        this._view.doResize();

        event.consume(true);
    },

    _endStatusBarDragging: function(event)
    {
        this._savedHeight = this.element.offsetHeight;
        delete this._statusBarDragOffset;

        event.consume();
    }
}

/**
 * @type {WebInspector.Drawer}
 */
WebInspector.drawer = null;
