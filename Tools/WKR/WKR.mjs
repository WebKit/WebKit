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

import fs from "fs"
import path from "path"
import RSS from "rss-parser"
import replaceAll from "replaceall"
import storage from "node-persist"
import axios from "axios"
import dotenv from "dotenv"

dotenv.config();

// Change to true when debugging to avoid posting notifications to Slack.
const DEBUG = false;

const contributorsURL = "https://svn.webkit.org/repository/webkit/trunk/Tools/Scripts/webkitpy/common/config/contributors.json";
const commitEndpointURL = "https://git.webkit.org/?p=WebKit-https.git;a=atom";
const defaultInterval = 60 * 1000;

async function sleep(milliseconds)
{
    await new Promise(function (resolve) {
        setTimeout(resolve, milliseconds);
    });
}

class Contributors {
    static async create()
    {
        let response = await axios.get(contributorsURL);
        return new Contributors(response.data);
    }

    constructor(data)
    {
        this.emails = new Map();
        this.entries = Object.values(data);
        for (let [fullName, entry] of Object.entries(data)) {
            entry.fullName = fullName;
            for (let email of entry.emails)
                this.emails.set(email, entry);
        }
    }

    queryWithEmail(email)
    {
        return this.emails.get(email);
    }
}

class Commit {
    static findAndRemove(change, regExp)
    {
        let matched = change.match(regExp);
        if (matched) {
            change = change.replace(regExp, "");
            return [matched[1], change];
        }
        return [null, change];
    }

    static cleanUpChange(change, contributors)
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

    constructor(feedItem, contributors)
    {
        let originalChange = feedItem.contentSnippet;
        let change = Commit.cleanUpChange(originalChange, contributors);
        [this.revision, change] = Commit.findAndRemove(change, /^git-svn-id: https:\/\/svn\.webkit\.org\/repository\/webkit\/trunk@(\d+) /im);
        [this.patchBy, change] = Commit.findAndRemove(change, /^Patch\s+by\s+(.+?)\s+on(?:\s+\d{4}-\d{2}-\d{2})?\n?/im);
        [this.revert, change] = Commit.findAndRemove(change, /(?:rolling out|reverting) (r?\d+(?:(?:,\s*|,?\s*and\s+)?r?\d+)*)\.?\s*/im);
        [this.bugzilla, change] = Commit.findAndRemove(change, /https?:\/\/(?:bugs\.webkit\.org\/show_bug\.cgi\?id=|webkit\.org\/b\/)(\d+)/im);
        this.email = feedItem.author;

        let lines = originalChange.split('\n');
        this.title = feedItem.title;
        if (lines.length)
            this.title = lines[0];

        if (this.patchBy)
            this.author = this.patchBy;
        if (!this.author) {
            this.author = this.email;
            if (this.email) {
                let entry = contributors.queryWithEmail(this.email);
                if (entry && entry.nicks && entry.nicks[0])
                    this.author = `${entry.fullName} (@${entry.nicks[0]})`;
            }
        }
        if (this.revert) {
            let matched = change.match(/^\"?(.+?)\"? \(Requested\s+by\s+(.+?)\s+on\s+#webkit\)\./im);
            if (matched) {
                let reason = matched[0];
                let author = matched[1];
                // FIXME: Implement more descriptive message when we detect that this commit is revert commit.
            }
        }
        if (!this.revision)
            throw new Error(`Canont find revision`);
        this.revision = Number.parseInt(this.revision, 10);
        this.url = `https://trac.webkit.org/r${this.revision}`;
    }

    message()
    {
        let results = [];
        results.push(`${this.title}`);
        results.push(`${this.url} by ${this.author}`);
        if (this.bugzilla)
            results.push(`https://webkit.org/b/${this.bugzilla}`);
        return results.join('\n');
    }
}

class WKR {
    constructor(revision)
    {
        this.revision = revision;
    }

    async postToSlack(commit)
    {
        let data = {
            "text": commit.message()
        };
        console.log(data);
        if (!DEBUG)
            await axios.post(process.env.slackURL, JSON.stringify(data));
        await sleep(500);
    }

    async action(interval)
    {
        try {
            console.log(`${Date.now()}: poll data`);
            let contributors = await Contributors.create();
            let parser = new RSS;
            let response = await parser.parseURL(commitEndpointURL);
            let commits = response.items.map((feedItem) => new Commit(feedItem, contributors));
            commits.sort((a, b) => a.revision - b.revision);
            if (this.revision) {
                commits = commits.filter((commit) => commit.revision > this.revision);
                for (let commit of commits)
                    await this.postToSlack(commit);
            }

            let latestCommit = commits[commits.length - 1];
            if (latestCommit) {
                this.revision = latestCommit.revision;
                await storage.setItem("revision", this.revision);
            }
        } catch (error) {
            console.error(String(error));
        }
        return defaultInterval;
    }

    static async create()
    {
        await storage.init({
            dir: 'data',
        });
        let revision = await storage.getItem("revision");
        console.log(`Previous Revision: ${revision}`);
        console.log(`Endpoint: ${process.env.slackURL}`);
        return new WKR(revision);
    }

    static async main()
    {
        let bot = await WKR.create();
        let interval = defaultInterval;
        for (;;) {
            let start = Date.now();
            interval = await bot.action(interval);
            await sleep(Math.max(interval - (Date.now() - start), 0));
        }
    }
}

WKR.main();
