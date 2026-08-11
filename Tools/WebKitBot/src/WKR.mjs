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

import RSS from "rss-parser";
import storage from "node-persist";
import axios from "axios";
import Commit from "./Commit.mjs";
import Contributors from "./Contributors.mjs";
import {dataLogLn} from "./Utility.mjs";

// Change to true when debugging to avoid posting notifications to Slack.
const DRY = false;

const commitEndpointURL = "https://git.webkit.org/?p=WebKit-https.git;a=atom";
const defaultInterval = 60 * 1000;

async function sleep(milliseconds)
{
    return new Promise((resolve) => setTimeout(resolve, milliseconds));
}

export default class WKR {
    constructor(webClient, auth, revision)
    {
        this._web = webClient;
        this._auth = auth;
        this._revision = revision;
    }

    async postToSlack(commit)
    {
        let data = {
            text: commit.message(),
        };
        dataLogLn(data);
        if (!DRY)
            await axios.post(process.env.slackURL, JSON.stringify(data));
        await sleep(500);
    }

    async action()
    {
        dataLogLn(`${Date.now()}: poll data`);
        let contributors = await Contributors.create();
        let parser = new RSS;
        let response = await parser.parseURL(commitEndpointURL);
        let commits = response.items.map((feedItem) => new Commit(feedItem, contributors));
        commits.sort((a, b) => a.revision - b.revision);
        if (this._revision) {
            commits = commits.filter((commit) => commit.revision > this._revision);
            for (let commit of commits)
                await this.postToSlack(commit);
        }

        let latestCommit = commits[commits.length - 1];
        if (latestCommit) {
            this._revision = latestCommit.revision;
            await storage.setItem("revision", this._revision);
        }
    }

    static async create(webClient, auth)
    {
        let revision = await storage.getItem("revision");
        dataLogLn(`Previous Revision: ${revision}`);
        dataLogLn(`Endpoint: ${process.env.slackURL}`);
        return new WKR(webClient, auth, revision);
    }

    static async main(webClient, auth)
    {
        let bot = await WKR.create(webClient, auth);
        while (true) {
            let start = Date.now();
            try {
                await bot.action();
            } catch (error) {
                console.error(error);
            }
            await sleep(Math.max(defaultInterval - (Date.now() - start), 0));
        }
    }
}
