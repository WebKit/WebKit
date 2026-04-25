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



class Benchmark {
  files = Object.create(null);

  async init() {
    let WTBenchmark;
    await this.loadAllFiles(JetStream.preload);
    this.sourceCode = this.files.BUNDLE;
    this.WTBenchmark = self.WTBenchmark;
  }

  async loadAllFiles(preload) {
    const loadPromises = Object.entries(preload).map(
      async ([name, url]) => {
        if (name.endsWith(".wasm")) {
          const buffer = (await JetStream.getBinary(url)).buffer;
          if (!(buffer instanceof ArrayBuffer)) {
            // The returned array buffer is from a different global when
            // prefetching resources and running in the shell. This is fine,
            // except for the source map code doing an instanceof
            // check that fails for the prototype being in a different realm.
            Object.setPrototypeOf(buffer, ArrayBuffer.prototype);
          }
          this.files[name] = buffer;
        } else {
          this.files[name] = await JetStream.getString(url);
        }
      })
    await Promise.all(loadPromises);
  }

  async runIteration() {
    await this.WTBenchmark.runTest(this.files);
  }
}