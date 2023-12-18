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

import {stat} from "fs";
import path from "path";
import util from "util";
import which from "which";
import {execFile, spawn} from "child_process";
import { HttpsProxyAgent } from 'https-proxy-agent';
import LogLevel from "@slack/rtm-api";
import SlackRTMAPI from "@slack/rtm-api";
import AsyncTaskQueue from "./AsyncTaskQueue.mjs";
import {dataLogLn, escapeForSlackText, isASCII, rootDirectoryOfWebKit} from "./Utility.mjs";

const defaultTaskLimit = 10;
const defaultPullPeriod = 60 * 1000 * 60;
const defaultTimeoutForRevert = 60 * 1000 * 10;

const execFileAsync = util.promisify(execFile);
const statAsync = util.promisify(stat);

function parseBugId(string)
{
    if (!string)
        return null;

    let match = string.match(/^https?:\/\/webkit\.org\/b\/(\d+)$/m);
    if (match)
        return match[1];

    match = string.match(/^https?:\/\/bugs\.webkit\.org\/show_bug\.cgi\?id=(\d+)(?:&ctype=xml|&excludefield=attachmentdata)*$/m);
    if (match)
        return match[1];

    return null;
}

function extractRevision(text)
{
    let revisions = [];
    for (let candidate of text.split(",")) {
        candidate = candidate.trim();
        if (!candidate)
            continue;

        let match = candidate.match(/^r?(\d{5,6}|\d+\@[^:\s]+|[0-9a-f]{6,40}):?$/);
        if (!match)
            return null;

        revisions.push(match[1]);
    }
    return revisions;
}

export function extractRevisionsAndReason(args)
{
    let revisions = [];
    let reason = "";
    for (let i = 0; i < args.length; ++i) {
        let arg = args[i];
        let extracted = extractRevision(arg);
        if (!extracted) {
            let reasons = [];
            for (; i < args.length; ++i)
                reasons.push(args[i]);
            reason = reasons.join(" ").trim();
            break;
        }
        revisions.push(...extracted);
    }

    // If reason starts with quote and ends with the same quote, remove them once.
    if (reason.length >= 2) {
        let firstCharacterOfReason = reason.charAt(0);
        if (firstCharacterOfReason === "'" || firstCharacterOfReason === "\"" || firstCharacterOfReason === "`") {
            if (reason.charAt(reason.length - 1) === firstCharacterOfReason)
                reason = reason.slice(1, reason.length - 1);
        }
    }

    return {revisions, reason};
}

export function extractCommandAndArgs(text)
{
    let args = text.trim().split(/\s+/);
    let command = args.shift().toLowerCase();
    return {command, args};
}

export function extractTextIfMentioned(text, id)
{
    let regexp = new RegExp(`<@${id}>`);
    let globalRegexp = new RegExp(`<@${id}>`, "g");
    let matched = text.match(regexp);
    if (!matched)
        return null;

    text = text.replace(globalRegexp, "");

    // Preprocessing for the text.
    // 1. Convert smart quotes to normal ASCII quotes because webkit-patch cannot accept non-ASCII text and slack may convert normal quotes to smart quotes.
    text = text.replace(/[\u2018\u2019]/g, "'");
    text = text.replace(/[\u201C\u201D]/g, "\"");

    // 2. Convert line-terminators to spaces. It is unlikely that we want to have line-terminators in webkitbot commands.
    text = text.replace(/(\r\n|\n|\r|\u2028|\u2029)/g, " ");

    return text;
}

export default class WebKitBot {
    constructor(webClient, auth)
    {
        this._taskQueue = new AsyncTaskQueue(defaultTaskLimit);
        this._web = webClient;
        this._auth = auth;

        this._commands = new Map;

        let revertCommand = {
            description: "Opens a bug to revert the specified revision, CCing author + reviewer, and attaching the reverse-diff of the given revisions marked as commit-queue=?.",
            usage: `\`revert SVN_REVISION [SVN_REVISIONS] REASON\`
e.g. \`revert 260220 Ensure it is working after refactoring\`
\`revert 260220,260221 Ensure it is working after refactoring\``,
            operation: this.revertCommand.bind(this),
        };
        this._commands.set("rollout", revertCommand);
        this._commands.set("revert", revertCommand);
        this._commands.set("dry-revert", {
            description: "Parse revert message, but do not revert actually.",
            usage: `\`dry-revert SVN_REVISION [SVN_REVISIONS] REASON\`
e.g. \`dry-revert 260220 Ensure it is working after refactoring\`
\`dry-revert 260220,260221 Ensure it is working after refactoring\``,
            operation: this.dryRevertCommand.bind(this),
        });
        this._commands.set("ping", {
            description: "Responds with pong to check if WebKitBot is alive/working",
            usage: "`ping`",
            operation: this.pingCommand.bind(this),
        });
        this._commands.set("pull", {
            description: "Pulls the latest checkout of WebKit checkout for reverting queue.",
            usage: "`pull`",
            operation: this.pullCommand.bind(this),
        });

        this._commands.set("help", {
            description: "Provides help on my individual commands.",
            usage: "`help [COMMAND]`",
            operation: this.helpCommand.bind(this),
        });
        this._commands.set("status", {
            description: "Shows current reverting queue status.",
            usage: "`status`",
            operation: this.statusCommand.bind(this),
        });
        this._commands.set("hi", {
            description: "Responds with hi.",
            usage: "`hi`",
            operation: this.hiCommand.bind(this),
        });
        this._commands.set("yt?", {
            description: "Responds with yes.",
            usage: "`yt?`",
            operation: this.youThereCommand.bind(this),
        });

        const proxy = new HttpsProxyAgent(process.env.http_proxy);
        this._rtm = new SlackRTMAPI.RTMClient(process.env.SLACK_TOKEN, { agent: proxy, logLevel: LogLevel.DEBUG });
        this._rtm.on("message", async (event) => {
            if (event.type !== "message")
                return;

            // If message has subtype, this is not an usual message.
            if (event.subtype)
                return;

            let text = extractTextIfMentioned(event.text, this._auth.user_id);
            if (text) {
                let {command, args} = extractCommandAndArgs(text);
                let operation = this._commands.get(command);
                if (operation)
                    await operation.operation(event, command, args);
                else
                    await this.unknownCommand(event, command, args);
            }
        });
        setInterval(() => {
            this._taskQueue.postOrFailWhenExceedingLimit({
                command: "pull",
            });
        }, defaultPullPeriod);
    }

    async revertCommand(event, command, args)
    {
        let {revisions, reason} = extractRevisionsAndReason(args);

        if (!isASCII(reason)) {
            await this._web.chat.postMessage({
                channel: event.channel,
                text: `<@${event.user}> webkit-patch only accepts an ASCII string for reason: \`${escapeForSlackText(reason)}\``,
            });
            return;
        }

        dataLogLn(revisions, reason);
        if (revisions.length) {
            try {
                await this._web.chat.postMessage({
                    channel: event.channel,
                    text: `<@${event.user}> Preparing revert for ${revisions.map((revision) => {
                        let revRepr = revision;
                        if (revRepr.match(/^\d+$/))
                            revRepr = `r${revRepr}`;
                        return `<${escapeForSlackText(`https://commits.webkit.org/${revRepr}|${revRepr}`)}>`;
                    }).join(" ")} ...`,
                });
                let bugId = await this._taskQueue.postOrFailWhenExceedingLimit({
                    command: "revert",
                    revisions,
                    reason,
                });
                await this._web.chat.postMessage({
                    channel: event.channel,
                    text: `<@${event.user}> Created a revert patch https://webkit.org/b/${escapeForSlackText(bugId)}`,
                });
            } catch (error) {
                console.error(error);
                let stderr = error.stderr;
                console.error("STDERR ", stderr);
                if (typeof stderr === "string") {
                    {
                        let index = stderr.indexOf("Failed to apply reverse diff for revision");
                        if (index !== -1) {
                            let lines = stderr.slice(index).split("\n");
                            lines.shift();
                            let files = [];
                            for (let line of lines) {
                                try {
                                    await statAsync(path.join(process.env.webkitWorkingDirectory, line));
                                    files.push(line);
                                } catch {
                                    // THis is not a file.
                                    break;
                                }
                            }
                            if (files.length !== 0) {
                                await this._web.chat.postMessage({
                                    channel: event.channel,
                                    text: `<@${event.user}> Failed to create revert patch because of the following conflicts:
\`\`\`
${escapeForSlackText(files.join("\n"))}\`\`\``,
                                });
                                return;
                            }
                        }
                    }
                    {
                        let index = stderr.indexOf("You are not authorized to access bug");
                        if (index !== -1) {
                            let line = stderr.slice(index).split("\n")[0].trim();
                            let matched = /#(\d+)/.match(line);
                            await this._web.chat.postMessage({
                                channel: event.channel,
                                text: `<@${event.user}> Failed to create revert patch. Please ensure commit-queue@webkit.org is authorized to access ${matched ? ("bug " + escapeForSlackText(matched[1])) : "the bug"}.`
                            });
                            return;
                        }

                    }
                }
                await this._web.chat.postMessage({
                    channel: event.channel,
                    text: `<@${event.user}> Failed to create revert patch.` + (stderr ? `
\`\`\`
${escapeForSlackText(stderr)}\`\`\`` : ""),
                });
            }
            return;
        }

        await this._web.chat.postMessage({
            channel: event.channel,
            text: `<@${event.user}> Failed to parse revision and reason`,
        });
    }

    async dryRevertCommand(event, command, args)
    {
        let {revisions, reason} = extractRevisionsAndReason(args);

        if (!isASCII(reason)) {
            await this._web.chat.postMessage({
                channel: event.channel,
                text: `<@${event.user}> webkit-patch only accepts an ASCII string for reason: \`${escapeForSlackText(reason)}\``,
            });
            return;
        }

        if (!revisions.length) {
            await this._web.chat.postMessage({
                channel: event.channel,
                text: `<@${event.user}> No revision is found: reason = \`${escapeForSlackText(reason)}\``,
            });
            return;
        }

        await this._web.chat.postMessage({
            channel: event.channel,
            text: `<@${event.user}> revisions = \`${escapeForSlackText(revisions.join(","))}\`, reason = \`${escapeForSlackText(reason)}\``,
        });
    }

    async pullCommand(event, command, args)
    {
        await this._web.chat.postMessage({
            channel: event.channel,
            text: `<@${event.user}> Preparing pulling the latest WebKit checkout.`,
        });
        await this._taskQueue.postOrFailWhenExceedingLimit({
            command: "pull",
        });
        await this._web.chat.postMessage({
            channel: event.channel,
            text: `<@${event.user}> Pulled the latest checkout.`,
        });
    }

    async pingCommand(event, command, args)
    {
        await this._web.chat.postMessage({
            channel: event.channel,
            text: `<@${event.user}> pong`,
        });
    }

    async helpCommand(event, command, args)
    {
        if (args.length) {
            let commandName = args[0];
            let operation = this._commands.get(commandName);
            if (operation) {
                await this._web.chat.postMessage({
                    channel: event.channel,
                    text: `<@${event.user}> \`${escapeForSlackText(commandName)}\`: ${escapeForSlackText(operation.description)}
Usage: ${escapeForSlackText(operation.usage)}`,
                });
            } else {
                await this._web.chat.postMessage({
                    channel: event.channel,
                    text: `<@${event.user}> Unknown command \`${escapeForSlackText(commandName)}\``,
                });
            }
        } else {
            let commandNames = [];
            for (let key of this._commands.keys())
                commandNames.push("`" + key + "`");
            await this._web.chat.postMessage({
                channel: event.channel,
                text: `<@${event.user}> Available commands: ${escapeForSlackText(commandNames.join(", "))}
Type \`help COMMAND\` for help on my individual commands.`,
            });
        }
    }

    async statusCommand(event, command, args)
    {
        await this._web.chat.postMessage({
            channel: event.channel,
            text: `<@${event.user}> ${escapeForSlackText(String(this._taskQueue.length))} requests in queue.`,
        });
    }

    async hiCommand(event, command, args)
    {
        await this._web.chat.postMessage({
            channel: event.channel,
            text: `Hi <@${event.user}>!`,
        });
    }

    async youThereCommand(event, command, args)
    {
        await this._web.chat.postMessage({
            channel: event.channel,
            text: `<@${event.user}> yes`,
        });
    }

    async unknownCommand(event, command, args)
    {
        dataLogLn("Unknown command: ", command);
        await this._web.chat.postMessage({
            channel: event.channel,
            text: `<@${event.user}> Unknown command \`${escapeForSlackText(command)}\``,
        });
    }

    execInWebKitDirectorySimple(command, args)
    {
        return new Promise((resolve, reject) => {
            let task = spawn(command, args, {
                cwd: process.env.webkitWorkingDirectory,
                env: {},
                stdio: "inherit",
            });
            task.on("close", (code) => {
                if (!code)
                    resolve(code);
                else
                    reject(code);
            });
        });
    }

    async cleanUpWorkingCopy()
    {
        dataLogLn("1. Resetting");
        await this.execInWebKitDirectorySimple("git", ["reset", "--hard"]);

        dataLogLn("2. Cleaning");
        await this.execInWebKitDirectorySimple("git", ["clean", "-df"]);

        dataLogLn("3. Fetching");
        await this.execInWebKitDirectorySimple("git", ["fetch", "origin"]);

        dataLogLn("4. Checkout out origin/main");
        await this.execInWebKitDirectorySimple("git", ["checkout", "origin/main", "-f"]);

        dataLogLn("5. Deleting local 'main' ref");
        await this.execInWebKitDirectorySimple("git", ["branch", "-D", "main"]);

        dataLogLn("6. Creating local 'main' ref");
        await this.execInWebKitDirectorySimple("git", ["checkout", "origin/main", "-b", "main"]);
    }

    async generateRevertingPatch(revisions, reason)
    {
        dataLogLn("Reverting ", revisions, reason);
        let revisionsArgument = revisions.join(" ");

        if (reason.startsWith("-"))
            throw new Error(`The revert reason may not begin with - ("${reason}")`);

        await this.cleanUpWorkingCopy();

        dataLogLn("7. Creating revert patch ", revisions, reason);
        let results;
        try {
            const webkitPatchPath = path.resolve("BotWebKit", "Tools", "Scripts", "webkit-patch");
            var pythonPath = which.sync('python3')
            results = await execFileAsync(pythonPath, [
                webkitPatchPath,
                "create-revert",
                "--force-clean",
                // In principle, we should pass --non-interactive here, but it
                // turns out that create-revert doesn't need it yet.  We can't
                // pass it prophylactically because we reject unrecognized command
                // line switches.
                "--parent-command=sheriff-bot",
                revisionsArgument,
                reason,
            ], {
                cwd: process.env.webkitWorkingDirectory,
                env: {
                    CHANGE_LOG_NAME: "Commit Queue",
                    CHANGE_LOG_EMAIL_ADDRESS: "commit-queue@webkit.org",
                    WEBKIT_BUGZILLA_USERNAME: process.env.webkitBugzillaUsername,
                    WEBKIT_BUGZILLA_PASSWORD: process.env.webkitBugzillaPassword,
                    http_proxy: process.env.http_proxy,
                    https_proxy: process.env.http_proxy,
                },
                timeout: defaultTimeoutForRevert,
                maxBuffer: 1024 * 1024 * 50,
            });
        } catch (error) {
            dataLogLn(error);
            let newError = new Error("Revert command failed");
            newError.stderr = error.stderr;
            throw newError;
        }
        let {stdout, stderr} = results;
        dataLogLn(stdout);
        dataLogLn(stderr);
        {
            let bugId = parseBugId(stdout);
            if (bugId !== null)
                return bugId;
        }
        {
            let bugId = parseBugId(stderr);
            if (bugId !== null)
                return bugId;
        }
        throw new Error("bug-id cannot be found");
    }

    async action(task)
    {
        dataLogLn(task);
        switch (task.command) {
        case "revert": {
            let {revisions, reason} = task;
            return this.generateRevertingPatch(revisions, reason);
        }
        case "pull":
            return this.cleanUpWorkingCopy();
        }
        throw new Error(`${task.command} is undefined action`);
    }

    static async create(webClient, auth)
    {
        let bot = new WebKitBot(webClient, auth);
        await bot._rtm.start();
        return bot;
    }

    static async main(webClient, auth)
    {
        let bot = await WebKitBot.create(webClient, auth);
        while (true) {
            let {task, resolve, reject} = await bot._taskQueue.take();
            try {
                let result = await bot.action(task);
                resolve(result);
            } catch (error) {
                reject(error);
            }
        }
    }
}
