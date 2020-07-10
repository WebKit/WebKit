/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

import replaceAll from "replaceall";
import {escapeForSlackText} from "./Utility.mjs";

function findAndRemove(change, regExp)
{
    let matched = change.match(regExp);
    if (matched) {
        change = change.replace(regExp, "");
        return [matched[1], change];
    }
    return [null, change];
}

function cleanUpChange(change, contributors)
{
    for (let entry of contributors.entries) {
        if (!entry.nicks)
            continue;

        let nameWithNicks = `${entry.fullName} (@${entry.nicks[0]})`;
        if (change.includes(entry.fullName)) {
            change = replaceAll(entry.fullName, nameWithNicks, change);
            for (let email of entry.emails)
                change = replaceAll(`<${email}>`, "", change);
        } else {
            for (let email of entry.emails)
                change = replaceAll(` ${email} `, ` ${nameWithNicks} `, change);
        }
    }
    return change;
}

export default class Commit {
    constructor(feedItem, contributors)
    {
        let originalChange = feedItem.contentSnippet;
        let change = cleanUpChange(originalChange, contributors);
        [this._revision, change] = findAndRemove(change, /^git-svn-id: https:\/\/svn\.webkit\.org\/repository\/webkit\/trunk@(\d+) /im);
        [this._patchBy, change] = findAndRemove(change, /^Patch\s+by\s+(.+?)\s+on(?:\s+\d{4}-\d{2}-\d{2})?\n?/im);
        [this._revert, change] = findAndRemove(change, /(?:rolling out|reverting) (r?\d+(?:(?:,\s*|,?\s*and\s+)?r?\d+)*)\.?\s*/im);
        [this._bugzilla, change] = findAndRemove(change, /https?:\/\/(?:bugs\.webkit\.org\/show_bug\.cgi\?id=|webkit\.org\/b\/)(\d+)/im);
        [this._radar, change] = findAndRemove(change, /<rdar:\/\/(?:problem\/)?(\d+)>/im);
        this._email = feedItem.author;

        let lines = originalChange.split("\n");
        this._title = feedItem.title;
        if (lines.length)
            this._title = lines[0];

        if (this._patchBy)
            this._author = this._patchBy;
        if (!this._author) {
            this._author = this._email;
            if (this._email) {
                let entry = contributors.queryWithEmail(this._email);
                if (entry && entry.nicks && entry.nicks[0])
                    this._author = `${entry.fullName} (@${entry.nicks[0]})`;
            }
        }
        if (this._revert) {
            let matched = change.match(/^"?(.+?)"? \(Requested\s+by\s+(.+?)\s+on\s+#webkit\)\./im);
            if (matched) {
                // FIXME: Implement more descriptive message when we detect that this commit is revert commit.
                // let reason = matched[0];
                // let author = matched[1];
            }
        }

        if (!this._revision)
            throw new Error("Cannot find revision");
        this._revision = Number.parseInt(this._revision, 10);
        this._bugzilla = this._bugzilla ? Number.parseInt(this._bugzilla, 10) : null;
        this._radar = this._radar ? Number.parseInt(this._radar, 10) : null;
        this._url = `https://trac.webkit.org/r${this._revision}`;
    }

    get revision() { return this._revision; }
    get patchBy() { return this._patchBy; }
    get revert() { return this._revert; }
    get bugzilla() { return this._bugzilla; }
    get email() { return this._email; }
    get radar() { return this._radar; }
    get title() { return this._title; }
    get author() { return this._author; }
    get url() { return this._url; }

    message()
    {
        let results = [];
        results.push(escapeForSlackText(`${this._title}`));
        results.push(escapeForSlackText(`${this._url} by ${this._author}`));
        let urls = [];
        if (this._bugzilla !== null)
            urls.push(escapeForSlackText(`https://webkit.org/b/${this._bugzilla}`));
        if (this._radar !== null) {
            // Link it in Slack format `<>`.
            urls.push(`<rdar://problem/${this._radar}>`);
        }
        if (urls.length !== 0)
            results.push(urls.join(" "));
        return results.join("\n");
    }
}
