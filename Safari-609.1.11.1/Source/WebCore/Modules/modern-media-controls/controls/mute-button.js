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

class MuteButton extends Button
{

    constructor(layoutDelegate)
    {
        super({
            cssClassName: "mute",
            iconName: Icons.Volume,
            layoutDelegate
        });

        this._muted = false;
        this._usesRTLIconVariant = false;
    }

    // Public

    get muted()
    {
        return this._muted;
    }

    set muted(flag)
    {
        if (this._muted === flag)
            return;

        this._muted = flag;
        this.needsLayout = true;
    }

    set usesRTLIconVariant(flag)
    {
        if (this._usesRTLIconVariant === flag)
            return;

        this._usesRTLIconVariant = flag;
        this.needsLayout = true;
    }

    // Protected

    layout()
    {
        if (this._usesRTLIconVariant)
            this.iconName = this._muted ? Icons.VolumeMutedRTL : Icons.VolumeRTL;
        else
            this.iconName = this._muted ? Icons.VolumeMuted : Icons.Volume;
    }

}
