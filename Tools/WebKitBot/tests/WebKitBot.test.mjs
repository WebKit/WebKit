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

import {extractRevisionsAndReason, extractTextIfMentioned, extractCommandAndArgs} from "../src/WebKitBot.mjs";
const WebKitBotID = 42;

test("revert command basic", () => {
    let message = `<@${WebKitBotID}> revert 263483 testing revert`;

    let text = extractTextIfMentioned(message, WebKitBotID);
    expect(text).toBe(" revert 263483 testing revert");

    let {command, args} = extractCommandAndArgs(text);
    expect(command).toBe("revert");
    expect(args).toEqual(["263483", "testing", "revert"]);

    let {revisions, reason} = extractRevisionsAndReason(args);
    expect(reason).toBe("testing revert");
    expect(revisions).toEqual(["263483"]);
});

test("revert command not mentioned", () => {
    let message = "revert 263483 testing revert";

    let text = extractTextIfMentioned(message, WebKitBotID);
    expect(text).toBe(null);
});

test("remove quotes in revert reason", () => {
    let message = `<@${WebKitBotID}> revert 263483 "testing revert"`;

    let text = extractTextIfMentioned(message, WebKitBotID);
    expect(text).toBe(" revert 263483 \"testing revert\"");

    let {command, args} = extractCommandAndArgs(text);
    expect(command).toBe("revert");
    expect(args).toEqual(["263483", "\"testing", "revert\""]);

    let {revisions, reason} = extractRevisionsAndReason(args);
    expect(reason).toBe("testing revert");
    expect(revisions).toEqual(["263483"]);
});

test("remove single smartquotes in revert reason", () => {
    let message = `<@${WebKitBotID}> revert 263483 \u2018testing revert\u2019`;

    let text = extractTextIfMentioned(message, WebKitBotID);
    expect(text).toBe(" revert 263483 'testing revert'");

    let {command, args} = extractCommandAndArgs(text);
    expect(command).toBe("revert");
    expect(args).toEqual(["263483", "'testing", "revert'"]);

    let {revisions, reason} = extractRevisionsAndReason(args);
    expect(reason).toBe("testing revert");
    expect(revisions).toEqual(["263483"]);
});

test("remove double smartquotes in revert reason", () => {
    let message = `<@${WebKitBotID}> revert 263483 \u201Ctesting revert\u201D`;

    let text = extractTextIfMentioned(message, WebKitBotID);
    expect(text).toBe(" revert 263483 \"testing revert\"");

    let {command, args} = extractCommandAndArgs(text);
    expect(command).toBe("revert");
    expect(args).toEqual(["263483", "\"testing", "revert\""]);

    let {revisions, reason} = extractRevisionsAndReason(args);
    expect(reason).toBe("testing revert");
    expect(revisions).toEqual(["263483"]);
});

test("remove line terminators", () => {
    let message = `<@${WebKitBotID}> revert 263483 testing revert\nis this work?`;

    let text = extractTextIfMentioned(message, WebKitBotID);
    expect(text).toBe(" revert 263483 testing revert is this work?");

    let {command, args} = extractCommandAndArgs(text);
    expect(command).toBe("revert");
    expect(args).toEqual(["263483", "testing", "revert", "is", "this", "work?"]);

    let {revisions, reason} = extractRevisionsAndReason(args);
    expect(reason).toBe("testing revert is this work?");
    expect(revisions).toEqual(["263483"]);
});

test("multiple revisions", () => {
    let message = `<@${WebKitBotID}> revert 263483,263484,r263485,r263486: testing revert`;

    let text = extractTextIfMentioned(message, WebKitBotID);
    expect(text).toBe(" revert 263483,263484,r263485,r263486: testing revert");

    let {command, args} = extractCommandAndArgs(text);
    expect(command).toBe("revert");
    expect(args).toEqual(["263483,263484,r263485,r263486:", "testing", "revert"]);

    let {revisions, reason} = extractRevisionsAndReason(args);
    expect(reason).toBe("testing revert");
    expect(revisions).toEqual(["263483", "263484", "263485", "263486"]);
});

test("multiple revisions with spaces", () => {
    let message = `<@${WebKitBotID}> revert 263483, 263484, r263485, r263486: testing revert`;

    let text = extractTextIfMentioned(message, WebKitBotID);
    expect(text).toBe(" revert 263483, 263484, r263485, r263486: testing revert");

    let {command, args} = extractCommandAndArgs(text);
    expect(command).toBe("revert");
    expect(args).toEqual(["263483,", "263484,", "r263485,", "r263486:", "testing", "revert"]);

    let {revisions, reason} = extractRevisionsAndReason(args);
    expect(reason).toBe("testing revert");
    expect(revisions).toEqual(["263483", "263484", "263485", "263486"]);
});

test("mention in different place", () => {
    let message = `revert 263483, 263484, r263485, r263486: testing revert <@${WebKitBotID}>`;

    let text = extractTextIfMentioned(message, WebKitBotID);
    expect(text).toBe("revert 263483, 263484, r263485, r263486: testing revert ");

    let {command, args} = extractCommandAndArgs(text);
    expect(command).toBe("revert");
    expect(args).toEqual(["263483,", "263484,", "r263485,", "r263486:", "testing", "revert"]);

    let {revisions, reason} = extractRevisionsAndReason(args);
    expect(reason).toBe("testing revert");
    expect(revisions).toEqual(["263483", "263484", "263485", "263486"]);
});
