/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 * Copyright 2026 Google LLC
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

import { glob } from "glob";
import * as fs from "fs";
import * as path from "path";
import { logInfo, logError, runTest } from "./helper.mjs";

const IGNORE_PATTERNS = [
    "**/node_modules/**",
    "**/.*/**",
    "**/build/**",
    "**/dotnet/build-*/**",
    // Existing benchmarks with common / non-standard license headers:
    "**/8bitbench/**",
    "**/ARES-6/Basic/test.js",
    "**/RexBench/OfflineAssembler/expected.js",
    "**/SunSpider/**",
    "**/class-fields/**",
    "**/wasm/tfjs-*",
    "**/wasm/tfjs.js",
    "**/wasm/tsf.js",
    "**/web-tooling-benchmark/third_party/**",
    "**/worker/bomb-subtests/**",
];

async function checkLicenses() {
    const files = await glob("**/*.{js,mjs}", { ignore: IGNORE_PATTERNS, nodir: true });

    logInfo(`Checking ${files.length} files for license headers...`);

    const offendingFiles = [];
    const dirHasLicense = new Map();

    for (const file of files) {
        const fileInfo = path.parse(file);
        const dir = fileInfo.dir;
        let hasLicenseFile = dirHasLicense.get(dir);
        if (hasLicenseFile === undefined) {
            hasLicenseFile =
                fs.existsSync(path.join(dir, "LICENSE")) ||
                fs.existsSync(path.join(dir, "LICENSE.txt"));
            dirHasLicense.set(dir, hasLicenseFile);
        }
        if (hasLicenseFile === true) {
            continue;
        }

        const content = fs.readFileSync(file, "utf8");
        const hasCopyright = /Copyright/i.test(content);
        const hasLicenseText = /Redistribution|Permission|License/i.test(content);

        if (hasCopyright && hasLicenseText) {
            continue;
        }
        const perFileLicense = path.join(dir, `${fileInfo.name}.LICENSE.txt`);
        if (fs.existsSync(perFileLicense)) {
            continue;
        }

        offendingFiles.push(file);
    }

    if (offendingFiles.length) {
        logError(`The following ${offendingFiles.length} file[s] have missing license headers:`);
        for (const file of offendingFiles) {
            logError(`  - ${file}`);
        }
        throw new Error("Missing license headers");
    }
}

const success = await runTest("Check Licenses", checkLicenses);
process.exit(success ? 0 : 1);
