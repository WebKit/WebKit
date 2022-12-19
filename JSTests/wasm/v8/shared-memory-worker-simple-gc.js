//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Failure:
// Exception: ReferenceError: Can't find variable: Worker
//  NewWorker@shared-memory-worker-simple-gc.js:24:20
//  RunSingleWorkerSingleMemoryTest@shared-memory-worker-simple-gc.js:41:25
//  global code@shared-memory-worker-simple-gc.js:90:32

// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

const kNumIterations = 10;

function NewWorker() {
  let script =
`onmessage = (msg) => {
   if (msg.memory) postMessage("ack");
   if (msg.quit) postMessage("bye");
   gc();
}`;
  return new Worker(script, {type: 'string'});
}

function PingWorker(worker, memory) {
  worker.postMessage({memory: memory});
  assertEquals("ack", worker.getMessage());
  worker.postMessage({quit: true});
  assertEquals("bye", worker.getMessage());
}

function AllocMemory() {
  let pages = 1, max = 1;
  return new WebAssembly.Memory({initial : pages, maximum : max, shared : true});
}

function RunSingleWorkerSingleMemoryTest() {
  // print(arguments.callee.name);
  let worker = NewWorker();
  let first = AllocMemory();
  for (let i = 0; i < kNumIterations; i++) {
    // print(`iteration ${i}`);
    PingWorker(worker, first);
    gc();
  }
  worker.terminate();
}

function RunSingleWorkerTwoMemoryTest() {
  // print(arguments.callee.name);
  let worker = NewWorker();
  let first = AllocMemory(), second = AllocMemory();
  for (let i = 0; i < kNumIterations; i++) {
    // print(`iteration ${i}`);
    PingWorker(worker, first);
    PingWorker(worker, second);
    gc();
  }
  worker.terminate();
}

function RunSingleWorkerMultipleMemoryTest() {
  // print(arguments.callee.name);
  let worker = NewWorker();
  let first = AllocMemory();
  for (let i = 0; i < kNumIterations; i++) {
    // print(`iteration ${i}`);
    PingWorker(worker, first);
    PingWorker(worker, AllocMemory());
    gc();
  }
  worker.terminate();
}

function RunMultipleWorkerMultipleMemoryTest() {
  // print(arguments.callee.name);
  let first = AllocMemory();
  for (let i = 0; i < kNumIterations; i++) {
    // print(`iteration ${i}`);
    let worker = NewWorker();
    PingWorker(worker, first);
    PingWorker(worker, AllocMemory());
    worker.terminate();
    gc();
  }
}

RunSingleWorkerSingleMemoryTest();
RunSingleWorkerTwoMemoryTest();
RunSingleWorkerMultipleMemoryTest();
RunMultipleWorkerMultipleMemoryTest();
