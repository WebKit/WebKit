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

import {buildGitWebkitRevertCommand, extractRevisionsAndReason, extractTextIfMentioned,
    extractCommandAndArgs, parseBugId, parsePRUrl, parsePullRequestAction} from "../src/CommandParser.mjs";
import {buildRevertSuccessMessage} from "../src/Utility.mjs";
const WebKitBotID = 42;

test("buildGitWebkitRevertCommand skips style checks with a reason", () => {
    let args = buildGitWebkitRevertCommand("git-webkit", ["263483"], "testing revert", null);
    expect(args).toEqual(["git-webkit", "revert", "263483", "--pr", "--defaults", "--no-checks", "--reason", "testing revert"]);
});

test("buildGitWebkitRevertCommand skips style checks with an issue URL", () => {
    let args = buildGitWebkitRevertCommand("git-webkit", ["263483", "263484"], "unused reason", "https://bugs.webkit.org/show_bug.cgi?id=1");
    expect(args).toEqual(["git-webkit", "revert", "263483", "263484", "--pr", "--defaults", "--no-checks", "--issue", "https://bugs.webkit.org/show_bug.cgi?id=1"]);
});

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

test("identifier linkified by Slack as an email address", () => {
    let message = `<@${WebKitBotID}> revert <mailto:319186@main|319186@main> Causes MotionMark regression`;

    let text = extractTextIfMentioned(message, WebKitBotID);
    expect(text).toBe(" revert 319186@main Causes MotionMark regression");

    let {command, args} = extractCommandAndArgs(text);
    expect(command).toBe("revert");
    expect(args).toEqual(["319186@main", "Causes", "MotionMark", "regression"]);

    let {revisions, reason} = extractRevisionsAndReason(args);
    expect(reason).toBe("Causes MotionMark regression");
    expect(revisions).toEqual(["319186@main"]);
});

test("multiple identifiers linkified by Slack as email addresses", () => {
    let message = `<@${WebKitBotID}> revert <mailto:319186@main|319186@main>, <mailto:319187@main|319187@main>: testing revert`;

    let text = extractTextIfMentioned(message, WebKitBotID);
    expect(text).toBe(" revert 319186@main, 319187@main: testing revert");

    let {command, args} = extractCommandAndArgs(text);
    expect(command).toBe("revert");
    expect(args).toEqual(["319186@main,", "319187@main:", "testing", "revert"]);

    let {revisions, reason} = extractRevisionsAndReason(args);
    expect(reason).toBe("testing revert");
    expect(revisions).toEqual(["319186@main", "319187@main"]);
});

test("identifier pasted as a commits.webkit.org link", () => {
    let message = `<@${WebKitBotID}> revert <https://commits.webkit.org/319186@main> testing revert`;

    let text = extractTextIfMentioned(message, WebKitBotID);
    expect(text).toBe(" revert https://commits.webkit.org/319186@main testing revert");

    let {command, args} = extractCommandAndArgs(text);
    expect(command).toBe("revert");

    let {revisions, reason} = extractRevisionsAndReason(args);
    expect(reason).toBe("testing revert");
    expect(revisions).toEqual(["319186@main"]);
});

test("revision pasted as a labelled commits.webkit.org link", () => {
    let message = `<@${WebKitBotID}> revert <https://commits.webkit.org/r263483|r263483> testing revert`;

    let text = extractTextIfMentioned(message, WebKitBotID);
    expect(text).toBe(" revert https://commits.webkit.org/r263483 testing revert");

    let {revisions, reason} = extractRevisionsAndReason(extractCommandAndArgs(text).args);
    expect(reason).toBe("testing revert");
    expect(revisions).toEqual(["263483"]);
});

test("user mentions in the reason are left intact", () => {
    let message = `<@${WebKitBotID}> revert 263483 breaks the build, see <@U12345678>`;

    let text = extractTextIfMentioned(message, WebKitBotID);
    expect(text).toBe(" revert 263483 breaks the build, see <@U12345678>");

    let {revisions, reason} = extractRevisionsAndReason(extractCommandAndArgs(text).args);
    expect(reason).toBe("breaks the build, see <@U12345678>");
    expect(revisions).toEqual(["263483"]);
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

test("identifier followed by trailing sentence punctuation", () => {
    let message = `<@${WebKitBotID}> revert 319904@main. Broke Simulator and Catalyst builds.`;

    let text = extractTextIfMentioned(message, WebKitBotID);
    let {command, args} = extractCommandAndArgs(text);
    expect(command).toBe("revert");

    let {revisions, reason} = extractRevisionsAndReason(args);
    expect(revisions).toEqual(["319904@main"]);
    expect(reason).toBe("Broke Simulator and Catalyst builds.");
});

test("identifier without trailing punctuation still works", () => {
    let message = `<@${WebKitBotID}> revert 319904@main reason`;

    let text = extractTextIfMentioned(message, WebKitBotID);
    let {command, args} = extractCommandAndArgs(text);
    expect(command).toBe("revert");

    let {revisions} = extractRevisionsAndReason(args);
    expect(revisions).toEqual(["319904@main"]);
});

test("branch name containing periods is preserved", () => {
    let message = `<@${WebKitBotID}> revert 319904@safari-7620.1.16-branch reason`;

    let text = extractTextIfMentioned(message, WebKitBotID);
    let {command, args} = extractCommandAndArgs(text);
    expect(command).toBe("revert");

    let {revisions} = extractRevisionsAndReason(args);
    expect(revisions).toEqual(["319904@safari-7620.1.16-branch"]);
});

test("bare svn revision with trailing period", () => {
    let message = `<@${WebKitBotID}> revert 263483. reason`;

    let text = extractTextIfMentioned(message, WebKitBotID);
    let {command, args} = extractCommandAndArgs(text);
    expect(command).toBe("revert");

    let {revisions} = extractRevisionsAndReason(args);
    expect(revisions).toEqual(["263483"]);
});

test("identifier wrapped in parentheses", () => {
    let message = `<@${WebKitBotID}> revert (319904@main) reason`;

    let text = extractTextIfMentioned(message, WebKitBotID);
    let {command, args} = extractCommandAndArgs(text);
    expect(command).toBe("revert");

    let {revisions} = extractRevisionsAndReason(args);
    expect(revisions).toEqual(["319904@main"]);
});

test("identifier wrapped in parentheses followed by a period", () => {
    let message = `<@${WebKitBotID}> revert (319904@main). Broke the build.`;

    let text = extractTextIfMentioned(message, WebKitBotID);
    let {command, args} = extractCommandAndArgs(text);
    expect(command).toBe("revert");

    let {revisions, reason} = extractRevisionsAndReason(args);
    expect(revisions).toEqual(["319904@main"]);
    expect(reason).toBe("Broke the build.");
});

const createdTranscript = `Cloning 'origin/main' to 'eng/revert-5-main-reason-for-revert'
Reverting 5@main...
Created the local development branch 'eng/revert-5-main-reason-for-revert'
Using committed changes...
    Found 1 commit...
Creating pull-request for 'eng/revert-5-main-reason-for-revert'...
Created 'PR 1 | Unreviewed, reverting 5@main (d8bce26fa65c)'!
https://github.com/WebKit/WebKit/pull/1
`;

const updatedTranscript = `Cloning 'origin/main' to 'eng/revert-5-main-reason-for-revert'
Reverting 5@main...
Using committed changes...
    Found 1 commit...
Updating pull-request for 'eng/revert-5-main-reason-for-revert'...
Updated 'PR 1 | Unreviewed, reverting 5@main (d8bce26fa65c)'!
https://github.com/WebKit/WebKit/pull/1
`;

// git-webkit printed a URL but no line we recognize as an action. buildRevertSuccessMessage
// turns this into "Posted revert PR" rather than claiming a creation.
const noActionTranscript = `Reverting 5@main...
Using committed changes...
https://github.com/WebKit/WebKit/pull/1
`;

test("parsePullRequestAction detects a created pull request", () => {
    expect(parsePullRequestAction(createdTranscript)).toBe("created");
    expect(parsePRUrl(createdTranscript)).toBe("https://github.com/WebKit/WebKit/pull/1");
});

test("parsePullRequestAction detects an updated pull request", () => {
    expect(parsePullRequestAction(updatedTranscript)).toBe("updated");
    expect(parsePRUrl(updatedTranscript)).toBe("https://github.com/WebKit/WebKit/pull/1");
});

test("parsePullRequestAction returns null when there is no action line", () => {
    expect(parsePullRequestAction(noActionTranscript)).toBe(null);
    expect(parsePRUrl(noActionTranscript)).toBe("https://github.com/WebKit/WebKit/pull/1");
});

test("parsePullRequestAction on empty output", () => {
    expect(parsePullRequestAction("")).toBe(null);
    expect(parsePullRequestAction(null)).toBe(null);
    expect(parsePullRequestAction("Created 'PR 1'!\n")).toBe(null);
    expect(parsePullRequestAction("Updated something that is not a pull request\n")).toBe(null);
});

test("buildRevertSuccessMessage reports a created pull request", () => {
    let message = buildRevertSuccessMessage(WebKitBotID, {prUrl: "https://github.com/WebKit/WebKit/pull/1", action: "created"});
    expect(message).toBe(`<@${WebKitBotID}> Created revert PR: https://github.com/WebKit/WebKit/pull/1`);
});

test("buildRevertSuccessMessage does not claim a creation when the pull request was updated", () => {
    let message = buildRevertSuccessMessage(WebKitBotID, {prUrl: "https://github.com/WebKit/WebKit/pull/1", action: "updated"});
    expect(message).toBe(`<@${WebKitBotID}> A revert PR for this already existed, so I updated it instead of creating a new one: https://github.com/WebKit/WebKit/pull/1`);
});

test("buildRevertSuccessMessage does not claim a creation when the action is unknown", () => {
    let message = buildRevertSuccessMessage(WebKitBotID, {prUrl: "https://github.com/WebKit/WebKit/pull/1", action: null});
    expect(message).toBe(`<@${WebKitBotID}> Posted revert PR: https://github.com/WebKit/WebKit/pull/1`);
});

test("buildRevertSuccessMessage reports a revert patch", () => {
    let message = buildRevertSuccessMessage(WebKitBotID, {bugId: "321686"});
    expect(message).toBe(`<@${WebKitBotID}> Created a revert patch https://webkit.org/b/321686`);
});

test("parseBugId accepts bug URLs", () => {
    expect(parseBugId("https://webkit.org/b/321569")).toBe("321569");
    expect(parseBugId("https://bugs.webkit.org/show_bug.cgi?id=321569")).toBe("321569");
});

test("parseBugId accepts a bare bug number", () => {
    expect(parseBugId("321569")).toBe("321569");
    expect(parseBugId(" 321569 ")).toBe("321569");
});

test("parseBugId rejects numbers that are not bug numbers", () => {
    expect(parseBugId("")).toBe(null);
    expect(parseBugId("32156")).toBe(null);
    expect(parseBugId("3215690")).toBe(null);
    expect(parseBugId("321569 causes a crash")).toBe(null);
    // A bare number on its own line of tool output is not a bug number.
    expect(parseBugId("Reverting 5@main...\n321569\nDone\n")).toBe(null);
});

test("a trailing bare bug number is not a revision", () => {
    let message = `<@${WebKitBotID}> revert 318045@main 321569`;

    let text = extractTextIfMentioned(message, WebKitBotID);
    let {command, args} = extractCommandAndArgs(text);
    expect(command).toBe("revert");

    let {revisions, reason} = extractRevisionsAndReason(args);
    expect(revisions).toEqual(["318045@main"]);
    expect(reason).toBe("321569");
    expect(parseBugId(reason)).toBe("321569");
});

test("a trailing bare bug number after a hash revision is not a revision", () => {
    let {revisions, reason} = extractRevisionsAndReason(["d8bce26fa65c", "321569"]);
    expect(revisions).toEqual(["d8bce26fa65c"]);
    expect(reason).toBe("321569");
});

test("bare svn revisions are still parsed as revisions", () => {
    // `263483 263484` is ambiguous, so it keeps its historical meaning: two svn revisions.
    expect(extractRevisionsAndReason(["263483", "263484"])).toEqual({revisions: ["263483", "263484"], reason: ""});
    expect(extractRevisionsAndReason(["263483", "263484", "testing", "revert"])).toEqual({revisions: ["263483", "263484"], reason: "testing revert"});
    expect(extractRevisionsAndReason(["r263483", "263484", "testing"])).toEqual({revisions: ["263483", "263484"], reason: "testing"});
    expect(extractRevisionsAndReason(["96324"])).toEqual({revisions: ["96324"], reason: ""});
    expect(extractRevisionsAndReason(["96324", "testing"])).toEqual({revisions: ["96324"], reason: "testing"});
});

test("a comma separated revision list is never a bug number", () => {
    expect(extractRevisionsAndReason(["318045@main,321569"])).toEqual({revisions: ["318045@main", "321569"], reason: ""});
});

test("a bare number followed by a reason is still a revision", () => {
    // Ambiguous, so leave today's behavior alone rather than guess.
    let {revisions, reason} = extractRevisionsAndReason(["318045@main", "321569", "causes", "a", "crash"]);
    expect(revisions).toEqual(["318045@main", "321569"]);
    expect(reason).toBe("causes a crash");
});

test("a bug URL in the reason position still becomes the issue", () => {
    let {revisions, reason} = extractRevisionsAndReason(["318045@main", "https://bugs.webkit.org/show_bug.cgi?id=321569"]);
    expect(revisions).toEqual(["318045@main"]);
    expect(parseBugId(reason)).toBe("321569");
});

test("a punctuated bare revision does not turn the next revision into a bug number", () => {
    let args = extractCommandAndArgs("revert 263483. 263484").args;
    let {revisions, reason} = extractRevisionsAndReason(args);
    expect(revisions).toEqual(["263483", "263484"]);
    expect(reason).toBe("");
});

test("a trailing bug number is recognized through sentence punctuation", () => {
    let args = extractCommandAndArgs("revert 318045@main 321569.").args;
    let {revisions, reason} = extractRevisionsAndReason(args);
    expect(revisions).toEqual(["318045@main"]);
    expect(parseBugId(reason)).toBe("321569");
});

test("a trailing bug number is recognized when wrapped in parentheses", () => {
    let args = extractCommandAndArgs("revert 318045@main (321569)").args;
    let {revisions, reason} = extractRevisionsAndReason(args);
    expect(revisions).toEqual(["318045@main"]);
    expect(parseBugId(reason)).toBe("321569");
});
