#! /usr/bin/env node
/* eslint-disable-next-line  no-unused-vars */

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

import { serve, optionDefinitions as serverOptionDefinitions } from "./server.mjs";
import { Builder, Capabilities, logging } from "selenium-webdriver";
import { Options as ChromeOptions } from "selenium-webdriver/chrome.js";
import { Options as FirefoxOptions } from "selenium-webdriver/firefox.js";
import commandLineArgs from "command-line-args";
import { promises as fs } from "fs";
import path from "path";
import os from "os";

import {logInfo, logError, printHelp, runTest} from "./helper.mjs";

const TESTS = [
    {
        name: "Run Single Suite",
        tags: ["all", "main"],
        run() {
            return runEnd2EndTest("Run Single Suite", { test: "proxy-mobx" });
        }
    },
    {
        name: "Run Multiple Suites",
        tags: ["all", "main"],
        run() {
            return runEnd2EndTest("Run Multiple Suites", { test: "prismjs-startup-es6,postcss-wtb" });
        }
    },
    {
        name: "Run Tag No Prefetch",
        tags: ["all", "main"],
        run() {
            return runEnd2EndTest("Run Tag No Prefetch", { tag: "proxy", prefetchResources: "false" });
        }
    },
    {
        name: "Run Disabled Suite",
        tags: ["all", "disabled"],
        run() {
            return runEnd2EndTest("Run Disabled Suite", { tag: "disabled" });
        }
    },
    {
        name: "Run Default Suite",
        tags: ["all", "default"],
        run() {
            return runEnd2EndTest("Run Default Suite");
        }
    },
    {
        name: "Verify In Depth Info",
        tags: ["all", "in-depth"],
        run() {
            return runBrowserDriverTest("In Depth Page Check", inDepthPageTest);
        }
    }
];

const VALID_TAGS = Array.from(new Set(TESTS.map((each) => each.tags).flat()));

function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

const optionDefinitions = [
    ...serverOptionDefinitions,
    { name: "browser", type: String, description: "Set the browser to test, choices are [safari, firefox, chrome, edge]. By default the $BROWSER env variable is used." },
    { name: "help", alias: "h", description: "Print this help text." },
    { name: "suite", type: String, defaultOption: true, typeLabel: `{underline choices}: ${VALID_TAGS.join(", ")}`, description: "Run a specific suite by name." }
];

const options = commandLineArgs(optionDefinitions);

if ("help" in options)
    printHelp("". optionDefinitions);

if (options.suite && !VALID_TAGS.includes(options.suite))
    printHelp(`Invalid suite: ${options.suite}. Choices are: ${VALID_TAGS.join(", ")}`);

const BROWSER = options?.browser;
if (!BROWSER)
    printHelp("No browser specified, use $BROWSER or --browser", optionDefinitions);
const IS_HEADLESS = os.platform() === "linux" && !process.env.DISPLAY;

let capabilities;
let browserOptions;
switch (BROWSER) {
    case "safari":
        capabilities = Capabilities.safari();
        capabilities.set("safari:diagnose", true);
        break;

    case "firefox": {
        capabilities = Capabilities.firefox()
        if (IS_HEADLESS) {
            browserOptions = new FirefoxOptions();
            browserOptions.addArguments("-headless"); 
        }
        break;
    }
    case "chrome": {
        capabilities = Capabilities.chrome()
        if (IS_HEADLESS) {
            browserOptions = new ChromeOptions();
            browserOptions.addArguments("--headless"); 
        }
        break;
    }
    case "edge": {
        capabilities = Capabilities.edge();
        break;
    }
    default: {
        printHelp(`Invalid browser "${BROWSER}", choices are: "safari", "firefox", "chrome", "edge"`, optionDefinitions);
    }
}

process.on("unhandledRejection", (err) => {
    logError(err);
    process.exit(1);
});
process.once("uncaughtException", (err) => {
    logError(err);
    process.exit(1);
});

const PORT = options.port;
const server = await serve(options);

async function runTests() {
    let success = true;
    const suiteFilter = options.suite || "all";

    const testsToRun = TESTS.filter(test => test.tags.includes(suiteFilter));

    if (testsToRun.length === 0) {
        console.error(`No suite found for filter: ${suiteFilter}`);
        process.exit(1);
    }

    try {
        for (const test of testsToRun) {
            success &&= await test.run();
        }
    } finally {
        server.close();
    }
    if (!success)
      process.exit(1);
}

async function runBrowserDriverTest(name, body) {
    return runTest(name, () => runBrowserDriver(body))
}

async function runBrowserDriver(body) {
    const builder = new Builder().withCapabilities(capabilities);
    if (browserOptions) {
        switch(BROWSER) {
            case "firefox":
                builder.setFirefoxOptions(browserOptions);
                break;
            case "chrome":
                builder.setChromeOptions(browserOptions);
                break;
            default:
                break;
        }
    }
    const driver = await builder.build();
    const sessionId = (await driver.getSession()).getId();
    const driverCapabilities = await driver.getCapabilities();
    logInfo(`Browser: ${driverCapabilities.getBrowserName()} ${driverCapabilities.getBrowserVersion()}`);
    let success = true;
    try {
        await body(driver);
    } catch(e) {
        success = false;
        throw e;
    } finally {
        await driver.quit();
        if (BROWSER === "safari")
            await sleep(1000);
        if (!success) {
            await printLogs(sessionId);
        }
    }
}

async function runEnd2EndTest(name, params) {
    return runBrowserDriverTest(name, (driver) => testEnd2End(driver, params));
}

async function testEnd2End(driver, params) {
    const urlParams = Object.assign({
            worstCaseCount: 2,
            iterationCount: 3 
        }, params);
    let results;
    const url = new URL(`http://localhost:${PORT}/index.html`);
    url.search = new URLSearchParams(urlParams).toString();
    logInfo(`JetStream PREPARE ${url}`);
    await driver.get(url.toString());
    await driver.executeAsyncScript((callback) => {
        // callback() is explicitly called without the default event
        // as argument to avoid serialization issues with chromedriver.
        globalThis.addEventListener("JetStreamReady", () => callback());
        // We might not get a chance to install the on-ready listener, thus
        // we also check if the runner is ready synchronously.
        if (globalThis?.JetStream?.isReady)
            callback();
    });
    results = await benchmarkResults(driver);
    // FIXME: validate results;
}

async function benchmarkResults(driver) {
    logInfo("JetStream START");
    await driver.manage().setTimeouts({ script: 2 * 60_000 });
    await driver.executeAsyncScript((callback) => {
        globalThis.JetStream.start();
        callback();
    });

    await new Promise((resolve, reject) => pollResultsUntilDone(driver, resolve, reject));
    const resultsJSON = await driver.executeScript(() => {
        return globalThis.JetStream.resultsJSON();
    });
    return JSON.parse(resultsJSON);
}

async function inDepthPageTest(driver) {
    await driver.get(`http://localhost:${PORT}/in-depth.html`);
    const descriptions = await driver.executeScript(() => {
        return Array.from(document.querySelectorAll("#workload-details dt[id]")).map(each => {
            return [each.id, { text: each.textContent, cssClass: each.className }];
        });
    }).then(entries => new Map(entries));

    const sectionErrors = []

    for (const [id, description] of descriptions) {
        if (id !== description.text) {
            sectionErrors.push(
                `Expected dt with id '${id}' to have text content '${id}' but got '${description.text}'`);
        }
    }

    const ids = Array.from(descriptions.keys());
    const sortedIds = ids.slice().sort((a, b) => {
        return a.toLowerCase().localeCompare(b.toLowerCase());
    });
    sortedIds.forEach((id, index) => {
        if (id !== ids[index]) {
            sectionErrors.push(
                `Expected index ${index} to be '${id}' but got '${ids[index]}' `);
        }
    });

    await driver.get(`http://localhost:${PORT}/index.html?tags=all`);
    const benchmarkData = await driver.executeScript(() => {
        return globalThis.JetStream.benchmarks.map(each => [each.name, Array.from(each.tags)]);
    }).then(entries => new Map(entries));

    const benchmarkNames = Array.from(benchmarkData.keys());
    benchmarkNames.sort((a,b) => {
        return a.toLowerCase().localeCompare(b.toLowerCase());
    });

    const missingIds = benchmarkNames.filter(name => !descriptions.has(name));
    if (missingIds.length > 0) {
        sectionErrors.push(`Missing in-depth.html info section: ${JSON.stringify(missingIds, undefined, 2)}`);
    }

    const unusedIds = sortedIds.filter(id => !benchmarkData.has(id)); 
    if (unusedIds.length > 0) {
        sectionErrors.push(`Unused in-depth.html info section: ${JSON.stringify(unusedIds, undefined, 2)}`);
    }

    if (sectionErrors.length > 0) {
        throw new Error(`info section errors: ${sectionErrors.join("\n")}`);
    }
}

class JetStreamTestError extends Error {
    constructor(errors) {
        super(`Tests failed: ${errors.map(e => e.stack).join(", ")}`);
        this.errors = errors;
    }
}

const UPDATE_INTERVAL = 250;
async function pollResultsUntilDone(driver, resolve, reject) {
    const previousResults = new Set();
    const intervalId = setInterval(async function logResult() {
        const {done, errors, resultsJSON} = await driver.executeScript(() => {
            return {
                done: globalThis.JetStream.isDone,
                errors: globalThis.JetStream.errors,
                resultsJSON: JSON.stringify(globalThis.JetStream.resultsObject("simple")),
            };
        });
        if (errors.length) {
            clearInterval(intervalId);
            reject(new JetStreamTestError(errors));
        }
        logIncrementalResult(previousResults, JSON.parse(resultsJSON));
        if (done) {
            clearInterval(intervalId);
            resolve();
        }
    }, UPDATE_INTERVAL)
}

function logIncrementalResult(previousResults, benchmarkResults) {
    for (const [testName, testResults] of Object.entries(benchmarkResults)) {
        if (previousResults.has(testName))
            continue;
        console.log(testName, testResults);
        previousResults.add(testName);
    }
}

function printLogs(sessionId) {
    if (BROWSER === "safari" && sessionId)
        return printSafariLogs(sessionId);
}

async function printSafariLogs(sessionId) {
    const sessionLogDir = path.join(os.homedir(), "Library", "Logs", "com.apple.WebDriver", sessionId);
    try {
        const files = await fs.readdir(sessionLogDir);
        const logFiles = files.filter(f => f.startsWith("safaridriver.") && f.endsWith(".txt"));
        if (logFiles.length === 0) {
            logInfo(`No safaridriver log files found in session directory: ${sessionLogDir}`);
            return;
        }
        for (const file of logFiles) {
            const logPath = path.join(sessionLogDir, file);
            const logContent = await fs.readFile(logPath, "utf8");
            logGroup(`SafariDriver Log: ${file}`, () => console.log(logContent));
        }
    } catch (err) {
        logError("Error reading SafariDriver logs:", err);
    }
}

setImmediate(runTests);
