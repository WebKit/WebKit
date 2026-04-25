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

import core from "@actions/core";
import { spawn } from "child_process";
import commandLineUsage from "command-line-usage";
import { styleText } from "node:util";

export const GITHUB_ACTIONS_OUTPUT = ("GITHUB_ACTIONS_OUTPUT" in process.env) || ("GITHUB_EVENT_PATH" in process.env);

export function logInfo(...args) {
  const text = args.join(" ")
  if (GITHUB_ACTIONS_OUTPUT)
    core.info(styleText("yellow", text));
  else
    console.log(styleText("yellow", text));
}

export function logError(...args) {
  let error;
  if (args.length == 1 && args[0] instanceof Error)
    error = args[0];
  const text = args.join(" ");
  if (GITHUB_ACTIONS_OUTPUT) {
    if (error?.stack)
      core.error(error.stack);
    else
      core.error(styleText("red", text));
  } else {
    if (error?.stack)
      console.error(styleText("red", error.stack));
    else
      console.error(styleText("red", text));
  }
}

export function logCommand(...args) {
  const cmd = args.join(" ");
  if (GITHUB_ACTIONS_OUTPUT) {
    core.notice(styleText("blue", cmd));
  } else {
    console.log(styleText("blue", cmd));
  }
}


export async function logGroup(name, body) {
  if (GITHUB_ACTIONS_OUTPUT) {
    core.startGroup(name);
  } else {
    logInfo("=".repeat(80));
    logInfo(name);
    logInfo(".".repeat(80));
  }
  try {
    return await body();
  } finally {
    if (GITHUB_ACTIONS_OUTPUT)
      core.endGroup();
  } 
}


export function printHelp(message, optionDefinitions) {
  const usage = commandLineUsage([
      {
          header: "Run all tests",
      },
      {
          header: "Options",
          optionList: optionDefinitions,
      },
  ]);
  if (!message?.length) {
      console.log(usage);
      process.exit(0);
  } else {
      console.error(message);
      console.log();
      console.log(usage);
      process.exit(1);
  }
}


export async function runTest(label, testFunction) {
    try {
      await logGroup(label, testFunction);
      logInfo("✅ Test completed!");
    } catch(e) {
      logError("❌ Test failed!");
      logError(e);
      return false;
    }
    return true;
}


export async function sh(binary, ...args) {
  const cmd = `${binary} ${args.join(" ")}`;
  if (GITHUB_ACTIONS_OUTPUT)
    core.startGroup(binary);
  logCommand(cmd);
  try {
    return await spawnCaptureStdout(binary, args);
  } catch(e) {
    logError(e.stdoutString);
    throw e;
  } finally {
    if (GITHUB_ACTIONS_OUTPUT)
      core.endGroup();
  }
}

const SPAWN_OPTIONS =  Object.freeze({ 
  stdio: ["inherit", "pipe", "inherit"]
});

async function spawnCaptureStdout(binary, args, options={}) {
  options = Object.assign(options, SPAWN_OPTIONS);
  const childProcess = spawn(binary, args, options);
  childProcess.stdout.pipe(process.stdout);
  return new Promise((resolve, reject) => {
    childProcess.stdoutString = "";
    childProcess.stdio[1].on("data", (data) => {
      childProcess.stdoutString += data.toString();
    });
    childProcess.on("close", (code) => {
      if (code === 0) {
        resolve(childProcess);
      } else {
        // Reject the Promise with an Error on failure
        const error = new Error(`Command failed with exit code ${code}: ${binary} ${args.join(" ")}`);
        error.process = childProcess;
        error.stdout = childProcess.stdoutString;
        error.exitCode = code;
        reject(error);
      }
    });
    childProcess.on("error", reject);
  })
}
