// Copyright 2025 the V8 project authors. All rights reserved.
// Copyright 2025 Apple Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Benchmark {
  async init() {
    Module.wasmBinary = await JetStream.getBinary(JetStream.preload.wasmBinary);
  }

  async runIteration() {
    if (!Module._compressFile)
      await setupModule(Module);

    Module.FS.writeFile('input', Module.wasmBinary);

    const inputStr = Module.stringToNewUTF8('input');
    const inputzStr = Module.stringToNewUTF8('input.z');
    const inputzoutStr = Module.stringToNewUTF8('input.z.out');

    for (let i = 0; i < 10; i++) {
      Module._compressFile(inputStr, inputzStr);
      Module._decompressFile(inputzStr, inputzoutStr);
      if (Module.FS.stat('input').size !== Module.FS.stat('input.z.out').size) {
        throw new Error("Length after decompression doesn't match");
      }
    }

    Module._free(inputzoutStr);
    Module._free(inputzStr);
    Module._free(inputStr);
  }

  validate() {
    const input = Module.FS.readFile('input');
    const outputRoundtrip = Module.FS.readFile('input.z.out');
    for (let i = 0; i < input.length; i++) {
      if (input[i] !== outputRoundtrip[i]) {
        throw new Error("Content after decompression doesn't match at offset " + i);
      }
    }
  }
}
