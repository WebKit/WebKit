/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WI.View = class View extends WI.Object
{
    constructor(element)
    {
        super();

        this._element = element || document.createElement("div");
        this._element.__view = this;
        this._parentView = null;
        this._subviews = [];
        this._dirty = false;
        this._dirtyDescendantsCount = 0;
        this._isAttachedToRoot = false;
        this._layoutReason = null;
        this._didInitialLayout = false;
    }

    // Static

    static fromElement(element)
    {
        if (!element || !(element instanceof HTMLElement))
            return null;

        if (element.__view instanceof WI.View)
            return element.__view;
        return null;
    }

    static rootView()
    {
        if (!WI.View._rootView) {
            // Since the root view is attached by definition, it does not go through the
            // normal view attachment process. Simply mark it as attached.
            WI.View._rootView = new WI.View(document.body);
            WI.View._rootView._isAttachedToRoot = true;
        }

        return WI.View._rootView;
    }

    // Public

    get element() { return this._element; }
    get layoutPending() { return this._dirty; }
    get parentView() { return this._parentView; }
    get subviews() { return this._subviews; }
    get isAttached() { return this._isAttachedToRoot; }

    isDescendantOf(view)
    {
        let parentView = this._parentView;
        while (parentView) {
            if (parentView === view)
                return true;
            parentView = parentView.parentView;
        }

        return false;
    }

    addSubview(view)
    {
        this.insertSubviewBefore(view, null);
    }

    insertSubviewBefore(view, referenceView)
    {
        console.assert(view instanceof WI.View);
        console.assert(!referenceView || referenceView instanceof WI.View);
        console.assert(view !== WI.View._rootView, "Root view cannot be a subview.");

        if (this._subviews.includes(view)) {
            console.assert(false, "Cannot add view that is already a subview.", view);
            return;
        }

        const beforeIndex = referenceView ? this._subviews.indexOf(referenceView) : this._subviews.length;
        if (beforeIndex === -1) {
            console.assert(false, "Cannot insert view. Invalid reference view.", referenceView);
            return;
        }

        this._subviews.insertAtIndex(view, beforeIndex);

        console.assert(!view.element.parentNode || this._element.contains(view.element.parentNode), "Subview DOM element must be a descendant of the parent view element.");
        if (!view.element.parentNode)
            this._element.insertBefore(view.element, referenceView ? referenceView.element : null);

        view._didMoveToParent(this);
    }

    removeSubview(view)
    {
        console.assert(view instanceof WI.View);
        console.assert(this._element.contains(view.element), "Subview DOM element must be a child of the parent view element.");

        let index = this._subviews.lastIndexOf(view);
        if (index === -1) {
            console.assert(false, "Cannot remove view which isn't a subview.", view);
            return;
        }

        this._subviews.splice(index, 1);
        view.element.remove();

        view._didMoveToParent(null);
    }

    removeAllSubviews()
    {
        for (let subview of this._subviews)
            subview._didMoveToParent(null);

        this._subviews = [];
        this._element.removeChildren();
    }

    replaceSubview(oldView, newView)
    {
        console.assert(oldView !== newView, "Cannot replace subview with itself.");
        if (oldView === newView)
            return;

        this.insertSubviewBefore(newView, oldView);
        this.removeSubview(oldView);
    }

    updateLayout(layoutReason)
    {
        this.cancelLayout();

        this._setLayoutReason(layoutReason);
        this._layoutSubtree();
    }

    updateLayoutIfNeeded(layoutReason)
    {
        if (!this._dirty && this._didInitialLayout)
            return;

        this.updateLayout(layoutReason);
    }

    needsLayout(layoutReason)
    {
        this._setLayoutReason(layoutReason);

        if (this._dirty)
            return;

        WI.View._scheduleLayoutForView(this);
    }

    cancelLayout()
    {
        WI.View._cancelScheduledLayoutForView(this);
    }

    // Protected

    get layoutReason() { return this._layoutReason; }
    get didInitialLayout() { return this._didInitialLayout; }

    attached()
    {
        // Implemented by subclasses.
    }

    detached()
    {
        // Implemented by subclasses.
    }

    initialLayout()
    {
        // Implemented by subclasses.

        // Called once when the view is shown for the first time.
        // Views with complex DOM subtrees should create UI elements in
        // initialLayout rather than at construction time.
    }

    layout()
    {
        // Implemented by subclasses.

        // Not responsible for recursing to child views.
        // Should not be called directly; use updateLayout() instead.
    }

    didLayoutSubtree()
    {
        // Implemented by subclasses.

        // Called after the view and its entire subtree have finished layout.
    }

    sizeDidChange()
    {
        // Implemented by subclasses.

        // Called after initialLayout, and before layout.
    }

    // Private

    _didMoveToParent(parentView)
    {
        this._parentView = parentView;

        let isAttachedToRoot = this.isDescendantOf(WI.View._rootView);
        this._didMoveToWindow(isAttachedToRoot);

        if (!this._parentView)
            return;

        let pendingLayoutsCount = this._dirtyDescendantsCount;
        if (this._dirty)
            pendingLayoutsCount++;

        let view = this._parentView;
        while (view) {
            view._dirtyDescendantsCount += pendingLayoutsCount;
            view = view.parentView;
        }
    }

    _didMoveToWindow(isAttachedToRoot)
    {
        if (this._isAttachedToRoot === isAttachedToRoot)
            return;

        this._isAttachedToRoot = isAttachedToRoot;
        if (this._isAttachedToRoot) {
            WI.View._scheduleLayoutForView(this);
            this.attached();
        } else {
            if (this._dirty)
                this.cancelLayout();
            this.detached();
        }

        for (let view of this._subviews)
            view._didMoveToWindow(isAttachedToRoot);
    }

    _layoutSubtree()
    {
        this._dirty = false;
        this._dirtyDescendantsCount = 0;
        let isInitialLayout = !this._didInitialLayout;

        if (!this._didInitialLayout) {
            this.initialLayout();
            this._didInitialLayout = true;
        }

        if (this._layoutReason === WI.View.LayoutReason.Resize)
            this.sizeDidChange();

        this.layout();

        if (WI.settings.enableLayoutFlashing.value)
            this._drawLayoutFlashingOutline(isInitialLayout);

        for (let view of this._subviews) {
            view._setLayoutReason(this._layoutReason);
            view._layoutSubtree();
        }

        this._layoutReason = null;

        this.didLayoutSubtree();
    }

    _setLayoutReason(layoutReason)
    {
        if (this._layoutReason === WI.View.LayoutReason.Resize)
            return;

        this._layoutReason = layoutReason || WI.View.LayoutReason.Dirty;
    }

    _drawLayoutFlashingOutline(isInitialLayout)
    {
        if (this._layoutFlashingTimeout)
            clearTimeout(this._layoutFlashingTimeout);
        else
            this._layoutFlashingPreviousOutline = this._element.style.outline;

        let hue = isInitialLayout ? 20 : 40;
        this._element.style.outline = `1px solid hsla(${hue}, 100%, 51%, 0.8)`;

        this._layoutFlashingTimeout = setTimeout(() => {
            if (this._element)
                this._element.style.outline = this._layoutFlashingPreviousOutline;

            this._layoutFlashingTimeout = undefined;
            this._layoutFlashingPreviousOutline = null;
        }, 500);
    }

    // Layout controller logic

    static _scheduleLayoutForView(view)
    {
        view._dirty = true;

        let parentView = view.parentView;
        while (parentView) {
            parentView._dirtyDescendantsCount++;
            parentView = parentView.parentView;
        }

        if (!view._isAttachedToRoot)
            return;

        if (WI.View._scheduledLayoutUpdateIdentifier)
            return;

        WI.View._scheduledLayoutUpdateIdentifier = requestAnimationFrame(WI.View._visitViewTreeForLayout);
    }

    static _cancelScheduledLayoutForView(view)
    {
        let cancelledLayoutsCount = view._dirtyDescendantsCount;
        if (view.layoutPending)
            cancelledLayoutsCount++;

        let parentView = view.parentView;
        while (parentView) {
            parentView._dirtyDescendantsCount = Math.max(0, parentView._dirtyDescendantsCount - cancelledLayoutsCount);
            parentView = parentView.parentView;
        }

        view._dirty = false;

        if (!WI.View._scheduledLayoutUpdateIdentifier)
            return;

        let rootView = WI.View._rootView;
        if (!rootView || rootView._dirtyDescendantsCount)
            return;

        // No views need layout, so cancel the pending requestAnimationFrame.
        cancelAnimationFrame(WI.View._scheduledLayoutUpdateIdentifier);
        WI.View._scheduledLayoutUpdateIdentifier = undefined;
    }

    static _visitViewTreeForLayout()
    {
        console.assert(WI.View._rootView, "Cannot layout view tree without a root.");

        WI.View._scheduledLayoutUpdateIdentifier = undefined;

        let views = [WI.View._rootView];
        while (views.length) {
            let view = views.shift();
            if (view.layoutPending)
                view._layoutSubtree();
            else if (view._dirtyDescendantsCount) {
                views = views.concat(view.subviews);
                view._dirtyDescendantsCount = 0;
            }
        }
    }
};

WI.View.LayoutReason = {
    Dirty: Symbol("layout-reason-dirty"),
    Resize: Symbol("layout-reason-resize")
};

WI.View._rootView = null;
WI.View._scheduledLayoutUpdateIdentifier = undefined;
