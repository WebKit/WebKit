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

import {readFile, readFileSync} from "fs";
import path from "path";
import util from "util";
import Commit from "../src/Commit.mjs";
import Contributors from "../src/Contributors.mjs";
import {rootDirectoryOfWebKit} from "../src/Utility.mjs";

const localContributorsPath = path.resolve(rootDirectoryOfWebKit(), "Tools", "Scripts", "webkitpy", "common", "config", "contributors.json");
const contributors = new Contributors(JSON.parse(readFileSync(localContributorsPath)));
const readFileAsync = util.promisify(readFile);

test("Commit can parse one having radar and buzilla", async () => {
    let feedItem = JSON.parse(await readFileAsync(path.resolve("tests", "resources", "HaveRadarAndBugzilla.json")));
    let commit = new Commit(feedItem, contributors);
    expect(commit.revision).toBe(263476);
    expect(commit.patchBy).toBe(null);
    expect(commit.revert).toBe(null);
    expect(commit.bugzilla).toBe(212392);
    expect(commit.radar).toBe(61799040);
    expect(commit.email).toBe("ddkilzer@apple.com");
    expect(commit.title).toBe("Use ObjectIdentifier<> instead of uint64_t for context IDs in VideoFullscreenManagerProxy");
    expect(commit.author).toBe("David Kilzer (@ddkilzer)");
    expect(commit.url).toBe("https://trac.webkit.org/r263476");
    expect(commit.message()).toBe(`Use ObjectIdentifier&lt;&gt; instead of uint64_t for context IDs in VideoFullscreenManagerProxy
https://trac.webkit.org/r263476 by David Kilzer (@ddkilzer)
https://webkit.org/b/212392 <rdar://problem/61799040>`);
});

test("Commit can parse one not having radar and buzilla", async () => {
    let feedItem = JSON.parse(await readFileAsync(path.resolve("tests", "resources", "NoRadarAndBugzilla.json")));
    let commit = new Commit(feedItem, contributors);
    expect(commit.revision).toBe(263465);
    expect(commit.patchBy).toBe(null);
    expect(commit.revert).toBe(null);
    expect(commit.bugzilla).toBe(null);
    expect(commit.radar).toBe(null);
    expect(commit.email).toBe("philn@webkit.org");
    expect(commit.title).toBe("Unreviewed GTK gardening");
    expect(commit.author).toBe("Philippe Normand (@philn)");
    expect(commit.url).toBe("https://trac.webkit.org/r263465");
    expect(commit.message()).toBe(`Unreviewed GTK gardening
https://trac.webkit.org/r263465 by Philippe Normand (@philn)`);
});

test("Commit can parse one having buzilla", async () => {
    let feedItem = JSON.parse(await readFileAsync(path.resolve("tests", "resources", "HavingBugzilla.json")));
    let commit = new Commit(feedItem, contributors);
    expect(commit.revision).toBe(263470);
    expect(commit.patchBy).toBe(null);
    expect(commit.revert).toBe(null);
    expect(commit.bugzilla).toBe(213191);
    expect(commit.radar).toBe(null);
    expect(commit.email).toBe("shvaikalesh@gmail.com");
    expect(commit.title).toBe("Add DFG/FTL fast path for GetPrototypeOf based on OverridesGetPrototype flag");
    expect(commit.author).toBe("Alexey Shvayka (@shvaikalesh)");
    expect(commit.url).toBe("https://trac.webkit.org/r263470");
    expect(commit.message()).toBe(`Add DFG/FTL fast path for GetPrototypeOf based on OverridesGetPrototype flag
https://trac.webkit.org/r263470 by Alexey Shvayka (@shvaikalesh)
https://webkit.org/b/213191`);
});
