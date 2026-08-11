// Copyright 2025 the V8 project authors. All rights reserved.
// Copyright 2025 Apple Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Benchmark {
    async init() {
        Module.wasmBinary = await JetStream.getBinary(JetStream.preload.wasmBinary);
    }

    async runIteration() {
        if (!Module._main)
            await setupModule(Module);

        Module._main();
    }
};
