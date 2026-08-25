/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

// Pure parsing of webkitbot commands. This module deliberately has no imports so that
// tests can exercise it without pulling in the Slack clients and their dependencies.

export function parseBugId(string)
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

export function parsePRUrl(string)
{
    if (!string)
        return null;

    let match = string.match(/https:\/\/github\.com\/WebKit\/WebKit\/pull\/\d+/im);
    return match ? match[0] : null;
}

function extractRevision(text)
{
    let revisions = [];
    for (let candidate of text.split(",")) {
        candidate = candidate.trim();
        if (!candidate)
            continue;

        // Accept identifiers pasted as commits.webkit.org links, which is what webkitbot itself posts.
        candidate = candidate.replace(/^https?:\/\/commits\.webkit\.org\//, "");

        let match = candidate.match(/^r?(\d{5,6}|\d+@[^:\s]+|[0-9a-f]{6,40}):?$/);
        if (!match)
            return null;

        revisions.push(match[1]);
    }
    return revisions;
}

export function buildGitWebkitRevertCommand(gitWebkitPath, revisions, reason, issueUrl)
{
    let args = [
        gitWebkitPath,
        "revert",
        ...revisions,
        "--pr",
        "--defaults",
        "--no-checks",
    ];

    if (issueUrl)
        args.push("--issue", issueUrl);
    else
        args.push("--reason", reason);

    return args;
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

    // 3. Strip Slack's auto-linked URLs: <https://url> → https://url, <https://url|label> → https://url
    text = text.replace(/<(https?:\/\/[^|>]+)(?:\|[^>]*)?>/g, "$1");

    // 4. Strip Slack's auto-linked email addresses: <mailto:target|label> → target.
    //    Slack mistakes commit identifiers for email addresses, so "319186@main" arrives as
    //    <mailto:319186@main|319186@main>.
    text = text.replace(/<mailto:([^|>\s]+)(?:\|[^>]*)?>/g, "$1");

    return text;
}
