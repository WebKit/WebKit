// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Polyfills that Transformers.js / the ONNX runtime needs in JavaScript shells.

class URL {
  href;
  constructor(url, base) {
    // DEBUG
    // console.log('URL', url, base);
    this.href = url;
  }
}
globalThis.URL = URL;

// Polyfill fetch for shell-compatibility and to cache / preload model weights etc.
let preload = { /* Initialized in init() below due to async. */ };

async function redirectingFetch(url) {
  // DEBUG
  // console.log('fetch', url);

  if (url.startsWith("./"))
    url = JetStream.resources[url];

  // Redirect some paths to cached/preloaded resources.
  if (preload[url]) {
    return {
      ok: true,
      status: 200,
      arrayBuffer() { return preload[url]; },
      async blob() {
        return {
          size: preload[url].byteLength,
          async arrayBuffer() { return preload[url]; }
        }
      },
    };
  }

  throw new Error(`Unexpected resource requested in benchmark: ${url}`);
};

// JetStream benchmark harness. Reuse for two different Transformers.js tasks.
// Assumes `preloadFiles(module)`, `initPipeline(pipelineFromTransformersJs)`,
// and `doTask(initializedPipeline, inputArrayBuffer)` is in the global scope.

class Benchmark {
  transformersJsModule;
  wasmBinary;
  pipeline;
  inputFile;
  output;

  async init() {
    this.transformersJsModule = await JetStream.dynamicImport(JetStream.preload.transformersJsModule);
    this.wasmBinary = await JetStream.getBinary(JetStream.preload.onnxWasmBinary);

    for (const url of Object.values(JetStream.preload)) {
      preload[url] = await JetStream.getBinary(url);
    }

    if ('inputFile' in JetStream.preload) {
      this.inputFile = (await JetStream.getBinary(JetStream.preload.inputFile)).buffer;
      // DEBUG
      // console.log('inputFile', this.inputFile.byteLength, 'bytes');
    }

    // After we have loaded everything close the door behind us to make sure no other network requests happen.
    globalThis.fetch = redirectingFetch;
  }

  async runIteration() {
    // Initialize the inference pipeline in the first iteration.
    if (!this.pipeline) {
      // TODO: Profile startup only: What is taking so much time here?
      let { env, pipeline } = this.transformersJsModule;
    
      env.allowRemoteModels = false;
      env.allowLocalModels = true;
      env.localModelPath = './transformersjs/build/models/';

      // Always select the Wasm backend, nothing else.
      delete env.backends.onnx.webgl;
      delete env.backends.onnx.webgpu;
    
      // Single-threaded only for now, since we cannot spawn workers in shells.
      // TODO: Implement sufficiently powerful workers in shells (or provide
      // polyfills).
      env.backends.onnx.wasm.numThreads = 1;

      // Do not specify path prefix, because this loads the JSEP build by default.
      // TODO: Do we want the JSEP build because it's the default online, or the
      // non-asyncified one, since it's the smaller / more performant one?
      // env.backends.onnx.wasm.wasmPaths = 'build/onnxruntime-web/';
      // So instead, give the ONNX runtime files directly:
      env.backends.onnx.wasm.wasmPaths = {
        // The ONNX runtime module is dynamically imported relative to the 
        // Transformers.js module above, hence strip the prefix.
        // With preloading, this is an (absolute) blob URL, so the replace is a nop.
        mjs: JetStream.preload.onnxJsModule.replace('./transformersjs/build/', './')
      };
      // Give it the wasmBinary directly instead of a path, such that the
      // ONNX runtime uses asynchronous (not streaming) Wasm instantiation.
      // (To keep the shell and browser results comparable, and streaming
      // instantiation is not available in shells.)
      env.backends.onnx.wasm.wasmBinary = this.wasmBinary;

      this.pipeline = await initPipeline(pipeline);
    }
    
    this.output = await doTask(this.pipeline, this.inputFile);
  }

  validate() {
    validate(this.output);
  }
}
