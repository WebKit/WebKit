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

import { makeObservable, observable, computed, action } from 'mobx';

const MAX_ROOM_UPDATES = 10;

export class Room {
    name;
    people = new Map();
    messages = [];
    roomUpdates = [];

    constructor(name) {
        makeObservable(this, {
            name: observable,
            people: observable,
            messages: observable,
            roomUpdates: observable,
            messageCount: computed,
            peopleCount: computed,
            lastMessage: computed,
            members: computed,
            addPerson: action,
            removePerson: action,
            addMessage: action,
            addStatusUpdate: action,
            dispose: action,
        });
        this.name = name;
    }

    get messageCount() {
        return this.messages.length;
    }

    get peopleCount() {
        return this.people.size;
    }

    get lastMessage() {
        if (this.messages.length === 0) {
            return null;
        }
        return this.messages[this.messages.length - 1];
    }

    get members() {
        return Array.from(this.people.values());
    }

    addPerson(person) {
        if (!this.people.has(person.id)) {
            this.people.set(person.id, person);
            person.addRoom(this);
        }
    }

    removePerson(personId) {
        const person = this.people.get(personId);
        if (person) {
            person.removeRoom(this);
            this.people.delete(personId);
        }
    }

    addMessage(message) {
        this.messages.push(message);
        message.author.setLastMessage(message);
    }

    addStatusUpdate(person, status) {
        this.roomUpdates.push(`${person.name} is now ${status}`);
        if (this.roomUpdates.length > MAX_ROOM_UPDATES * 2)
            this.roomUpdates = this.roomUpdates.slice(MAX_ROOM_UPDATES);
    }

    dispose(notifications) {
        this.people.forEach(person => person.removeRoom(this));
        if (notifications) {
            notifications.removeRoom(this.name);
        }
    }
}