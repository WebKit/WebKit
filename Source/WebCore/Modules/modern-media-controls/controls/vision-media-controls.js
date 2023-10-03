/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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

const MinSizeToShowLargeProminentControl = 292;

const MinWidthToShowTopCornerControlsBars = 262;
const MinWidthToShowBottomBar = 390;
const MinWidthToShowVolumeSlider = 390;
const MinWidthToShowSeekingControls = 520;
const MinWidthToShowOverflowButton = 520;
const MinWidthToShowLargeCentralControls = 640;

const MaxWidthForBottomControlsBarInFullscreen = 602;

const MinWidthToShowSeekingControlsForAudio = 120;
const MinWidthToShowTimeControlForAudio = 262;

class VisionMediaControls extends MediaControls
{

    constructor(options)
    {
        super(options);

        this._createControls();

        this.element.classList.add("inline", "vision");

        // Flags.

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
            this.playPauseButton.style = this._widthForSizeClassDetermination >= MinSizeToShowLargeProminentControl && this._heightForSizeClassDetermination >= MinSizeToShowLargeProminentControl ? Button.Styles.Center : Button.Styles.SmallCenter;
            this.children = [this.playPauseButton];
            return;
        }

        if (this._topLeftControlsBar && this._centerControlsBar && this.bottomControlsBar && this._topRightControlsBar)
            this._performLayout();
    }

    commitProperty(propertyName)
    {
        // We override the default behavior of the "visible" property, which usually means the node
        // will not be displayed if false, but we want to allow placards to be visible, even when
        // controls are supposed to be hidden.
        if (propertyName !== "visible")
            super.commitProperty(propertyName);
    }

    // Private

    get _widthForSizeClassDetermination()
    {
        return Math.ceil(this.width / this.scaleFactor);
    }

    get _heightForSizeClassDetermination()
    {
        return Math.ceil(this.height / this.scaleFactor);
    }

    _createControls()
    {
        this._backdrop = new LayoutNode('<div class="backdrop"></div>');

        // Buttons.

        this.skipBackButton = new SkipBackButton(this);
        this.skipBackButton.circular = true;
        this.playPauseButton.circular = true;
        this.skipForwardButton = new SkipForwardButton(this);
        this.skipForwardButton.circular = true;
        this.overflowButton.iconName = Icons.Ellipsis;
        this.overflowButton.circular = true;
        this.overflowButton.addExtraContextMenuOptions(this.tracksButton.contextMenuOptions);
        this.fullscreenButton.style = Button.Styles.Rounded;
        this.fullscreenButton.circular = true;

        // Time Control.

        this.timeControl.scrubber = new VisionSlider(this.layoutDelegate, "scrubber");

        // Controls Bars.

        this._topLeftControlsBar = new ControlsBar("simple-layout top-left");
        this._topLeftControlsBar.hasBackgroundTint = false;

        this._topRightControlsBar = new ControlsBar("simple-layout top-right");
        this._topRightControlsBar.hasBackgroundTint = false;

        this._centerControlsBar = new ControlsBar("simple-layout center");
        this._centerControlsBar.hasBackgroundTint = false;

        this._bottomBarLeftContainer = new ButtonsContainer({ cssClassName: "left" });
        this._bottomBarRightContainer = new ButtonsContainer({ cssClassName: "right" });

        this.volumeSlider = new VisionSlider(this, "volume");
        this.volumeSlider.width = 150;
        this.volumeSlider.recessed = true;

        this.volumeContainer = new VolumeContainer("volume-container");
    }

    _performLayout()
    {
        throw "Derived class must implement this function.";
    }

    _topRightControlsBarChildren()
    {
        throw "Derived class must implement this function.";
    }

    _performVolumeContainerLayout()
    {
        throw "Derived class must implement this function.";
    }

    _performTopLeftControlsBarLayout()
    {
        const buttons = [this.fullscreenButton];
        for (let button of buttons)
            button.visible = button.enabled;
        this._topLeftControlsBar.children = buttons;
    }

    _performTopRightControlsBarLayout()
    {
        this._performVolumeContainerLayout();

        this._topRightControlsBar.children = this._topRightControlsBarChildren();
    }

    _updateBottomControlsBarLabel()
    {
        this.bottomControlsBar.element.setAttribute("aria-label", this._shouldUseAudioLayout ? UIString("Audio Controls") : UIString("Video Controls"));
    }

    _bottomBarLeftContainerButtons()
    {
        if (this._widthForSizeClassDetermination < MinWidthToShowSeekingControlsForAudio && this._shouldUseAudioLayout)
            return [this.playPauseButton];
        return [this.skipBackButton, this.playPauseButton, this.skipForwardButton];
    }

    _bottomBarRightContainerButtons()
    {
        if (this._shouldUseAudioLayout || this._shouldUseSingleBarLayout)
            return [this.muteButton];
        return [this.overflowButton];
    }
}

class VisionInlineMediaControls extends VisionMediaControls
{
    constructor(options = {})
    {
        options.layoutTraits = new VisionLayoutTraits(LayoutTraits.Mode.Inline);
        
        super(options);
    }

    _createControls()
    {
        super._createControls();

        this.overflowButton.style = Button.Styles.Rounded;
        this.overflowButton.circular = true;

        this.timeControl.timeLabelsAttachment = TimeControl.TimeLabelsAttachment.Above;

        this.bottomControlsBar.hasBackgroundTint = false;

        this._bottomBarLeftContainer.leftMargin = 10;
        this._bottomBarLeftContainer.buttonMargin = 14;
        this._bottomBarLeftContainer.rightMargin = 15;

        this._bottomBarRightContainer.leftMargin = 15;
        this._bottomBarRightContainer.buttonMargin = 12;
        this._bottomBarRightContainer.rightMargin = 12;
    }

    _performLayout()
    {
        if (this._shouldUseSingleBarLayout || this._shouldUseAudioLayout) {
            this._performSingleBarLayout();
            this.children = [this.bottomControlsBar];
            return;
        }

        const children = [this._backdrop];

        if (this._widthForSizeClassDetermination >= MinWidthToShowTopCornerControlsBars) {
            this._performTopLeftControlsBarLayout();
            this._performTopRightControlsBarLayout();
            children.push(this._topLeftControlsBar);
            children.push(this._topRightControlsBar);
        }

        this._performCentralContainerLayout();
        children.push(this._centerControlsBar);
    
        if (this._widthForSizeClassDetermination >= MinWidthToShowBottomBar) {
            this._performBottomBarLayout();
            children.push(this.bottomControlsBar);
        }

        this.children = children;
    }

    _topRightControlsBarChildren()
    {
        const children = [];

        if (this._widthForSizeClassDetermination >= MinWidthToShowVolumeSlider)
            children.push(this.volumeContainer);
        else
            children.push(this.muteButton);

        if (this._widthForSizeClassDetermination >= MinWidthToShowOverflowButton)
            children.push(this.overflowButton);

        return children;
    }

    _centerContainerButtons()
    {
        if (this._widthForSizeClassDetermination < MinWidthToShowSeekingControls)
            return [this.playPauseButton];
        return [this.skipBackButton, this.playPauseButton, this.skipForwardButton];
    }
    
    _performCentralContainerLayout()
    {
        const scaleFactor = this._widthForSizeClassDetermination >= MinWidthToShowLargeCentralControls ? 1.5 : 1;
        
        const buttons = this._centerContainerButtons();
        for (let button of buttons) {
            button.style = Button.Styles.Bar;
            button.visible = button.enabled;
            button.scaleFactor = scaleFactor;
            button.circular = true;
        }
        this._centerControlsBar.children = buttons;
    }

    _performVolumeContainerLayout()
    {
        if (this._widthForSizeClassDetermination >= MinWidthToShowVolumeSlider) {
            this.muteButton.style = Button.Styles.Bar;
            this.muteButton.circular = false;
            this.volumeContainer.children = [this.volumeSlider, this.muteButton];
        } else {
            this.muteButton.style = Button.Styles.Rounded;
            this.muteButton.circular = true;
            this.volumeContainer.children = [];
        }
    }

    _performBottomBarLayout()
    {
        const margin = this.computedValueForStylePropertyInPx("--inline-controls-inside-margin");
        this.bottomControlsBar.width = this.width - (2 * margin);

        const control = this.statusLabel.enabled ? this.statusLabel : this.timeControl;
        control.width = this.bottomControlsBar.width;
        control.layout();

        this.bottomControlsBar.children = [control];
    }

    _performSingleBarLayout()
    {
        const children = [this._bottomBarLeftContainer];

        this.timeControl.scrubber.recessed = true;

        this.bottomControlsBar.width = this.width;

        this.skipBackButton.style = Button.Styles.Bar;
        this.skipForwardButton.style = Button.Styles.Bar;
        this.playPauseButton.style = Button.Styles.Bar;
        this.muteButton.style = Button.Styles.Bar;
        this.muteButton.circular = true;
        this.overflowButton.style = Button.Styles.Bar;

        this.timeControl.timeLabelsAttachment = TimeControl.TimeLabelsAttachment.Side;

        this.bottomControlsBar.hasBackgroundTint = true;
        
        this._bottomBarLeftContainer.children = this._bottomBarLeftContainerButtons();
        this._bottomBarLeftContainer.layout();
        this._bottomBarRightContainer.children = this._bottomBarRightContainerButtons();
        this._bottomBarRightContainer.layout();
        
        const widthLeftOfTimeControl = this._bottomBarLeftContainer.children.length > 0 ? this._bottomBarLeftContainer.width : this._bottomBarLeftContainer.leftMargin;
        const widthRightOfTimeControl = this._bottomBarRightContainer.children.length > 0 ? this._bottomBarRightContainer.width : this._bottomBarRightContainer.rightMargin;
        const centerControl = this.statusLabel.enabled ? this.statusLabel : this.timeControl;

        if (this._widthForSizeClassDetermination >= MinWidthToShowTimeControlForAudio && centerControl == this.timeControl) {
            centerControl.x = widthLeftOfTimeControl;
            centerControl.width = this.bottomControlsBar.width - widthLeftOfTimeControl - widthRightOfTimeControl;
            centerControl.layout();
            children.push(centerControl);
        }

        this._bottomBarRightContainer.x = this.bottomControlsBar.width - this._bottomBarRightContainer.width;
        children.push(this._bottomBarRightContainer);
        
        this.bottomControlsBar.children = children;
    }
}

class VisionFullscreenMediaControls extends VisionMediaControls
{
    constructor(options = {})
    {
        options.layoutTraits = new VisionLayoutTraits(LayoutTraits.Mode.Fullscreen);

        super(options);

        this.element.classList.add("fullscreen");
    }

    _createControls()
    {
        super._createControls();

        this.timeControl.scrubber.recessed = true;

        this._topRightControlsBar.hasBackgroundTint = false;

        this.muteButton.style = Button.Styles.Bar;
        this.overflowButton.style = Button.Styles.Bar;

        this.skipBackButton.scaleFactor = 1.6;
        this.playPauseButton.scaleFactor = 2;
        this.skipForwardButton.scaleFactor = 1.6;

        this._bottomBarLeftContainer.leftMargin = 18;
        this._bottomBarLeftContainer.buttonMargin = 24;
        this._bottomBarLeftContainer.rightMargin = 25;

        this._bottomBarRightContainer.leftMargin = 25;
        this._bottomBarRightContainer.buttonMargin = 24;
        this._bottomBarRightContainer.rightMargin = 20;
    }

    _performLayout()
    {
        this._performTopLeftControlsBarLayout();
        this._performTopRightControlsBarLayout();
        this._performBottomBarLayout();
        
        this.children = [this._backdrop, this._topLeftControlsBar, this._topRightControlsBar, this.bottomControlsBar];
    }

    _performVolumeContainerLayout()
    {
        this.muteButton.style = Button.Styles.Bar;
        this.muteButton.circular = false;
        this.volumeContainer.children = [this.volumeSlider, this.muteButton];
    }

    _topRightControlsBarChildren()
    {
        return [this.volumeContainer];
    }

    _performBottomBarLayout()
    {
        // Compute the visible size for the controls bar.
        const inlineInsideMargin = this.computedValueForStylePropertyInPx("--inline-controls-inside-margin");
        this.bottomControlsBar.width = Math.min(this._shouldUseAudioLayout ? this.width : (this.width - 2 * inlineInsideMargin), MaxWidthForBottomControlsBarInFullscreen);

        this.playPauseButton.style = Button.Styles.Bar;
        
        this._bottomBarLeftContainer.children = this._bottomBarLeftContainerButtons();
        this._bottomBarLeftContainer.layout();
        this._bottomBarRightContainer.children = this._bottomBarRightContainerButtons();
        this._bottomBarRightContainer.layout();
        
        const widthLeftOfTimeControl = this._bottomBarLeftContainer.children.length > 0 ? this._bottomBarLeftContainer.width : this._bottomBarLeftContainer.leftMargin;
        const widthRightOfTimeControl = this._bottomBarRightContainer.children.length > 0 ? this._bottomBarRightContainer.width : this._bottomBarRightContainer.rightMargin;
        const centerControl = this.statusLabel.enabled ? this.statusLabel : this.timeControl;
        centerControl.x = widthLeftOfTimeControl;
        centerControl.width = this.bottomControlsBar.width - widthLeftOfTimeControl - widthRightOfTimeControl;
        centerControl.layout();
        
        this._bottomBarRightContainer.x = this.bottomControlsBar.width - this._bottomBarRightContainer.width;
        
        // Ensure we position the bottom controls bar at the bottom of the frame, accounting for
        // the inside margin, unless this would yield a position outside of the frame.
        this.bottomControlsBar.x = (this.width - this.bottomControlsBar.width) / 2;
        
        this.bottomControlsBar.children = [this._bottomBarLeftContainer, centerControl, this._bottomBarRightContainer];
    }
}
