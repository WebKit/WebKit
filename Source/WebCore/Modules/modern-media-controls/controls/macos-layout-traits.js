/*
 * Copyright (C) 2021 Apple Inc. All Rights Reserved.
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

class MacOSLayoutTraits extends LayoutTraits
{
    mediaControlsClass()
    {
        if (this.isFullscreen)
            return MacOSFullscreenMediaControls;
        return MacOSInlineMediaControls;
    }

    overridenSupportingObjectClasses()
    {
        return null;
    }

    resourceDirectory()
    {
        return "macOS";
    }

    controlsAlwaysAvailable()
    {
        return this.isFullscreen;
    }

    controlsNeverAvailable()
    {
        return false;
    }

    supportsIconWithFullscreenVariant()
    {
        return this.isFullscreen;
    }

    supportsDurationTimeLabel()
    {
        return true;
    }

    controlsDependOnPageScaleFactor()
    {
        return false;
    }

    skipDuration()
    {
        return 15;
    }

    promoteSubMenusWhenShowingMediaControlsContextMenu()
    {
        return true;
    }

    supportsTouches()
    {
        return false;
    }

    supportsAirPlay()
    {
        return true;
    }

    supportsPiP()
    {
        return true;
    }

    inheritsBorderRadius()
    {
        return false;
    }

    toString()
    {
        const mode = this.isFullscreen ? "Fullscreen" : "Inline";
        return `[MacOSLayoutTraits ${mode}]`;
    }
}

window.layoutTraitsClasses["MacOSLayoutTraits"] = MacOSLayoutTraits;
