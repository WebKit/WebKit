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

class LayoutTraits
{

    constructor(mode)
    {
        this.mode = mode
    }

    get isFullscreen()
    {
        return this.mode == LayoutTraits.Mode.Fullscreen;
    }

    mediaControlsClass()
    {
        throw "Derived class must implement this function.";
    }

    overridenSupportingObjectClasses()
    {
        throw "Derived class must implement this function.";
    }

    resourceDirectory()
    {
        throw "Derived class must implement this function.";
    }

    controlsAlwaysAvailable()
    {
        throw "Derived class must implement this function.";
    }

    controlsNeverAvailable()
    {
        throw "Derived class must implement this function.";
    }

    supportsIconWithFullscreenVariant()
    {
        throw "Derived class must implement this function.";
    }

    supportsDurationTimeLabel()
    {
        throw "Derived class must implement this function.";
    }

    skipDuration()
    {
        throw "Derived class must implement this function.";
    }

    controlsDependOnPageScaleFactor()
    {
        throw "Derived class must implement this function.";
    }

    promoteSubMenusWhenShowingMediaControlsContextMenu()
    {
        throw "Derived class must implement this function.";
    }

    supportsTouches()
    {
        // Can be overridden by subclasses.

        return GestureRecognizer.SupportsTouches;
    }

    supportsAirPlay()
    {
        throw "Derived class must implement this function.";
    }

    supportsPiP()
    {
        throw "Derived class must implement this function.";
    }

    inheritsBorderRadius()
    {
        throw "Derived class must implement this function.";
    }
}

LayoutTraits.Mode = {
    Inline     : 0,
    Fullscreen : 1
};

// LayoutTraits subclasses should "register" themselves by adding themselves to this map.
window.layoutTraitsClasses = { };
