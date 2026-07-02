/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 * Copyright 2025 Google LLC
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

import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";
import { runTest } from "./src/test.mjs";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const samples = [
  { file: "data/sample.html", lang: "markup" },
  { file: "data/sample.js", lang: "javascript" },
  { file: "data/sample.css", lang: "css" },
  { file: "data/sample.cpp", lang: "cpp" },
  { file: "data/sample.md", lang: "markdown" },
  { file: "data/sample.json", lang: "json" },
  { file: "data/sample.sql", lang: "sql" },
  { file: "data/sample.py", lang: "python" },
  { file: "data/sample.ts", lang: "typescript" },
];

const samplesWithContent = samples.map((sample) => {
  const content = fs.readFileSync(path.join(__dirname, sample.file), "utf8");
  return { ...sample, content };
});

const startTime = process.hrtime.bigint();
const results = runTest(samplesWithContent);
const endTime = process.hrtime.bigint();

const duration = Number(endTime - startTime) / 1e6; // milliseconds

for (const result of results) {
  console.log(`Output size: ${result.html.length} characters`);
}

console.log(
  `\nTotal highlighting time for all files: ${duration.toFixed(2)}ms`
);
