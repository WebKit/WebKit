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

const InsideMargin = 6; // Minimum margin to guarantee around all controls, this constant needs to stay in sync with the --inline-controls-inside-margin CSS variable.
const BottomControlsBarHeight = 31; // This constant needs to stay in sync with the --inline-controls-bar-height CSS variable.

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

        // Ensure the tracks panel is a child if it were presented.
        if (this.tracksPanel.presented)
            children.push(this.tracksPanel);

        // Update the top left controls bar.
        this._topLeftControlsBarContainer.children = this._topLeftContainerButtons();
        this._topLeftControlsBarContainer.layout();
        this.topLeftControlsBar.width = this._topLeftControlsBarContainer.width;
        this.topLeftControlsBar.visible = this._topLeftControlsBarContainer.children.some(button => button.visible);

        // Compute the visible size for the controls bar.
        this.bottomControlsBar.width = this._shouldUseAudioLayout ? this.width : (this.width - 2 * InsideMargin);

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
        this.leftContainer.children = this._leftContainerButtons();
        this.rightContainer.children = this._rightContainerButtons();
        this.rightContainer.children.concat(this.leftContainer.children).forEach(button => delete button.dropped);
        this.muteButton.style = this.preferredMuteButtonStyle;
        this.muteButton.usesRTLIconVariant = !this.usesLTRUserInterfaceLayoutDirection;

        for (let button of this._droppableButtons()) {
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
        this.bottomControlsBar.y = Math.max(0, this.height - BottomControlsBarHeight - InsideMargin);

        this.bottomControlsBar.children = controlsBarChildren;
        if (!this._shouldUseAudioLayout && !this._shouldUseSingleBarLayout)
            children.push(this.topLeftControlsBar);
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

    // Private

    _updateBottomControlsBarLabel()
    {
        this.bottomControlsBar.element.setAttribute("aria-label", this._shouldUseAudioLayout ? UIString("Audio Controls") : UIString("Video Controls"));
    }
    
    _topLeftContainerButtons()
    {
        if (this._shouldUseSingleBarLayout)
            return [];
        if (this.usesLTRUserInterfaceLayoutDirection)
            return [this.fullscreenButton, this.pipButton];
        return [this.pipButton, this.fullscreenButton];
    }

    _leftContainerButtons()
    {
        return [this.skipBackButton, this.playPauseButton, this.skipForwardButton];
    }

    _rightContainerButtons()
    {
        if (this._shouldUseAudioLayout)
            return [this.muteButton, this.airplayButton];

        if (this._shouldUseSingleBarLayout)
            return [this.muteButton, this.airplayButton, this.pipButton, this.tracksButton, this.fullscreenButton];

        const buttons = [];
        if (this.preferredMuteButtonStyle === Button.Styles.Bar)
            buttons.push(this.muteButton);
        buttons.push(this.airplayButton, this.tracksButton);
        return buttons;
    }

    _droppableButtons()
    {
        if (this._shouldUseSingleBarLayout)
            return [this.skipForwardButton, this.skipBackButton, this.airplayButton, this.tracksButton, this.pipButton, this.fullscreenButton, this.muteButton];

        const buttons = [this.skipForwardButton, this.skipBackButton, this.airplayButton, this.tracksButton];
        if (this.preferredMuteButtonStyle === Button.Styles.Bar)
            buttons.push(this.muteButton);
        return buttons;
    }

    _addTopRightBarWithMuteButtonToChildren(children)
    {
        if (!this.muteButton.enabled)
            return;

        delete this.muteButton.dropped;
        this.muteButton.style = Button.Styles.Bar;
        this.muteButton.usesRTLIconVariant = !this.usesLTRUserInterfaceLayoutDirection;
        this._topRightControlsBarContainer.children = [this.muteButton];
        this._topRightControlsBarContainer.layout();
        this.topRightControlsBar.width = this._topRightControlsBarContainer.width;
        children.push(this.topRightControlsBar);
    }

}
