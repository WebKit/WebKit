/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 * Copyright 2025 Google LLC
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

import { makeObservable, observable, action, reaction } from 'mobx';

export class Notifications {
    lastMessages = new Map(); // Map<string, Message>
    lastMessage;    roomDisposers = new Map(); // Map<string, Function>

    constructor(rooms) {
        makeObservable(this, {
            lastMessages: observable,
            updateLastMessage: action,
            addRoom: action,
            removeRoom: action,
        });

        rooms.forEach(room => this.addRoom(room));
    }

    addRoom(room) {
        const disposer = reaction(
            () => room.lastMessage,
            (message) => {
                if (message) {
                    this.updateLastMessage(room.name, message);
                }
            }
        );
        this.roomDisposers.set(room.name, disposer);
    }

    removeRoom(roomName) {
        if (this.roomDisposers.has(roomName)) {
            this.roomDisposers.get(roomName)(); // Dispose the reaction
            this.roomDisposers.delete(roomName);
            this.lastMessages.delete(roomName);
        }
    }

    updateLastMessage(roomName, message) {
        this.lastMessages.set(roomName, message);
    }
}