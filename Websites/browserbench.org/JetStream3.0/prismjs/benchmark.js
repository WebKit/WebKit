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

const EXPECTED_ASSERTION_COUNT = 1213680;

class Benchmark extends StartupBenchmark {
  lastResult;
  totalHash = 0xdeadbeef;
  samples = [];

  constructor({iterationCount}) {
    super({
      iterationCount,
      expectedCacheCommentCount: 71,
      sourceCodeReuseCount: 1,
    });
  }

  async init() {
    await Promise.all([
      super.init(),
      this.loadData("cpp", JetStream.preload.SAMPLE_CPP, -1086372285),
      this.loadData("css", JetStream.preload.SAMPLE_CSS, 1173668337),
      this.loadData("markup", JetStream.preload.SAMPLE_HTML, -270772291),
      this.loadData("js", JetStream.preload.SAMPLE_JS, -838545229),
      this.loadData("markdown", JetStream.preload.SAMPLE_MD, 5859883),
      this.loadData("sql", JetStream.preload.SAMPLE_SQL, 5859941),
      this.loadData("json", JetStream.preload.SAMPLE_JSON, 5859883),
      this.loadData("typescript", JetStream.preload.SAMPLE_TS, 133251625),
    ]);
  }

  async loadData(lang, file, hash) {
    const sample = { lang, hash };
    // Push eagerly to have deterministic order.
    this.samples.push(sample);
    sample.content = await JetStream.getString(file);
    // Warm up quickHash and force good string representation.
    this.quickHash(sample.content);
    console.assert(sample.content.length > 0);
  }

  runIteration(iteration) {
    // Module is loaded into PrismJSBenchmark
    let PrismJSBenchmark;
    eval(this.iterationSourceCodes[iteration]);
    this.lastResult = PrismJSBenchmark.runTest(this.samples);

    for (const result of this.lastResult) {
      result.hash = this.quickHash(result.html);
      this.totalHash ^= result.hash;
    }
  }

  validate() {
    console.assert(this.lastResult.length == this.samples.length);
    for (let i = 0; i < this.samples.length; i++) {
      const sample = this.samples[i];
      const result = this.lastResult[i];
      console.assert(result.html.length > 0);
      console.assert(
        result.hash == sample.hash,
        `Invalid result.hash = ${result.hash}, expected ${sample.hash}`
      );
    }
  }
}
