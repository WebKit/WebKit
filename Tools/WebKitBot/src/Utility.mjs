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

import path from "path";

// https://api.slack.com/reference/surfaces/formatting#escaping
export function escapeForSlackText(text)
{
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;");
}

export function buildRevertSuccessMessage(user, result)
{
    if (!result.prUrl)
        return `<@${user}> Created a revert patch https://webkit.org/b/${escapeForSlackText(result.bugId)}`;

    if (result.action === "created")
        return `<@${user}> Created revert PR: ${escapeForSlackText(result.prUrl)}`;

    if (result.action === "updated")
        return `<@${user}> A revert PR for this already existed, so I updated it instead of creating a new one: ${escapeForSlackText(result.prUrl)}`;

    // git-webkit did not tell us whether it created or updated the pull request, so don't
    // claim a creation we never observed.
    return `<@${user}> Posted revert PR: ${escapeForSlackText(result.prUrl)}`;
}

export function dataLogLn(message)
{
    if (process.env.DEBUG)
        console.log(message);
}

export function isASCII(string)
{
    return /^[\u0000-\u007F]*$/.test(string);
}

export function rootDirectoryOfWebKitBot()
{
    return path.dirname(new URL(import.meta.url).pathname);
}

export function rootDirectoryOfWebKit()
{
    return path.resolve(rootDirectoryOfWebKitBot(), "..", "..", "..");
}
