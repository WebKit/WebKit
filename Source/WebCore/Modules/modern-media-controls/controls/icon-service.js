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
    Airplay         : { name: "airplay", label: UIString("AirPlay") },
    AirplayPlacard  : { name: "airplay-placard", label: UIString("AirPlay") },
    EnterFullscreen : { name: "enter-fullscreen", label: UIString("Enter Full Screen") },
    EnterPiP        : { name: "pip-in", label: UIString("Enter Picture in Picture") },
    ExitFullscreen  : { name: "exit-fullscreen", label: UIString("Exit Full Screen") },
    Forward         : { name: "forward", label: UIString("Forward") },
    InvalidPlacard  : { name: "invalid-placard", label: UIString("Invalid") },
    Pause           : { name: "pause", label: UIString("Pause") },
    PiPPlacard      : { name: "pip-placard", label: UIString("Picture in Picture") },
    Play            : { name: "play", label: UIString("Play") },
    Rewind          : { name: "rewind", label: UIString("Rewind") },
    ScaleToFill     : { name: "scale-to-fill", label: UIString("Scale to Fill") },
    ScaleToFit      : { name: "scale-to-fit", label: UIString("Scale to Fit") },
    SkipBack        : { name: "interval-skip-back", label: UIString("Skip Back 30 seconds") },
    Start           : { name: "start", label: UIString("Start") },
    Tracks          : { name: "media-selection", label: UIString("Media Selection") },
    Volume          : { name: "volume", label: UIString("Mute") },
    VolumeDown      : { name: "volume-down", label: UIString("Volume Down") },
    VolumeMuted     : {name: "volume-mute", label: UIString("Unmute") },
    VolumeUp        : { name: "volume-up", label: UIString("Volume Up") }
};

const IconsWithFullscreenVariants = [Icons.Airplay.name, Icons.Tracks.name, Icons.Pause.name, Icons.EnterPiP.name, Icons.Play.name, Icons.VolumeDown.name, Icons.VolumeUp.name];
const IconsWithCompactVariants = [Icons.Play.name, Icons.Pause.name, Icons.SkipBack.name, Icons.Volume.name, Icons.VolumeMuted.name, Icons.Airplay.name, Icons.EnterPiP.name, Icons.Tracks.name, Icons.EnterFullscreen.name];

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
            image.src = "data:image/png;base64," + this.mediaControlsHost.base64StringForIconNameAndType(fileName, "png");
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

        if (layoutTraits & LayoutTraits.macOS) {
            if (layoutTraits & LayoutTraits.Fullscreen && IconsWithFullscreenVariants.includes(iconName))
                iconName += "-fullscreen";
            else if (layoutTraits & LayoutTraits.Compact && IconsWithCompactVariants.includes(iconName))
                iconName += "-compact";
        }

        const fileName = `${iconName}@${window.devicePixelRatio}x`;

        return [fileName, platform];
    }

};
