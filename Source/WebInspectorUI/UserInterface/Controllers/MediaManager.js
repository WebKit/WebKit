/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WI.MediaManager = class MediaManager extends WI.Object
{
    constructor()
    {
        super();

        this._enabled = false;
        this._mediaPlayerIdentifierMap = new Map;

        WI.Frame.addEventListener(WI.Frame.Event.MainResourceDidChange, this._mainResourceDidChange, this);
    }

    // Agent

    get domains() { return ["Media"]; }

    activateExtraDomain(domain)
    {
        // COMPATIBILITY (iOS 14.0): Inspector.activateExtraDomains was removed in favor of a declared debuggable type

        console.assert(domain === "Media");

        for (let target of WI.targets)
            this.initializeTarget(target);
    }

    // Target

    initializeTarget(target)
    {
        if (!this._enabled)
            return;

        if (target.hasDomain("Media"))
            target.MediaAgent.enable();
    }

    // Public

    get mediaPlayerIdentifierMap() { return this._mediaPlayerIdentifierMap; }

    enable()
    {
        console.assert(!this._enabled);

        this._enabled = true;

        for (let target of WI.targets)
            this.initializeTarget(target);
    }

    disable()
    {
        console.assert(this._enabled);

        for (let target of WI.targets) {
            if (target.hasDomain("Media"))
                target.MediaAgent.disable();
        }

        this._clearPlayers();

        this._enabled = false;
    }

    // MediaObserver
    
    playerAdded(playerPayload)
    {
        let player = WI.MediaPlayer.fromPayload(playerPayload);
        if (this._mediaPlayerIdentifierMap.has(player.identifier))
            return;
        this._mediaPlayerIdentifierMap.set(player.identifier, player);
        this.dispatchEventToListeners(WI.MediaManager.Event.MediaPlayerAdded, player);
    }

    playerRemoved(playerIdentifier)
    {
        let player = this._mediaPlayerIdentifierMap.take(playerIdentifier);
        if (!player)
            return;
        console.assert(player);
        this.dispatchEventToListeners(WI.MediaManager.Event.MediaPlayerRemoved, player);
    }

    playerUpdated(playerPayload, event, data)
    {
        let player = this._mediaPlayerIdentifierMap.take(playerPayload.playerId);
        if (!player)
            return;
        const newPlayer = WI.MediaPlayer.fromPayload(playerPayload);
        const difference = this._calculatePlayerDifference(player.properties(), newPlayer.properties());
        player.updateFromPayload(playerPayload);
        player.pushEvent({event, data: difference /* data */});
        this._mediaPlayerIdentifierMap.set(player.identifier, player)
        this.dispatchEventToListeners(WI.MediaManager.Event.MediaPlayerUpdated, player);
    }

    // Private

    _mainResourceDidChange(event)
    {
        console.assert(event.target instanceof WI.Frame);
        if (!event.target.isMainFrame())
            return;
        
        this._clearPlayers();
    }

    _clearPlayers()
    {
        for (let player of this._mediaPlayerIdentifierMap.values())
            this.dispatchEventToListeners(WI.MediaManager.Event.MediaPlayerRemoved, player);
        this._mediaPlayerIdentifierMap.clear();
    }

    _calculatePlayerDifference(oldPlayer, newPlayer)
    {
        let difference = {old: {}, new: {}};
        for (let key of WI.MediaPlayer.Properties) {
            const oldData = JSON.stringify(oldPlayer[key]);
            const newData = JSON.stringify(newPlayer[key]);
            if (oldData != newData) {
                difference.old[key] = oldData;
                difference.new[key] = newData;
            }
        }
        
        if (this._isEmpty(difference.old))
            delete difference.old;

        if (this._isEmpty(difference.new))
            delete difference.new;

        return !this._isEmpty(difference) ? JSON.stringify(difference) : '';
    }

    _isEmpty(object)
    {
        for (let _ in object) { return false; }
        return true;
    }
};

WI.MediaManager.Event = {
    MediaPlayerAdded: "media-player-added",
    MediaPlayerRemoved: "media-player-removed",
    MediaPlayerUpdated: "media-player-updated"
}
