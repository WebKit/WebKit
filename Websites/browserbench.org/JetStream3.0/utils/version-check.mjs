// Copyright (C) 2007-2025 Apple Inc. All rights reserved.
// Copyright (C) 2025 Google LLC

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//  notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//  notice, this list of conditions and the following disclaimer in the
//  documentation and/or other materials provided with the distribution.

// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

import fs from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const dirname = path.dirname(fileURLToPath(import.meta.url));
const projectRoot = path.join(dirname, "..");
const packageJsonPath = path.join(projectRoot, "package.json");

const packageJsonContent = await fs.readFile(packageJsonPath, "utf8");
const pkg = JSON.parse(packageJsonContent);

const nodeEngine = pkg.engines?.node;
// Regex to parse simple ">= MAJOR[.MINOR[.PATCH]]"
const match = nodeEngine.match(/>=?\s*(\d+)(\.\d+)?(\.\d+)?/);
const requiredMajor = parseInt(match[1]);
const currentMajor = parseInt(process.versions.node.split(".")[0]);

if (currentMajor < requiredMajor) {
    console.error(
        `❌ Error: Node.js v${requiredMajor} or higher require. expected: "${nodeEngine}", current: ${process.version}).`
    );
    process.exit(1);
}
