/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

const MinimumHeightToShowVolumeSlider = 136;

class MacOSInlineMediaControls extends InlineMediaControls
{

    constructor(options = {})
    {
        if (options.mode === undefined)
            options.layoutTraits = new MacOSLayoutTraits(LayoutTraits.Mode.Inline);
        else
            options.layoutTraits = new MacOSLayoutTraits(options.mode);

        super(options);

        this.element.classList.add("mac");

        this._backgroundClickDelegateNotifier = new BackgroundClickDelegateNotifier(this);

        this.volumeSlider = new Slider(this, "volume");
        this.volumeSlider.width = 60;

        this._volumeSliderContainer = new LayoutNode(`<div class="volume-slider-container"></div>`);

        if (this.layoutTraits.mode == LayoutTraits.Mode.NarrowViewer) {
            this.element.classList.add("narrowviewer");
            this.fullscreenButton.isFullscreen = true;
        }
    }

    _setupVolumeSliderConfiguration()
    {
        if (this._volumeSliderSetupComplete) {
            return;
        }

        const hasValidHost = this.mediaControlsHost && typeof this.mediaControlsHost.isMediaControlsMacInlineSizeSpecsEnabled !== 'undefined';

        if (!hasValidHost) {
            return;
        }
        
        if (!this.mediaControlsHost.isMediaControlsMacInlineSizeSpecsEnabled || this.element.classList.contains('audio')) {
            this.timeControl.scrubber.knobStyle = Slider.KnobStyle.Bar;
            this._volumeSliderContainer.children = [new BackgroundTint, this.volumeSlider];
            
            // Wire up events to display the volume slider.
            this.muteButton.element.addEventListener("mouseenter", this);
            this.muteButton.element.addEventListener("mouseleave", this);
            this._volumeSliderContainer.element.addEventListener("mouseleave", this);
        } else {
            this.volumeSlider.width = 135;
            this._volumeSliderContainer.children = [new BackgroundTint, this.volumeSlider, this.muteButton];
        }
        
        this._volumeSliderSetupComplete = true;
    }

    findHostFromParent()
    {
        const secondParent = this.element?.parentElement?.parentElement || this.element?.parentNode?.parentNode;

        if (secondParent instanceof ShadowRoot && secondParent.host?.controlsHost) {
            if (typeof secondParent.host.controlsHost.isMediaControlsMacInlineSizeSpecsEnabled !== 'undefined') {
                return secondParent.host.controlsHost;
            }
        }
        
        return null;
    }

    get mediaControlsHost()
    {
        if (this._host) {
            return this._host;
        }

        this._host = this.findHostFromParent();
        return this._host;
    }

    // Protected

    layout()
    {
        super.layout();

        if (!this._volumeSliderContainer)
            return;

        if (this.mediaControlsHost?.isMediaControlsMacInlineSizeSpecsEnabled && !this.element.classList.contains('audio')) {
            if (this.rightContainer?.element) {
                this.rightContainer.element.style.removeProperty('left');
                this.rightContainer._dirtyProperties.delete('x');
            }
        }

        if (!this._volumeSliderSetupComplete) {
            this._setupVolumeSliderConfiguration();
        }

        if (!this._inlineInsideMargin)
            this._inlineInsideMargin = this.computedValueForStylePropertyInPx("--inline-controls-inside-margin");
        if (!this._inlineBottomControlsBarHeight)
            this._inlineBottomControlsBarHeight = this.computedValueForStylePropertyInPx("--inline-controls-bar-height");

        if (this.mediaControlsHost && (!this.mediaControlsHost?.isMediaControlsMacInlineSizeSpecsEnabled || this.element.classList.contains('audio'))) {
            this._volumeSliderContainer.x = this.rightContainer.x + this.muteButton.x;
            this._volumeSliderContainer.y = this.bottomControlsBar.y - this._inlineBottomControlsBarHeight - this._inlineInsideMargin;
        } else {
            const hasMinimumHeight = this.height >= MinimumHeightToShowVolumeSlider;
            const hasMinimumWidth = this.width >= this.volumeSlider.width + 130;
            const shouldShowVolumeContainer = hasMinimumHeight && hasMinimumWidth && !this.showsStartButton && this.visible && this.playPauseButton && this.playPauseButton.enabled;

            // Force mute button visibility based on width even if container logic doesn't run
            if (this.width < 60 && this.muteButton) {
                this.muteButton.visible = false;
            }

            const children = this.children ? [...this.children] : [];
            if (shouldShowVolumeContainer && !children.includes(this._volumeSliderContainer)) {
                this._volumeSliderContainer.children = [new BackgroundTint, this.volumeSlider, this.muteButton];
                children.push(this._volumeSliderContainer);
                this.children = children;
                // Clear any old x-y values which may override CSS
                this._volumeSliderContainer.x = 0;
                this._volumeSliderContainer.y = 0;
                if (this._volumeSliderContainer.element) {
                    this._volumeSliderContainer.element.style.removeProperty('left');
                    this._volumeSliderContainer.element.style.removeProperty('top');
                }

                if (this.muteButton)
                    this.muteButton.visible = true;
            } else if (!shouldShowVolumeContainer && children.includes(this._volumeSliderContainer)) {
                const volumeContainerIndex = children.indexOf(this._volumeSliderContainer);
                children.splice(volumeContainerIndex, 1);
                this.children = children;

                if (this.muteButton)
                    this.muteButton.visible = false;
            }
        }
    }

    get preferredMuteButtonStyle()
    {
        return (this.height >= MinimumHeightToShowVolumeSlider) ? Button.Styles.Bar : super.preferredMuteButtonStyle;
    }

    handleEvent(event)
    {
        if (!this._volumeSliderSetupComplete) {
            this._setupVolumeSliderConfiguration();
        }

        if (this.mediaControlsHost && (!this.mediaControlsHost?.isMediaControlsMacInlineSizeSpecsEnabled || this.element.classList.contains('audio'))) {
            if (event.type === "mouseenter" && event.currentTarget === this.muteButton.element) {
                if (this.muteButton.parent === this.rightContainer) {
                    this.addChild(this._volumeSliderContainer);
                }
            } else if (event.type === "mouseleave" && (event.currentTarget === this.muteButton.element || event.currentTarget === this._volumeSliderContainer.element)) {
                if (!this._volumeSliderContainer.element.contains(event.relatedTarget)) {
                    this._volumeSliderContainer.remove();
                }
            }
        }
    }

    rightContainerButtons()
    {
        if (!this._volumeSliderSetupComplete) {
            this._setupVolumeSliderConfiguration();
        }

        if (this.mediaControlsHost && this.mediaControlsHost?.isMediaControlsMacInlineSizeSpecsEnabled && !this.element.classList.contains('audio')) {
            const buttons = super.rightContainerButtons();
            if (buttons && this.muteButton) {
                const muteButtonIndex = buttons.indexOf(this.muteButton);
                if (muteButtonIndex !== -1) {
                    buttons.splice(muteButtonIndex, 1);
                }
            }
            return buttons;
        }
        return super.rightContainerButtons ? super.rightContainerButtons() : [];
    }
}
