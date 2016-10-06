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
    Pause   : "pause",
    Start   : "start"
};

const IconsWithFullScreenVariants = [Icons.Pause];

const iconService = new class IconService {

    constructor()
    {
        this.images = {};
    }

    imageForIconNameAndLayoutTraits(iconName, layoutTraits)
    {
        const path = this.urlForIconNameAndLayoutTraits(iconName, layoutTraits);
        
        let image = this.images[path];
        if (image)
            return image;

        image = this.images[path] = new Image;
        image.src = path;
        return image;
    }

    urlForIconNameAndLayoutTraits(iconName, layoutTraits)
    {
        let platform;
        if (layoutTraits & LayoutTraits.macOS)
            platform = "macOS";
        else if (layoutTraits & LayoutTraits.iOS)
            platform = "iOS";
        else
            throw "Could not identify icon's platform from layout traits.";

        if (layoutTraits & LayoutTraits.Fullscreen && IconsWithFullScreenVariants.includes(iconName))
            iconName += "-fullscreen";

        return `${this.directoryPath}/${platform}/${iconName}@${window.devicePixelRatio}x.png`;
    }

};
