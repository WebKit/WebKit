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

globalThis.console = {
  log() { },
  warn() { },
  assert(condition) {
    if (!condition) throw new Error("Invalid assertion");
  }
};

globalThis.clearTimeout = function () { };


// JetStream benchmark.
class Benchmark extends StartupBenchmark {
  // How many times (separate iterations) should we reuse the source code.
  // Use 0 to skip.
  lastResult = {};
  sourceHash = 0

  constructor({iterationCount}) {
    super({
      iterationCount,
      expectedCacheCommentCount: 597,
      sourceCodeReuseCount: 1,
    });
  }

  runIteration(iteration) {
    let sourceCode = this.iterationSourceCodes[iteration];
    if (!sourceCode)
      throw new Error(`Could not find source for iteration ${iteration}`);
    // Module in sourceCode it assigned to the ReactRenderTest variable.
    let ReactRenderTest;

    let initStart = performance.now();
    const res = eval(sourceCode);
    const runStart = performance.now();

    this.lastResult = ReactRenderTest.renderTest();
    this.lastResult.htmlHash = this.quickHash(this.lastResult.html);
    const end = performance.now();

    const loadTime = runStart - initStart;
    const runTime = end - runStart;
    // For local debugging: 
    // print(`Iteration ${iteration}:`);
    // print(`  Load time: ${loadTime.toFixed(2)}ms`);
    // print(`  Render time: ${runTime.toFixed(2)}ms`);
  }

  validate() {
    this.expect("HTML length", this.lastResult.html.length, 183778);
    this.expect("HTML hash", this.lastResult.htmlHash, 1177839858);
  }

  expect(name, value, expected) {
    if (value != expected)
      throw new Error(`Expected ${name} to be ${expected}, but got ${value}`);
  }
}
