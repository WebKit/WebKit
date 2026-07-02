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

const CACHE_BUST_COMMENT = "/*ThouShaltNotCache*/";
const CACHE_BUST_COMMENT_RE = new RegExp(
  `\n${RegExp.escape(CACHE_BUST_COMMENT)}\n`,
  "g"
);

class StartupBenchmark {
  // Total iterations for this benchmark.
  #iterationCount = 0;
  // Original source code.
  #sourceCode;
  // quickHahs(this.#sourceCode) for use in custom validate() methods.
  #sourceHash = 0;
  // Number of no-cache comments in the original #sourceCode.
  #expectedCacheCommentCount = 0;
  // How many times (separate iterations) should we reuse the source code.
  // Use 0 to skip and only use a single #sourceCode string.
  #sourceCodeReuseCount = 1;
  // #sourceCode for each iteration, number of unique sources is controlled
  // by codeReuseCount;
  #iterationSourceCodes = [];

  constructor({
    iterationCount,
    expectedCacheCommentCount,
    sourceCodeReuseCount = 1,
  } = {}) {
    console.assert(
      iterationCount > 0,
      `Expected iterationCount to be positive, but got ${iterationCount}`
    );
    this.#iterationCount = iterationCount;
    console.assert(
      expectedCacheCommentCount > 0,
      `Expected expectedCacheCommentCount to be positive, but got ${expectedCacheCommentCount}`
    );
    this.#expectedCacheCommentCount = expectedCacheCommentCount;
    console.assert(
      sourceCodeReuseCount >= 0,
      `Expected sourceCodeReuseCount to be non-negative, but got ${sourceCodeReuseCount}`
    );
    this.#sourceCodeReuseCount = sourceCodeReuseCount;
  }

  get iterationCount() {
    return this.#iterationCount;
  }

  get sourceCode() {
    return this.#sourceCode;
  }

  get sourceHash() {
    return this.#sourceHash;
  }

  get expectedCacheCommentCount() {
    return this.#expectedCacheCommentCount;
  }

  get sourceCodeReuseCount() {
    return this.#sourceCodeReuseCount;
  }

  get iterationSourceCodes() {
    return this.#iterationSourceCodes;
  }

  async init() {
    if (!JetStream.preload.BUNDLE) {
      throw new Error("Missing JetStream.preload.BUNDLE");
    }
    this.#sourceCode = await JetStream.getString(JetStream.preload.BUNDLE);
    if (!this.sourceCode || !this.sourceCode.length) {
      throw new Error("Couldn't load JetStream.preload.BUNDLE");
    }

    const cacheCommentCount = this.sourceCode.match(
      CACHE_BUST_COMMENT_RE
    ).length;
    this.#sourceHash = this.quickHash(this.sourceCode);
    this.validateSourceCacheComments(cacheCommentCount);
    for (let i = 0; i < this.iterationCount; i++)
      this.#iterationSourceCodes[i] = this.createIterationSourceCode(i);
    this.validateIterationSourceCodes();
  }

  validateSourceCacheComments(cacheCommentCount) {
    console.assert(
      cacheCommentCount === this.expectedCacheCommentCount,
      `Invalid cache comment count ${cacheCommentCount} expected ${this.expectedCacheCommentCount}.`
    );
  }

  validateIterationSourceCodes() {
    if (this.#iterationSourceCodes.some((each) => !each?.length))
      throw new Error(`Got invalid iterationSourceCodes`);
    let expectedSize = 1;
    if (this.sourceCodeReuseCount !== 0)
      expectedSize = Math.ceil(this.iterationCount / this.sourceCodeReuseCount);
    const uniqueSources = new Set(this.iterationSourceCodes);
    if (uniqueSources.size != expectedSize)
      throw new Error(
        `Expected ${expectedSize} unique sources, but got ${uniqueSources.size}.`
      );
  }

  createIterationSourceCode(iteration) {
    // Alter the code per iteration to prevent caching.
    const cacheId =
      Math.floor(iteration / this.sourceCodeReuseCount) *
      this.sourceCodeReuseCount;
    // Reuse existing sources if this.codeReuseCount > 1:
    if (cacheId < this.iterationSourceCodes.length)
      return this.iterationSourceCodes[cacheId];

    const sourceCode = this.sourceCode.replaceAll(
      CACHE_BUST_COMMENT_RE,
      `/*${cacheId}*/`
    );
    // Warm up quickHash.
    this.quickHash(sourceCode);
    return sourceCode;
  }

  quickHash(str) {
    let hash = 5381;
    let i = str.length;
    while (i > 0) {
      hash = (hash * 33) ^ (str.charCodeAt(i) | 0);
      i -= 919;
    }
    return hash | 0;
  }
}
