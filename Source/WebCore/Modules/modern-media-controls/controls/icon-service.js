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
    Airplay         : "airplay",
    AirplayPlacard  : "airplay-placard",
    EnterFullscreen : "enter-fullscreen",
    EnterPiP        : "pip-in",
    ExitFullscreen  : "exit-fullscreen",
    Forward         : "forward",
    InvalidPlacard  : "invalid-placard",
    Pause           : "pause",
    PiPPlacard      : "pip-placard",
    Play            : "play",
    Rewind          : "rewind",
    ScaleToFill     : "scale-to-fill",
    ScaleToFit      : "scale-to-fit",
    SkipBack        : "interval-skip-back",
    Start           : "start",
    Tracks          : "media-selection",
    Volume          : "volume",
    VolumeDown      : "volume-down",
    VolumeMuted     : "volume-mute",
    VolumeUp        : "volume-up"
};

const IconsWithFullscreenVariants = [Icons.Airplay, Icons.Tracks, Icons.Pause, Icons.EnterPiP, Icons.Play, Icons.VolumeDown, Icons.VolumeUp];
const IconsWithCompactVariants = [Icons.Play, Icons.Pause, Icons.SkipBack, Icons.Volume, Icons.VolumeMuted, Icons.EnterFullscreen];

const iconService = new class IconService {

    constructor()
    {
        this.images = {};
    }

    // Public

    imageForIconNameAndLayoutTraits(iconName, layoutTraits)
    {
        const [fileName, platform] = this._fileNameAndPlatformForIconNameAndLayoutTraits(iconName, layoutTraits);
        const path = `${platform}/${fileName}.png`;

        let image = this.images[path];
        if (image)
            return image;

        image = this.images[path] = new Image;

        if (this.mediaControlsHost)
            image.src = "data:image/png;base64," + this.mediaControlsHost.base64StringForIconAndPlatform(fileName, platform);
        else
            image.src = `${this.directoryPath}/${path}`;

        return image;
    }

    // Private

    _fileNameAndPlatformForIconNameAndLayoutTraits(iconName, layoutTraits)
    {
        let platform;
        if (layoutTraits & LayoutTraits.macOS)
            platform = "macOS";
        else if (layoutTraits & LayoutTraits.iOS)
            platform = "iOS";
        else
            throw "Could not identify icon's platform from layout traits.";

        if (layoutTraits & LayoutTraits.Fullscreen && IconsWithFullscreenVariants.includes(iconName))
            iconName += "-fullscreen";
        else if (layoutTraits & LayoutTraits.Compact && IconsWithCompactVariants.includes(iconName))
            iconName += "-compact";

        const fileName = `${iconName}@${window.devicePixelRatio}x`;

        return [fileName, platform];
    }

};
