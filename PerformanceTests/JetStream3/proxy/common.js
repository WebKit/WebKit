/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

"use strict";

function createLists(store, count) {
    for (let i = 0; i < count; i++) {
        store.lists.push(new ObservableReminderList(store, listName(i)));
    }
}

function createReminders(store, count) {
    store.lists.forEach((list, listIndex) => {
        for (let i = 0; i < count; i++) {
            const text = nouns[((listIndex * 10) + i) % nouns.length];
            list.reminders.push(new ObservableReminder(list, text));
        }
    });
}

function randomItem(array) {
    return array[array.length * Math.random() | 0];
}

function listName(index) {
    const date = new Date();
    date.setDate(date.getDate() + index);
    return date.toDateString();
}

const nouns = ["magazine", "passion", "statement", "extent", "judgment", "setting", "piano", "control", "employer", "speech", "presence", "indication", "philosophy", "worker", "supermarket", "atmosphere", "actor", "chest", "garbage", "feedback", "pizza", "speaker", "selection", "effort", "math", "gate", "profession", "recognition", "software", "coffee", "reading", "hotel", "concept", "poem", "population", "property", "office", "phone", "suggestion", "theory", "drawer", "death", "cigarette", "alcohol", "marketing", "session", "cheek", "establishment", "family", "revenue", "location", "mode", "awareness", "preference", "consequence", "cookie", "loss", "poetry", "cabinet", "difference", "story", "penalty", "king", "opinion", "climate", "imagination", "criticism", "meal", "desk", "dealer", "introduction", "physics", "customer", "child", "problem", "medicine", "church", "unit", "tongue", "communication", "storage", "stranger", "presentation", "discussion", "excitement", "role", "ladder", "estate", "town", "proposal", "employee", "administration", "variety", "television", "president", "inspector", "negotiation", "height", "comparison", "decision"];
const adjectives = ["fast", "cheerful", "hungry", "fantastic", "squealing", "critical", "yielding", "abashed", "shocking", "quickest"];

function addRandomTags(store) {
    forEachReminder(store, reminder => {
        if (Math.random() < 0.5) {
            reminder.tags.add(randomItem(adjectives));
        }

        if (Math.random() < 0.5) {
            reminder.tags.add(randomItem(adjectives));                
        }
    });
}

function completeSomeReminders(store) {
    forEachReminder(store, reminder => {
        reminder.isCompleted = Math.random() < 0.5;
    });
}

function sortRemindersByCompleted(store) {
    store.lists.forEach(list => {
        list.reminders.sort((a, b) => b.isCompleted - a.isCompleted);
    });
}

function pushBackRemindersWithTag(store, tag) {
    for (let i = store.lists.length - 2; i >= 0; i--) {
        const list = store.lists[i];
        const nextList = store.lists[i + 1];

        for (let j = list.reminders.length - 1; j >= 0; j--) {
            const reminder = list.reminders[j];
            if (reminder.tags.has(tag)) {
                reminder.moveToAnotherList(nextList);
            }
        }
    }
}

function removeRemindersWithTag(store, tag) {
    store.lists.forEach(list => {
        list.reminders = list.reminders.filter(reminder => !reminder.tags.has(tag));
    });
}

function removeCompletedReminders(store) {
    store.lists.forEach(list => {
        list.reminders = list.reminders.filter(reminder => !reminder.isCompleted);
    });
}

function removeFirstAndLastRemindersFromEachList(store) {
    store.lists.forEach(list => {
        list.reminders.shift();
        list.reminders.pop();
    });
}

function removeEmptyLists(store) {
    for (let i = store.lists.length - 1; i >= 0; i--) {
        const list = store.lists[i];
        if (list.isEmpty) {
            store.lists.splice(i, 1);
        }
    }
}

function forEachReminder(store, callback) {
    store.lists.forEach(list => {
        list.reminders.forEach(callback);
    });
}

function renderTags(entries) {
    return entries.map(([tag, count]) => `#${tag} (${count})`).join(", ") || "<none>";
}

class ReminderStore {
    constructor() {
        this.lists = [];
        this.allTimeCompletedTags = new Map;
    }

    get allTags() {
        const map = new Map;

        forEachReminder(this, reminder => {
            reminder.tags.forEach(tag => {
                const oldCount = map.get(tag) ?? 0;
                map.set(tag, oldCount + 1);
            });
        });

        return map;
    }

    get completedRemidersCount() {
        return this.lists.reduce((count, list) => count + list.completedRemidersCount, 0);
    }

    get remindersCount() {
        return this.lists.reduce((count, list) => count + list.reminders.length, 0);
    }

    get popularTags() {
        return this.popularTagsWithCount.map(e => e[0]);
    }

    get popularTagsWithCount() {
        return [...this.allTags].sort((a, b) => b[1] - a[1]).slice(0, 5);
    }

    render() {
        return [
            this.lists.map(list => list.render()).join("\n") || "<empty>",
            "-".repeat(31),
            `completed: ${this.completedRemidersCount} / ${this.remindersCount}`,
            "-".repeat(31),
            `current popular tags: ${renderTags(this.popularTagsWithCount)}`,
            `all-time completed tags: ${renderTags([...this.allTimeCompletedTags])}`,
        ].join("\n");
    }
}

class ReminderList {
    constructor(store, name) {
        this._store = store;

        this.name = name;
        this.reminders = [];
    }

    get store() {
        return this._store;
    }

    get indexInStore() {
        return this._store.lists.indexOf(this);
    }

    get isEmpty() {
        return !this.reminders.length;
    }

    get completedRemidersCount() {
        return this.reminders.filter(reminder => reminder.isCompleted).length;
    }

    render() {
        const content = this.isEmpty ? "<empty>" : this.reminders.map(reminder => reminder.render()).join("\n");

        return `--- ${this.name} (${this.completedRemidersCount} / ${this.reminders.length}) ---\n${content}`;
    }
}

class Reminder {
    constructor(list, text) {
        this._list = list;

        this.text = text;
        this.tags = new Set();
        this.isCompleted = false;
    }

    get list() {
        return this._list;
    }

    get indexInList() {
        return this._list.reminders.indexOf(this);
    }

    _isCompletedChanged(isCompleted) {
        if (isCompleted) {
            const tagsMap = this._list.store.allTimeCompletedTags;
            this.tags.forEach(tag => {
                const oldCount = tagsMap.get(tag) ?? 0;
                tagsMap.set(tag, oldCount + 1);
            });
        }
    }

    moveToAnotherList(newList) {
        this._list.reminders.splice(this.indexInList, 1);
        this._list = newList;
        newList.reminders.unshift(this);
    }

    render() {
        const tags = this.tags.size ? ` (${Array.from(this.tags, tag => `#${tag}`).join(" ")})` : "";

        return `[${this.isCompleted ? "x" : " "}] ${this.text}${tags}`;
    }
}

function test(store) {
    createLists(store, 10);
    createReminders(store, 10);
    addRandomTags(store);

    const { popularTags } = store;

    pushBackRemindersWithTag(store, popularTags[0]);
    removeRemindersWithTag(store, popularTags[2]);
    removeFirstAndLastRemindersFromEachList(store);

    completeSomeReminders(store);
    sortRemindersByCompleted(store);
    removeCompletedReminders(store);
    removeEmptyLists(store);
}
