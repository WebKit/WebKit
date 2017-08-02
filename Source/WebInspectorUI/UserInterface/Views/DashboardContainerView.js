/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

WI.DashboardContainerView = class DashboardContainerView extends WI.Object
{
    constructor()
    {
        super();

        this._toolbarItem = new WI.NavigationItem("dashboard-container", "group", WI.UIString("Activity Viewer"));

        this._advanceForwardArrowElement = this._toolbarItem.element.appendChild(document.createElement("div"));
        this._advanceForwardArrowElement.className = "advance-arrow advance-forward";
        this._advanceBackwardArrowElement = this._toolbarItem.element.appendChild(document.createElement("div"));
        this._advanceBackwardArrowElement.className = "advance-arrow advance-backward";

        this._advanceForwardArrowElement.addEventListener("click", this._advanceForwardArrowClicked.bind(this));
        this._advanceBackwardArrowElement.addEventListener("click", this._advanceBackwardArrowClicked.bind(this));

        // Represents currently open dashboards, with the most recent entries appended to the end.
        this._dashboardStack = [];
        this._currentIndex = -1;

        this._updateAdvanceArrowVisibility();
    }

    // Public

    get toolbarItem()
    {
        return this._toolbarItem;
    }

    get currentDashboardView()
    {
        if (this._currentIndex === -1)
            return null;

        return this._dashboardStack[this._currentIndex];
    }

    showDashboardViewForRepresentedObject(representedObject)
    {
        var dashboardView = this._dashboardViewForRepresentedObject(representedObject);
        if (!dashboardView)
            return null;

        if (this.currentDashboardView === dashboardView)
            return dashboardView;

        var index = this._dashboardStack.indexOf(dashboardView);
        this._showDashboardAtIndex(index);
        return dashboardView;
    }

    hideDashboardViewForRepresentedObject(representedObject)
    {
        var onlyReturnExistingViews = true;
        var dashboardView = this._dashboardViewForRepresentedObject(representedObject, onlyReturnExistingViews);

        if (this.currentDashboardView !== dashboardView)
            return;

        console.assert(this._currentIndex > 0);
        this._showDashboardAtIndex(this._currentIndex - 1);
    }

    closeDashboardViewForRepresentedObject(representedObject)
    {
        var onlyReturnExistingViews = true;
        var dashboardView = this._dashboardViewForRepresentedObject(representedObject, onlyReturnExistingViews);
        if (!dashboardView)
            return;

        this._closeDashboardView(dashboardView);
    }

    // Private

    _advanceForwardArrowClicked()
    {
        this._showDashboardAtIndex(this._currentIndex + 1);
    }

    _advanceBackwardArrowClicked()
    {
        this._showDashboardAtIndex(this._currentIndex - 1);
    }

    _dismissAdvanceArrows()
    {
        this._advanceForwardArrowElement.classList.add(WI.DashboardContainerView.InactiveStyleClassName);
        this._advanceBackwardArrowElement.classList.add(WI.DashboardContainerView.InactiveStyleClassName);
    }

    _updateAdvanceArrowVisibility()
    {
        var canAdvanceForward = this._currentIndex < this._dashboardStack.length - 1;
        var canAdvanceBackward = this._currentIndex > 0;
        this._advanceForwardArrowElement.classList.toggle(WI.DashboardContainerView.InactiveStyleClassName, !canAdvanceForward);
        this._advanceBackwardArrowElement.classList.toggle(WI.DashboardContainerView.InactiveStyleClassName, !canAdvanceBackward);
    }

    _dashboardViewForRepresentedObject(representedObject, onlyReturnExistingViews)
    {
        console.assert(representedObject);

        // Iterate over all known dashboard views and see if any are for this object.
        for (var dashboardView of this._dashboardStack) {
            if (dashboardView.representedObject === representedObject)
                return dashboardView;
        }

        if (onlyReturnExistingViews)
            return null;

        // No existing content view found, make a new one.
        dashboardView = WI.DashboardView.create(representedObject);

        console.assert(dashboardView, "Unknown representedObject", representedObject);
        if (!dashboardView)
            return null;

        this._dashboardStack.push(dashboardView);
        this._toolbarItem.element.appendChild(dashboardView.element);

        return dashboardView;
    }

    _showDashboardAtIndex(index)
    {
        console.assert(index >= 0 && index <= this._dashboardStack.length - 1);

        if (this._currentIndex === index)
            return;

        var advanceDirection = null;
        if (this._currentIndex < index)
            advanceDirection = WI.DashboardContainerView.AdvanceDirection.Forward;
        else
            advanceDirection = WI.DashboardContainerView.AdvanceDirection.Backward;
        var initialDirection = WI.DashboardContainerView.AdvanceDirection.None;
        var isInitialDashboard = this._currentIndex === -1;
        if (!isInitialDashboard)
            this._hideDashboardView(this.currentDashboardView, advanceDirection);

        this._currentIndex = index;
        this._showDashboardView(this.currentDashboardView, isInitialDashboard ? initialDirection : advanceDirection);
    }

    _showDashboardView(dashboardView, advanceDirection)
    {
        console.assert(dashboardView instanceof WI.DashboardView);

        dashboardView.shown();
        this._dismissAdvanceArrows();

        var animationClass = null;
        if (advanceDirection === WI.DashboardContainerView.AdvanceDirection.Forward)
            animationClass = WI.DashboardContainerView.ForwardIncomingDashboardStyleClassName;
        if (advanceDirection === WI.DashboardContainerView.AdvanceDirection.Backward)
            animationClass = WI.DashboardContainerView.BackwardIncomingDashboardStyleClassName;

        var container = this;
        dashboardView.element.classList.add(WI.DashboardContainerView.VisibleDashboardStyleClassName);

        function animationEnded(event) {
            if (event.target !== dashboardView.element)
                return;

            dashboardView.element.removeEventListener("animationend", animationEnded);
            dashboardView.element.classList.remove(animationClass);
            container._updateAdvanceArrowVisibility();
        }

        if (animationClass) {
            dashboardView.element.classList.add(animationClass);
            dashboardView.element.addEventListener("animationend", animationEnded);
        }

        return dashboardView;
    }

    _hideDashboardView(dashboardView, advanceDirection, callback)
    {
        console.assert(dashboardView instanceof WI.DashboardView);
        console.assert(this.currentDashboardView === dashboardView);

        dashboardView.hidden();
        this._dismissAdvanceArrows();

        var animationClass = null;
        if (advanceDirection === WI.DashboardContainerView.AdvanceDirection.Forward)
            animationClass = WI.DashboardContainerView.ForwardOutgoingDashboardStyleClassName;
        if (advanceDirection === WI.DashboardContainerView.AdvanceDirection.Backward)
            animationClass = WI.DashboardContainerView.BackwardOutgoingDashboardStyleClassName;

        var container = this;

        function animationEnded(event) {
            if (event.target !== dashboardView.element)
                return;

            dashboardView.element.removeEventListener("animationend", animationEnded);
            dashboardView.element.classList.remove(animationClass);
            dashboardView.element.classList.remove(WI.DashboardContainerView.VisibleDashboardStyleClassName);
            container._updateAdvanceArrowVisibility();

            if (typeof callback === "function")
                callback();
        }

        if (animationClass) {
            dashboardView.element.classList.add(animationClass);
            dashboardView.element.addEventListener("animationend", animationEnded);
        } else
            dashboardView.element.classList.remove(WI.DashboardContainerView.VisibleDashboardStyleClassName);
    }

    _closeDashboardView(dashboardView)
    {
        console.assert(dashboardView instanceof WI.DashboardView);

        function dissociateDashboardView() {
            dashboardView.closed();
            dashboardView.element.parentNode.removeChild(dashboardView.element);
        }

        var index = this._dashboardStack.indexOf(dashboardView);

        if (this.currentDashboardView === dashboardView) {
            var direction = WI.DashboardContainerView.AdvanceDirection.Backward;
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

        this._updateAdvanceArrowVisibility();
    }
};

WI.DashboardContainerView.VisibleDashboardStyleClassName = "visible";
WI.DashboardContainerView.InactiveStyleClassName = "inactive";

WI.DashboardContainerView.AdvanceDirection = {
    Forward: Symbol("dashboard-container-view-advance-direction-forward"),
    Backward: Symbol("dashboard-container-view-advance-direction-backward"),
    None: Symbol("dashboard-container-view-advance-direction-none")
};

WI.DashboardContainerView.ForwardIncomingDashboardStyleClassName = "slide-in-down";
WI.DashboardContainerView.BackwardIncomingDashboardStyleClassName = "slide-in-up";
WI.DashboardContainerView.ForwardOutgoingDashboardStyleClassName = "slide-out-down";
WI.DashboardContainerView.BackwardOutgoingDashboardStyleClassName = "slide-out-up";
