#! /usr/bin/env node

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

import commandLineArgs from "command-line-args";
import fs from "fs";
import { fileURLToPath } from "url";
import path from "path";

import { logError, logCommand, printHelp, runTest, sh, logInfo } from "./helper.mjs";

const FILE_PATH = fileURLToPath(import.meta.url);
const SRC_DIR = path.dirname(path.dirname(FILE_PATH));

const optionDefinitions = [
  { name: "help", alias: "h", description: "Print this help text." },
  { name: "diff", type: String, description: "A git commit range to determine which builds to run (e.g. main...HEAD)." },
];

const options = commandLineArgs(optionDefinitions);

if ("help" in options)
  printHelp(optionDefinitions);


if (options.diff) {
    const { stdoutString } = await sh("git", "diff", "--name-only", options.diff);
    const changedDirs = new Set();
    const changedFiles = stdoutString.trim().split("\n");
    for (const file of changedFiles) {
        let currentDir = path.dirname(file)
        while (currentDir !== ".") {
            changedDirs.add(path.join(SRC_DIR, currentDir));
            currentDir = path.dirname(currentDir);
        }
    }
    options.changedDirs = changedDirs;
} else {
    options.changedDirs = undefined;
}

async function findPackageJsonFiles(dir, accumulator=[]) {
    const dirEntries = fs.readdirSync(dir, { withFileTypes: true });
    for (const dirent of dirEntries) {
        if (dirent.name === "node_modules" || dirent.name === ".git")
            continue;
        const fullPath = path.join(dir, dirent.name);
        if (dirent.isDirectory()) {
            // Ignore third-party git dirs.
            if (fs.existsSync(path.join(fullPath, ".git"))) {
                continue;
            }
            findPackageJsonFiles(fullPath, accumulator);
        } else if (dirent.name === "package.json") {
            accumulator.push(fullPath)
        }
    }
    return accumulator;
}

async function runBuilds() {
    const packageJsonFiles = await findPackageJsonFiles(SRC_DIR);
    let success = true;

    logInfo(`Found ${packageJsonFiles.length} package.json files`);
    console.log(packageJsonFiles)
    let filteredPackageJsonFiles = packageJsonFiles;
    if (options.changedDirs?.size === 0) {
        logInfo("No file changes detected, skipping all");
    }
    if (options.changedDirs?.size > 0) {
        filteredPackageJsonFiles = packageJsonFiles.filter(file => {
            const dir = path.dirname(file);
            return options.changedDirs.has(dir);
        });
        logInfo(`Found ${filteredPackageJsonFiles.length} modified package.json files to build`);
    }

    for (const file of filteredPackageJsonFiles) {
        const content = fs.readFileSync(file, "utf-8");
        const packageJson = JSON.parse(content);
        if (!packageJson.scripts?.build) {
            continue;
        }

        const dir = path.dirname(file);
        const relativeDir = path.relative(SRC_DIR, dir);
        const testName = `Building ./${relativeDir}:`;
        
        const buildTask = async () => {
            const oldCWD = process.cwd();
            try {
                logCommand("cd", dir);
                process.chdir(dir);
                await sh("npm", "ci");
                await sh("npm", "run", "build");
            } finally {
                process.chdir(oldCWD);
                await sh("git", "reset", "--hard");
            }
        };
        
        success &&= await runTest(testName, buildTask);
    }

    if (!success) {
        logError("One or more builds failed.");
        process.exit(1);
    }
}

setImmediate(runBuilds);
