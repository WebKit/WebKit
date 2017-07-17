
class TracksPanel extends LayoutNode
{

    constructor()
    {
        super(`<div class="tracks-panel"></div>`);
        this._backgroundTint = new BackgroundTint;
        this._scrollableContainer = new LayoutNode(`<div class="scrollable-container"></div>`);
        this._rightX = 0;
        this._bottomY = 0;
        this._presented = false;
        
        this.children = [this._backgroundTint, this._scrollableContainer];
    }

    // Public

    get presented()
    {
        return this._presented;
    }

    presentInParent(node)
    {
        if (this._presented && this.parent === node)
            return;

        this._presented = true;

        this._scrollableContainer.children = this._childrenFromDataSource();

        node.addChild(this);

        this.element.removeEventListener("transitionend", this);
        this.element.classList.remove("fade-out");

        this._mousedownTarget().addEventListener("mousedown", this, true);
        window.addEventListener("keydown", this, true);

        this._focusedTrackNode = null;
    }

    hide()
    {
        if (!this._presented)
            return;

        this._presented = false;

        this._mousedownTarget().removeEventListener("mousedown", this, true);
        window.removeEventListener("keydown", this, true);

        this.element.addEventListener("transitionend", this);

        // Ensure a transition will indeed happen by starting it only on the next frame.
        window.requestAnimationFrame(() => { this.element.classList.add("fade-out"); });
    }

    get maxHeight()
    {
        return this._maxHeight;
    }

    set maxHeight(height)
    {
        if (this._maxHeight === height)
            return;

        this._maxHeight = height;
        this.markDirtyProperty("maxHeight")
    }

    get bottomY()
    {
        return this._bottomY;
    }

    set bottomY(bottomY)
    {
        if (this._bottomY === bottomY)
            return;

        this._bottomY = bottomY;
        this.markDirtyProperty("bottomY");
    }

    get rightX()
    {
        return this._rightX;
    }

    set rightX(x)
    {
        if (this._rightX === x)
            return;

        this._rightX = x;
        this.markDirtyProperty("rightX");
    }

    // Protected

    trackNodeSelectionAnimationDidEnd(trackNode)
    {
        if (this.uiDelegate && typeof this.uiDelegate.tracksPanelSelectionDidChange === "function")
            this.uiDelegate.tracksPanelSelectionDidChange(trackNode.index, trackNode.sectionIndex);
    }

    mouseMovedOverTrackNode(trackNode)
    {
        this._focusTrackNode(trackNode);
    }

    mouseExitedTrackNode(trackNode)
    {
        this._focusedTrackNode.element.blur();
        delete this._focusedTrackNode;
    }

    commitProperty(propertyName)
    {
        if (propertyName === "rightX")
            this.element.style.right = `${this._rightX}px`;
        else if (propertyName === "bottomY")
            this.element.style.bottom = `${this._bottomY}px`;
        else if (propertyName === "maxHeight") {
            this.element.style.maxHeight = `${this._maxHeight}px`;
            this._scrollableContainer.element.style.maxHeight = `${this._maxHeight}px`;
        } else
            super.commitProperty(propertyName);
    }

    handleEvent(event)
    {
        switch (event.type) {
        case "mousedown":
            this._handleMousedown(event);
            break;
        case "keydown":
            this._handleKeydown(event);
            break;
        case "transitionend":
            this.remove();
            break;
        }
    }

    // Private

    _mousedownTarget()
    {
        const mediaControls = this.parentOfType(MacOSFullscreenMediaControls);
        if (mediaControls)
            return mediaControls.element;
        return window;
    }

    _childrenFromDataSource()
    {
        const children = [];

        this._trackNodes = [];

        const dataSource = this.dataSource;
        if (!dataSource)
            return children;

        const numberOfSections = dataSource.tracksPanelNumberOfSections();
        if (numberOfSections === 0)
            return children;

        for (let sectionIndex = 0; sectionIndex < numberOfSections; ++sectionIndex) {
            let sectionNode = new LayoutNode(`<section></section>`);
            sectionNode.addChild(new LayoutNode(`<h3>${dataSource.tracksPanelTitleForSection(sectionIndex)}</h3>`));

            let tracksListNode = sectionNode.addChild(new LayoutNode(`<ul></ul>`));
            let numberOfTracks = dataSource.tracksPanelNumberOfTracksInSection(sectionIndex);
            for (let trackIndex = 0; trackIndex < numberOfTracks; ++trackIndex) {
                let trackTitle = dataSource.tracksPanelTitleForTrackInSection(trackIndex, sectionIndex);
                let trackSelected = dataSource.tracksPanelIsTrackInSectionSelected(trackIndex, sectionIndex);
                let trackNode = tracksListNode.addChild(new TrackNode(trackIndex, sectionIndex, trackTitle, trackSelected, this));
                this._trackNodes.push(trackNode);
            }
            children.push(sectionNode);
        }

        return children;
    }

    _handleMousedown(event)
    {
        if (this._isPointInTracksPanel(new DOMPoint(event.clientX, event.clientY)))
            return;

        this._dismiss();

        event.preventDefault();
        event.stopPropagation();
    }

    _isPointInTracksPanel(point)
    {
        let ancestor = this.element.parentNode;
        while (ancestor && !(ancestor instanceof ShadowRoot))
            ancestor = ancestor.parentNode;

        if (!ancestor)
            ancestor = document;

        return this.element.contains(ancestor.elementFromPoint(point.x, point.y));
    }

    _handleKeydown(event)
    {
        switch (event.key) {
        case "Home":
        case "PageUp":
            this._focusFirstTrackNode();
            break;
        case "End":
        case "PageDown":
            this._focusLastTrackNode();
            break;
        case "ArrowDown":
            if (event.altKey || event.metaKey)
                this._focusLastTrackNode();
            else
                this._focusNextTrackNode();
            break;
        case "ArrowUp":
            if (event.altKey || event.metaKey)
                this._focusFirstTrackNode();
            else
                this._focusPreviousTrackNode();
            break;
        case " ":
        case "Enter":
            if (this._focusedTrackNode)
                this._focusedTrackNode.activate();
            break;
        case "Escape":
            this._dismiss();
            break;
        default:
            return;
        }

        // Ensure that we don't let the browser react to a key code we handled,
        // for instance scrolling the page if we handled an arrow key.
        event.preventDefault();
    }

    _dismiss()
    {
        if (this.parent && typeof this.parent.hideTracksPanel === "function")
            this.parent.hideTracksPanel();
    }

    _focusTrackNode(trackNode)
    {
        if (!trackNode || trackNode === this._focusedTrackNode)
            return;

        trackNode.element.focus();
        this._focusedTrackNode = trackNode;
    }

    _focusPreviousTrackNode()
    {
        const previousIndex = this._focusedTrackNode ? this._trackNodes.indexOf(this._focusedTrackNode) - 1 : this._trackNodes.length - 1;
        this._focusTrackNode(this._trackNodes[previousIndex]);
    }

    _focusNextTrackNode()
    {
        this._focusTrackNode(this._trackNodes[this._trackNodes.indexOf(this._focusedTrackNode) + 1]);
    }

    _focusFirstTrackNode()
    {
        this._focusTrackNode(this._trackNodes[0]);
    }

    _focusLastTrackNode()
    {
        this._focusTrackNode(this._trackNodes[this._trackNodes.length - 1]);
    }

}

class TrackNode extends LayoutNode
{

    constructor(index, sectionIndex, title, selected, panel)
    {
        super(`<li tabindex="0">${title}</li>`);

        this.index = index;
        this.sectionIndex = sectionIndex;
        this._panel = panel;
        this._selected = selected;

        if (selected)
            this.element.classList.add("selected");

        this.element.addEventListener("mousemove", this);
        this.element.addEventListener("mouseleave", this);
        this.element.addEventListener("click", this);
    }

    // Public

    activate()
    {
        this.element.addEventListener("animationend", this);
        this.element.classList.add("animated");
    }

    // Protected

    handleEvent(event)
    {
        switch (event.type) {
        case "mousemove":
            this._panel.mouseMovedOverTrackNode(this);
            break;
        case "mouseleave":
            this._panel.mouseExitedTrackNode(this);
            break;
        case "click":
            this.activate();
            break;
        case "animationend":
            this._animationDidEnd();
            break;
        }
    }

    // Private

    _animationDidEnd()
    {
        this.element.removeEventListener("animationend", this);
        this._panel.trackNodeSelectionAnimationDidEnd(this);
    }

}
