/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

load("./utils/shell-config.js");

const CLI_PARAMS = {
  __proto__: null,
  help: { help: "Print this help message.", param: "help" },
  "iteration-count": {
    help: "Set the default iteration count.",
    param: "testIterationCount",
  },
  "worst-case-count": {
    help: "Set the default worst-case count.",
    param: "testWorstCaseCount",
  },
  "dump-json-results": {
    help: "Print summary json to the console.",
    param: "dumpJSONResults",
  },
  "dump-test-list": {
    help: "Print the selected test list instead of running.",
    param: "dumpTestList",
  },
  ramification: {
    help: "Enable ramification support. See RAMification.py for more details.",
    param: "RAMification",
  },
  "no-prefetch": {
    help: "Do not prefetch resources. Will add network overhead to measurements!",
    param: "prefetchResources",
  },
  "group-details": {
    help: "Display detailed group items",
    param: "groupDetails",
  },
  test: {
    help: "Run a specific test or comma-separated list of tests.",
    param: "test",
  },
  tag: {
    help: "Run tests with a specific tag or comma-separated list of tags.",
    param: "tag",
  },
  "start-automatically": {
    help: "Start the benchmark automatically.",
    param: "startAutomatically",
  },
  report: { help: "Report results to a server.", param: "report" },
  "start-delay": {
    help: "Delay before starting the benchmark.",
    param: "startDelay",
  },
  "custom-pre-iteration-code": {
    help: "Custom code to run before each iteration.",
    param: "customPreIterationCode",
  },
  "custom-post-iteration-code": {
    help: "Custom code to run after each iteration.",
    param: "customPostIterationCode",
  },
  "force-gc": {
    help: "Force garbage collection before each benchmark, requires engine support.",
    param: "forceGC",
  },
};

const cliParams = new Map();
const cliArgs = [];

if (globalThis.arguments?.length) {
  for (const argument of globalThis.arguments) {
    if (argument.startsWith("--")) {
      parseCliFlag(argument);
    } else {
      cliArgs.push(argument);
    }
  }
}

function parseCliFlag(argument) {
    const parts = argument.slice(2).split("=");
    const flagName = parts[0];
    if (!(flagName in CLI_PARAMS)) {
        const message =`Unknown flag: '--${flagName}'`;
        help(message);
        throw Error(message);
    }
    let value = parts.slice(1).join("=");
    if (flagName === "no-prefetch") value = "false";
    else if (value === "") value = "true";
    cliParams.set(CLI_PARAMS[flagName].param, value);
}


if (cliArgs.length) {
    let tests = cliParams.has("test") ? cliParams.get("tests").split(",") : []
    tests = tests.concat(cliArgs);
    cliParams.set("test", tests.join(","));
}

if (cliParams.size) 
    globalThis.JetStreamParamsSource = cliParams;

load("./utils/params.js");


async function runJetStream() {
    try {
        await JetStream.initialize();
        await JetStream.start();
    } catch (e) {
        console.error("JetStream3 failed: " + e);
        console.error(e.stack);
        throw e;
    }
}

load("./JetStreamDriver.js");

function help(message=undefined) {
    if (message)
        console.log(message);
    else
        console.log("JetStream Driver Help");
    console.log("");

    console.log("Options:");
    for (const [flag, { help }] of Object.entries(CLI_PARAMS))
        console.log(`    --${flag.padEnd(20)} ${help}`);
    console.log("");

    // If we had an early bailout during flag parsing we won't have 
    // JetStreamDriver.js loaded yet and thus none of the helper globals here
    // have been defined yet.
    if (typeof benchmarksByTag !== "undefined") {
        console.log("Available tags:");
        const tagNames = Array.from(benchmarksByTag.keys()).sort();
        for (const tagName of tagNames) console.log("  ", tagName);
        console.log("");
    }

    if (typeof BENCHMARKS !== "undefined") {
        console.log("Available tests:");
        const benchmarkNames = BENCHMARKS.map((b) => b.name).sort();
        for (const benchmark of benchmarkNames) console.log("  ", benchmark);
    }
}

if (cliParams.has("help")) {
    help();
} else if (cliParams.has("dumpTestList")) {
  JetStream.dumpTestList();
} else {
  runJetStream();
}
