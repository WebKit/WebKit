/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

// console.log = () => {};

class Benchmark extends StartupBenchmark {
  iteration = 0;

  constructor({iterationCount, expectedCacheCommentCount}) {
    super({
      iterationCount,
      codeReuseCount: 2,
      expectedCacheCommentCount,
    });
  }

  runIteration() {
    const sourceCode = this.iterationSourceCodes[this.iteration];
    if (!sourceCode)
      throw new Error(`Could not find source for iteration ${this.iteration}`);
    // Module in sourceCode it assigned to the ClassStartupTest variable.
    let BabylonJSBenchmark;

    // let initStart = performance.now();
    eval(sourceCode);
    // const runStart = performance.now();

    const { classNames, cameraRotationLength } = BabylonJSBenchmark.runTest(30);
    const lastResult = {
      classNames,
      cameraRotationLength,
    };
    this.validateIteration(lastResult)
    // const end = performance.now();
    // const loadTime = runStart - initStart;
    // const runTime = end - runStart;
    // For local debugging:
    // print(`Iteration ${this.iteration}:`);
    // print(`  Load time: ${loadTime.toFixed(2)}ms`);
    // print(`  Render time: ${runTime.toFixed(2)}ms`);
    this.iteration++;
  }

  validateIteration(lastResult) {
    this.expect(
      "this.lastResult.classNames.length",
      lastResult.classNames.length,
      2135
    );
    this.expect(
      "this.lastResult.cameraRotationLength",
      Math.round(lastResult.cameraRotationLength * 1000),
      464
    );
  }

  expect(name, value, expected) {
    if (value != expected)
      throw new Error(`Expected ${name} to be ${expected}, but got ${value}`);
  }
}
