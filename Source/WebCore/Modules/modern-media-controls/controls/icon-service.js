/*
 * Copyright (C) 2016 Apple Inc. All Rights Reserved.
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

const Icons = {
    Airplay         : { name: "Airplay", type: "svg", label: UIString("AirPlay") },
    AirplayPlacard  : { name: "airplay-placard", type: "png", label: UIString("AirPlay") },
    Close           : { name: "X", type: "svg", label: UIString("Close") },
    Ellipsis        : { name: "Ellipsis", type: "svg", label: UIString("More\u2026") },
    EnterFullscreen : { name: "EnterFullscreen", type: "svg", label: UIString("Enter Full Screen") },
    EnterPiP        : { name: "PipIn", type: "svg", label: UIString("Enter Picture in Picture") },
    ExitFullscreen  : { name: "ExitFullscreen", type: "svg", label: UIString("Exit Full Screen") },
    Forward         : { name: "Forward", type: "svg", label: UIString("Forward") },
    InvalidCircle   : { name: "InvalidCircle", type: "pdf", label: UIString("Invalid") },
    InvalidPlacard  : { name: "invalid-placard", type: "png", label: UIString("Invalid") },
    Overflow        : { name: "Overflow", type: "svg", label: UIString("More\u2026") },
    Pause           : { name: "Pause", type: "svg", label: UIString("Pause") },
    PiPPlacard      : { name: "pip-placard", type: "png", label: UIString("Picture in Picture") },
    Play            : { name: "Play", type: "svg", label: UIString("Play") },
    PlayCircle      : { name: "PlayCircle", type: "pdf", label: UIString("Play") },
    Rewind          : { name: "Rewind", type: "svg", label: UIString("Rewind") },
    SkipBack10      : { name: "SkipBack10", type: "svg", label: UIString("Skip Back 10 Seconds") },
    SkipBack15      : { name: "SkipBack15", type: "svg", label: UIString("Skip Back 15 Seconds") },
    SkipForward10   : { name: "SkipForward10", type: "svg", label: UIString("Skip Forward 10 Seconds") },
    SkipForward15   : { name: "SkipForward15", type: "svg", label: UIString("Skip Forward 15 Seconds") },
    SpinnerSprite   : { name: "SpinnerSprite", type: "png", label: UIString("Loading\u2026") },
    Tracks          : { name: "MediaSelector", type: "svg", label: UIString("Media Selection") },
    Volume0         : { name: "Volume0", type: "svg", label: UIString("Mute") },
    Volume0RTL      : { name: "Volume0-RTL", type: "svg", label: UIString("Mute") },
    Volume1         : { name: "Volume1", type: "svg", label: UIString("Mute") },
    Volume1RTL      : { name: "Volume1-RTL", type: "svg", label: UIString("Mute") },
    Volume2         : { name: "Volume2", type: "svg", label: UIString("Mute") },
    Volume2RTL      : { name: "Volume2-RTL", type: "svg", label: UIString("Mute") },
    Volume3         : { name: "Volume3", type: "svg", label: UIString("Mute") },
    Volume3RTL      : { name: "Volume3-RTL", type: "svg", label: UIString("Mute") },
    VolumeMuted     : { name: "VolumeMuted", type: "svg", label: UIString("Unmute") },
    VolumeMutedRTL  : { name: "VolumeMuted-RTL", type: "svg", label: UIString("Unmute") },
};

const MimeTypes = {
    "pdf": "application/pdf",
    "png": "image/png",
    "svg": "image/svg+xml"
};

const IconsWithFullscreenVariants = [Icons.Airplay, Icons.Tracks, Icons.EnterPiP];

const iconService = new class IconService {

    constructor()
    {
        this.images = {};
    }

    // Public
    get shadowRoot()
    {
        return this.shadowRootWeakRef ? this.shadowRootWeakRef.deref() : null;
    }

    set shadowRoot(shadowRoot)
    {
        this.shadowRootWeakRef = new WeakRef(shadowRoot);
    }

    imageForIconAndLayoutTraits(icon, layoutTraits)
    {
        const [fileName, resourceDirectory] = this._fileNameAndResourceDirectoryForIconAndLayoutTraits(icon, layoutTraits);
        const path = `${resourceDirectory}/${fileName}.${icon.type}`;

        let image = this.images[path];
        if (image)
            return image;

        image = this.images[path] = new Image;

        // Prevent this image from being shown if it's ever attached to the DOM.
        image.style.display = "none";

        // Must attach the `<img>` to the UA shadow root before setting `src` so that `isInUserAgentShadowTree` is correct.
        this.shadowRoot?.appendChild(image);

        if (this.mediaControlsHost)
            image.src = `data:${MimeTypes[icon.type]};base64,${this.mediaControlsHost.base64StringForIconNameAndType(fileName, icon.type)}`;
        else
            image.src = `${this.directoryPath}/${path}`;

        // Remove the `<img>` from the shadow root once the `src` has been set as `isInUserAgentShadowTree` has already been checked by this point.
        image.remove();

        return image;
    }

    // Private

    _fileNameAndResourceDirectoryForIconAndLayoutTraits(icon, layoutTraits)
    {
        let resourceDirectory = layoutTraits.resourceDirectory();

        let iconName = icon.name;
        if (layoutTraits.supportsIconWithFullscreenVariant() && IconsWithFullscreenVariants.includes(icon))
            iconName += "-fullscreen";

        let fileName = iconName;
        if (icon.type === "png")
            fileName = `${iconName}@${window.devicePixelRatio}x`;

        return [fileName, resourceDirectory];
    }

};
