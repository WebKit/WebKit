/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.DashboardContainerView = function() {
    WebInspector.Object.call(this);

    this._toolbarItem = new WebInspector.NavigationItem("dashboard-container", "group", WebInspector.UIString("Activity Viewer"));

    // Represents currently open dashboards, with the most recent entries appended to the end.
    this._dashboardStack = [];
    this._currentIndex = -1;
};

WebInspector.DashboardContainerView.VisibleDashboardStyleClassName = "visible";

WebInspector.DashboardContainerView.AdvanceDirection = {
    Forward: "dashboard-container-view-advance-direction-forward",
    Backward: "dashboard-container-view-advance-direction-backward",
    None: "dashboard-container-view-advance-direction-none"
};

WebInspector.DashboardContainerView.ForwardIncomingDashboardStyleClassName = "slide-in-down";
WebInspector.DashboardContainerView.BackwardIncomingDashboardStyleClassName = "slide-in-up";
WebInspector.DashboardContainerView.ForwardOutgoingDashboardStyleClassName = "slide-out-down";
WebInspector.DashboardContainerView.BackwardOutgoingDashboardStyleClassName = "slide-out-up";

WebInspector.DashboardContainerView.prototype = {
    constructor: WebInspector.DashboardContainerView,
    __proto__: WebInspector.Object.prototype,

    // Public

    get toolbarItem()
    {
        return this._toolbarItem;
    },

    get currentDashboardView()
    {
        if (this._currentIndex === -1)
            return null;

        return this._dashboardStack[this._currentIndex];
    },

    showDashboardViewForRepresentedObject: function(representedObject)
    {
        var dashboardView = this._dashboardViewForRepresentedObject(representedObject);
        if (!dashboardView)
            return null;

        if (this.currentDashboardView === dashboardView)
            return;

        var index = this._dashboardStack.indexOf(dashboardView);
        this._showDashboardAtIndex(index);
        return dashboardView;
    },

    hideDashboardViewForRepresentedObject: function(representedObject)
    {
        var onlyReturnExistingViews = true;
        var dashboardView = this._dashboardViewForRepresentedObject(representedObject, onlyReturnExistingViews);

        if (this.currentDashboardView !== dashboardView)
            return;

        console.assert(this._currentIndex > 0);
        this._showDashboardAtIndex(this._currentIndex - 1);
    },

    closeDashboardViewForRepresentedObject: function(representedObject)
    {
        var onlyReturnExistingViews = true;
        var dashboardView = this._dashboardViewForRepresentedObject(representedObject, onlyReturnExistingViews);
        if (!dashboardView)
            return null;

        this._closeDashboardView(dashboardView);
    },

    // Private

    _dashboardViewForRepresentedObject: function(representedObject, onlyReturnExistingViews)
    {
        console.assert(representedObject);

        // Iterate over all known dashboard views and see if any are for this object.
        for (var dashboardView of this._dashboardStack) {
            if (dashboardView.representedObject === representedObject)
                return dashboardView;
        }

        if (onlyReturnExistingViews)
            return null;

        try {
            // No existing content view found, make a new one.
            dashboardView = new WebInspector.DashboardView(representedObject);
        } catch (e) {
            console.error(e);
            return null;
        }

        this._dashboardStack.push(dashboardView);
        this._toolbarItem.element.appendChild(dashboardView.element);

        return dashboardView;
    },

    _showDashboardAtIndex: function(index)
    {
        console.assert(index >= 0 && index <= this._dashboardStack.length - 1);

        if (this._currentIndex === index)
            return;

        var advanceDirection = WebInspector.DashboardContainerView.AdvanceDirection.Forward;
        var initialDirection = WebInspector.DashboardContainerView.AdvanceDirection.None;
        var isFirstDashboard = this._currentIndex === -1;
        if (!isFirstDashboard)
            this._hideDashboardView(this.currentDashboardView, advanceDirection);

        this._currentIndex = index;
        this._showDashboardView(this.currentDashboardView, isFirstDashboard ? initialDirection : advanceDirection);
    },

    _showDashboardView: function(dashboardView, advanceDirection)
    {
        console.assert(dashboardView instanceof WebInspector.DashboardView);

        dashboardView.shown();

        var animationClass = null;
        if (advanceDirection === WebInspector.DashboardContainerView.AdvanceDirection.Forward)
            animationClass = WebInspector.DashboardContainerView.ForwardIncomingDashboardStyleClassName;
        if (advanceDirection === WebInspector.DashboardContainerView.AdvanceDirection.Backward)
            animationClass = WebInspector.DashboardContainerView.BackwardIncomingDashboardStyleClassName;

        if (animationClass) {
            function animationEnded(event) {
                if (event.target !== dashboardView.element)
                    return;

                dashboardView.element.removeEventListener("webkitAnimationEnd", animationEnded);
                dashboardView.element.classList.remove(animationClass);
                dashboardView.element.classList.add(WebInspector.DashboardContainerView.VisibleDashboardStyleClassName);
            }
            dashboardView.element.classList.add(animationClass);
            dashboardView.element.addEventListener("webkitAnimationEnd", animationEnded);
        } else
            dashboardView.element.classList.add(WebInspector.DashboardContainerView.VisibleDashboardStyleClassName);

        return dashboardView;
    },

    _hideDashboardView: function(dashboardView, advanceDirection, callback)
    {
        console.assert(dashboardView instanceof WebInspector.DashboardView);
        console.assert(this.currentDashboardView === dashboardView);

        dashboardView.hidden();

        var animationClass = null;
        if (advanceDirection === WebInspector.DashboardContainerView.AdvanceDirection.Forward)
            animationClass = WebInspector.DashboardContainerView.ForwardOutgoingDashboardStyleClassName;
        if (advanceDirection === WebInspector.DashboardContainerView.AdvanceDirection.Backward)
            animationClass = WebInspector.DashboardContainerView.BackwardOutgoingDashboardStyleClassName;

        if (animationClass) {
            function animationEnded(event) {
                if (event.target !== dashboardView.element)
                    return;

                dashboardView.element.removeEventListener("webkitAnimationEnd", animationEnded);
                dashboardView.element.classList.remove(animationClass);
                dashboardView.element.classList.remove(WebInspector.DashboardContainerView.VisibleDashboardStyleClassName);

                if (typeof callback === "function")
                    callback();
            }
            dashboardView.element.classList.add(animationClass);
            dashboardView.element.addEventListener("webkitAnimationEnd", animationEnded);
        } else
            dashboardView.element.classList.remove(WebInspector.DashboardContainerView.VisibleDashboardStyleClassName);
    },

    _closeDashboardView: function(dashboardView)
    {
        console.assert(dashboardView instanceof WebInspector.DashboardView);

        function dissociateDashboardView() {
            dashboardView.closed();
            dashboardView.element.parentNode.removeChild(dashboardView.element);
        }

        var index = this._dashboardStack.indexOf(dashboardView);

        if (this.currentDashboardView === dashboardView) {
            var direction = WebInspector.DashboardContainerView.AdvanceDirection.Backward;
            this._hideDashboardView(this.currentDashboardView, direction, dissociateDashboardView);
            this._dashboardStack.splice(index, 1);
            --this._currentIndex;
            this._showDashboardView(this.currentDashboardView, direction);
            return;
        }

        this._dashboardStack.splice(index, 1);
        if (this._currentIndex > index)
            --this._currentIndex;
        dissociateDashboardView.call(this);
    }
};
