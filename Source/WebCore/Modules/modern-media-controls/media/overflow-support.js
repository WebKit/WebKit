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

class OverflowSupport extends MediaControllerSupport
{

    // Protected

    get mediaEvents()
    {
        let mediaEvents = [];
        window["WebKitAdditions.OverflowSupport.prototype.get_mediaEvents"]?.(this, mediaEvents);
        return mediaEvents;
    }

    get tracksToMonitor()
    {
        let tracksToMonitor = [];
        window["WebKitAdditions.OverflowSupport.prototype.get_tracksToMonitor"]?.(this, tracksToMonitor);
        return tracksToMonitor;
    }

    get control()
    {
        return this.mediaController.controls.overflowButton;
    }

    buttonWasPressed(control)
    {
        this.mediaController.showMediaControlsContextMenu(control);
    }

    syncControl()
    {
        this.control.enabled = this.mediaController.canShowMediaControlsContextMenu;

        let defaultContextMenuOptions = {};
        window["WebKitAdditions.OverflowSupport.prototype.syncControl"]?.(this, defaultContextMenuOptions);
        this.control.defaultContextMenuOptions = defaultContextMenuOptions;
    }

}
