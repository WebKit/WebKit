/*
 * Copyright (C) 2017 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

class InlineMediaControls extends MediaControls
{

    constructor(options)
    {
        super(options);

        this.element.classList.add("inline");

        this.skipBackButton = new SkipBackButton(this);
        this.skipForwardButton = new SkipForwardButton(this);

        this.topLeftControlsBar = new ControlsBar("top-left");
        this._topLeftControlsBarContainer = this.topLeftControlsBar.addChild(new ButtonsContainer);

        this.topRightControlsBar = new ControlsBar("top-right");
        this._topRightControlsBarContainer = this.topRightControlsBar.addChild(new ButtonsContainer);

        this.leftContainer = new ButtonsContainer({ cssClassName: "left" });
        this.rightContainer = new ButtonsContainer({ cssClassName: "right" });

        this.centerControlsBar = new ControlsBar("center");
        this._centerControlsBarContainer = this.centerControlsBar.addChild(new ButtonsContainer);

        this._shouldUseAudioLayout = false;
        this._shouldUseSingleBarLayout = false;
        this.showsStartButton = false;
        this._updateBottomControlsBarLabel();
    }

    // Public

    set shouldUseAudioLayout(flag)
    {
        if (this._shouldUseAudioLayout === flag)
            return;

        this._shouldUseAudioLayout = flag;
        this.element.classList.toggle("audio", flag);
        this.needsLayout = true;
        this._updateBottomControlsBarLabel();
    }

    set shouldUseSingleBarLayout(flag)
    {
        if (this._shouldUseSingleBarLayout === flag)
            return;

        this._shouldUseSingleBarLayout = flag;
        this.needsLayout = true;
    }

    get showsStartButton()
    {
        return !!this._showsStartButton;
    }

    set showsStartButton(flag)
    {
        if (this._showsStartButton === flag)
            return;

        this._showsStartButton = flag;
        this.element.classList.toggle("shows-start-button", flag);
        this.layout();
    }

    // Protected

    layout()
    {
        super.layout();

        const children = [];

        if (this.placard) {
            children.push(this.placard);
            if (this.placardPreventsControlsBarDisplay()) {
                this.children = children;
                return;
            }
        }

        if (!this.visible) {
            this.children = children;
            return;
        }

        // The controls might be too small to allow showing anything at all.
        if (!this._shouldUseAudioLayout && (this.width < MinimumSizeToShowAnyControl || this.height < MinimumSizeToShowAnyControl)) {
            this.children = children;
            return;
        }

        // If we should show the start button, then only show that button.
        if (this._showsStartButton) {
            this.playPauseButton.style = this.width <= MaximumSizeToShowSmallProminentControl || this.height <= MaximumSizeToShowSmallProminentControl ? Button.Styles.SmallCenter : Button.Styles.Center;
            this.children = [this.playPauseButton];
            return;
        }

        if (!this.bottomControlsBar)
            return;

        // Update the top left controls bar.
        this._topLeftControlsBarContainer.children = this.topLeftContainerButtons();
        this._topLeftControlsBarContainer.layout();
        this.topLeftControlsBar.width = this._topLeftControlsBarContainer.width;
        this.topLeftControlsBar.visible = this._topLeftControlsBarContainer.children.some(button => button.visible);

        this._centerControlsBarContainer.children = this.centerContainerButtons();
        this._centerControlsBarContainer.layout();
        this.centerControlsBar.width = this._centerControlsBarContainer.width;
        this.centerControlsBar.visible = this._centerControlsBarContainer.children.some(button => button.visible);

        // Compute the visible size for the controls bar.
        if (!this._inlineInsideMargin)
            this._inlineInsideMargin = this.computedValueForStylePropertyInPx("--inline-controls-inside-margin");
        this.bottomControlsBar.width = this._shouldUseAudioLayout ? this.width : (this.width - 2 * this._inlineInsideMargin);

        // Compute the absolute minimum width to display the center control (status label or time control).
        const centerControl = this.statusLabel.enabled ? this.statusLabel : this.timeControl;
        let minimumCenterControlWidth = centerControl.minimumWidth;

        // Worst case scenario is that we can't fit the center control with the required margins. In this case,
        // we need to make the play/pause button display as a corner button.
        const minimumControlsBarWidthForCenterControl = minimumCenterControlWidth + this.leftContainer.leftMargin + this.rightContainer.rightMargin;
        if (this.bottomControlsBar.width < minimumControlsBarWidthForCenterControl) {
            this.playPauseButton.style = Button.Styles.Corner;
            if (!this._shouldUseSingleBarLayout && this.height >= 82) {
                children.push(this.topLeftControlsBar);
                this._addTopRightBarWithMuteButtonToChildren(children);
            }
            this.children = children.concat(this.playPauseButton);
            return;
        }

        // Now allow the minimum center element to display with fewer constraints.
        minimumCenterControlWidth = centerControl.idealMinimumWidth;

        // Iterate through controls to see if we need to drop any of them. Reset all default states before we proceed.
        this.bottomControlsBar.visible = true;
        this.playPauseButton.style = Button.Styles.Bar;
        this.leftContainer.children = this.leftContainerButtons();
        this.rightContainer.children = this.rightContainerButtons();
        this.rightContainer.children.concat(this.leftContainer.children).forEach(button => delete button.dropped);
        this.muteButton.style = this.preferredMuteButtonStyle;
        this.overflowButton.clearExtraContextMenuOptions();

        for (let button of this.droppableButtons()) {
            // If the button is not enabled, we can skip it.
            if (!button.enabled)
                continue;

            // Ensure button containers are laid out with latest constraints.
            this.leftContainer.layout();
            this.rightContainer.layout();

            // Nothing left to do if the combined width of both containers and the time control is shorter than the available width.
            if (this.leftContainer.width + minimumCenterControlWidth + this.rightContainer.width < this.bottomControlsBar.width)
                break;

            // This button must now be dropped.
            button.dropped = true;

            if (button !== this.overflowButton)
                this.overflowButton.addExtraContextMenuOptions(button.contextMenuOptions);
        }

        let collapsableButtons = this.collapsableButtons();
        let shownRightContainerButtons = this.rightContainer.children.filter(button => button.enabled && !button.dropped);
        let maximumRightContainerButtonCount = this.maximumRightContainerButtonCountOverride ?? 2; // Allow AirPlay and overflow if all buttons are shown.
        for (let i = shownRightContainerButtons.length - 1; i >= 0 && shownRightContainerButtons.length > maximumRightContainerButtonCount; --i) {
            let button = shownRightContainerButtons[i];
            if (!collapsableButtons.has(button))
                continue;

            button.dropped = true;
            this.overflowButton.addExtraContextMenuOptions(button.contextMenuOptions);
        }

        // Update layouts once more.
        this.leftContainer.layout();
        this.rightContainer.layout();

        const widthLeftOfTimeControl = this.leftContainer.children.length > 0 ? this.leftContainer.width : this.leftContainer.leftMargin;
        const widthRightOfTimeControl = this.rightContainer.children.length > 0 ? this.rightContainer.width : this.rightContainer.rightMargin;
        centerControl.x = widthLeftOfTimeControl;
        centerControl.width = this.bottomControlsBar.width - widthLeftOfTimeControl - widthRightOfTimeControl;
        centerControl.layout();

        // Add visible children.
        const controlsBarChildren = [];
        if (this.leftContainer.children.length)
            controlsBarChildren.push(this.leftContainer);
        controlsBarChildren.push(centerControl);
        if (this.rightContainer.children.length) {
            controlsBarChildren.push(this.rightContainer);
            this.rightContainer.x = this.bottomControlsBar.width - this.rightContainer.width;
        }

        // Ensure we position the bottom controls bar at the bottom of the frame, accounting for
        // the inside margin, unless this would yield a position outside of the frame.
        if (!this._inlineBottomControlsBarHeight)
            this._inlineBottomControlsBarHeight = this.computedValueForStylePropertyInPx("--inline-controls-bar-height");
        this.bottomControlsBar.y = Math.max(0, this.height - this._inlineBottomControlsBarHeight - this._inlineInsideMargin);

        this.bottomControlsBar.children = controlsBarChildren;
        if (!this._shouldUseAudioLayout && !this._shouldUseSingleBarLayout)
            children.push(this.topLeftControlsBar);
        if (!this._shouldUseAudioLayout && !this._shouldUseSingleBarLayout && this._centerControlsBarContainer.children.length)
            children.push(this.centerControlsBar);
        children.push(this.bottomControlsBar);
        if (this.muteButton.style === Button.Styles.Corner || (this.muteButton.dropped && !this._shouldUseAudioLayout && !this._shouldUseSingleBarLayout))
            this._addTopRightBarWithMuteButtonToChildren(children);
        this.children = children;
    }

    commitProperty(propertyName)
    {
        // We override the default behavior of the "visible" property, which usually means the node
        // will not be displayed if false, but we want to allow placards to be visible, even when
        // controls are supposed to be hidden.
        if (propertyName !== "visible")
            super.commitProperty(propertyName);
    }

    get preferredMuteButtonStyle()
    {
        return (this._shouldUseAudioLayout || this._shouldUseSingleBarLayout) ? Button.Styles.Bar : Button.Styles.Corner;
    }

    topLeftContainerButtons()
    {
        if (this._shouldUseSingleBarLayout)
            return [];
        if (this.usesLTRUserInterfaceLayoutDirection)
            return [this.fullscreenButton, this.pipButton];
        return [this.pipButton, this.fullscreenButton];
    }

    leftContainerButtons()
    {
        return [this.skipBackButton, this.playPauseButton, this.skipForwardButton];
    }

    centerContainerButtons() {
        return [];
    }

    rightContainerButtons()
    {
        const buttons = [];
        if (this._shouldUseAudioLayout)
            buttons.push(this.muteButton, this.airplayButton, this.tracksButton);
        else if (this._shouldUseSingleBarLayout)
            buttons.push(this.muteButton, this.airplayButton, this.pipButton, this.tracksButton, this.fullscreenButton);
        else {
            if (this.preferredMuteButtonStyle === Button.Styles.Bar)
                buttons.push(this.muteButton);
            buttons.push(this.airplayButton, this.tracksButton);
        }
        buttons.push(this.overflowButton);
        return buttons.filter(button => button !== null);
    }

    droppableButtons()
    {
        let buttons = this.collapsableButtons();
        buttons.add(this.skipForwardButton);
        buttons.add(this.skipBackButton);
        if (this._shouldUseSingleBarLayout || this.preferredMuteButtonStyle === Button.Styles.Bar)
            buttons.add(this.muteButton);
        buttons.add(this.airplayButton);
        if (this._shouldUseSingleBarLayout)
            buttons.add(this.fullscreenButton);
        buttons.add(this.overflowButton);
        buttons.delete(null);
        return buttons;
    }

    collapsableButtons()
    {
        let buttons = new Set([
            this.tracksButton,
        ]);
        if (this._shouldUseSingleBarLayout)
            buttons.add(this.pipButton);
        return buttons;
    }

    // Private

    _updateBottomControlsBarLabel()
    {
        this.bottomControlsBar.element.setAttribute("aria-label", this._shouldUseAudioLayout ? UIString("Audio Controls") : UIString("Video Controls"));
    }

    _addTopRightBarWithMuteButtonToChildren(children)
    {
        if (!this.muteButton.enabled)
            return;

        delete this.muteButton.dropped;
        this.muteButton.style = Button.Styles.Bar;
        this._topRightControlsBarContainer.children = [this.muteButton];
        this._topRightControlsBarContainer.layout();
        this.topRightControlsBar.width = this._topRightControlsBarContainer.width;
        children.push(this.topRightControlsBar);
    }

}
