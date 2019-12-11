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
    EnterFullscreen : { name: "EnterFullscreen", type: "svg", label: UIString("Enter Full Screen") },
    EnterPiP        : { name: "PipIn", type: "svg", label: UIString("Enter Picture in Picture") },
    ExitFullscreen  : { name: "ExitFullscreen", type: "svg", label: UIString("Exit Full Screen") },
    Forward         : { name: "Forward", type: "svg", label: UIString("Forward") },
    InvalidCompact  : { name: "InvalidCompact", type: "pdf", label: UIString("Invalid") },
    InvalidPlacard  : { name: "invalid-placard", type: "png", label: UIString("Invalid") },
    Pause           : { name: "Pause", type: "svg", label: UIString("Pause") },
    PiPPlacard      : { name: "pip-placard", type: "png", label: UIString("Picture in Picture") },
    Play            : { name: "Play", type: "svg", label: UIString("Play") },
    PlayCompact     : { name: "PlayCompact", type: "pdf", label: UIString("Play") },
    Rewind          : { name: "Rewind", type: "svg", label: UIString("Rewind") },
    SkipBack        : { name: "SkipBack15", type: "svg", label: UIString("Skip Back %s Seconds", SkipSeconds) },
    SkipForward     : { name: "SkipForward15", type: "svg", label: UIString("Skip Forward %s Seconds", SkipSeconds) },
    SpinnerCompact  : { name: "ActivityIndicatorSpriteCompact", type: "png", label: UIString("Loading…") },
    Tracks          : { name: "MediaSelector", type: "svg", label: UIString("Media Selection") },
    Volume          : { name: "VolumeHi", type: "svg", label: UIString("Mute") },
    VolumeRTL       : { name: "VolumeHi-RTL", type: "svg", label: UIString("Mute") },
    VolumeDown      : { name: "VolumeLo", type: "svg", label: UIString("Volume Down") },
    VolumeMuted     : { name: "Mute", type: "svg", label: UIString("Unmute") },
    VolumeMutedRTL  : { name: "Mute-RTL", type: "svg", label: UIString("Unmute") },
    VolumeUp        : { name: "VolumeHi", type: "svg", label: UIString("Volume Up") }
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

    imageForIconAndLayoutTraits(icon, layoutTraits)
    {
        const [fileName, platform] = this._fileNameAndPlatformForIconAndLayoutTraits(icon, layoutTraits);
        const path = `${platform}/${fileName}.${icon.type}`;

        let image = this.images[path];
        if (image)
            return image;

        image = this.images[path] = new Image;

        if (this.mediaControlsHost)
            image.src = `data:${MimeTypes[icon.type]};base64,${this.mediaControlsHost.base64StringForIconNameAndType(fileName, icon.type)}`;
        else
            image.src = `${this.directoryPath}/${path}`;

        return image;
    }

    // Private

    _fileNameAndPlatformForIconAndLayoutTraits(icon, layoutTraits)
    {
        let platform;
        if (layoutTraits & LayoutTraits.macOS)
            platform = "macOS";
        else if (layoutTraits & LayoutTraits.iOS || layoutTraits & LayoutTraits.Compact)
            platform = "iOS";
        else
            throw "Could not identify icon's platform from layout traits.";

        let iconName = icon.name;
        if (layoutTraits & LayoutTraits.macOS && layoutTraits & LayoutTraits.Fullscreen && IconsWithFullscreenVariants.includes(icon))
            iconName += "-fullscreen";

        let fileName = iconName;
        if (icon.type === "png")
            fileName = `${iconName}@${window.devicePixelRatio}x`;

        return [fileName, platform];
    }

};
