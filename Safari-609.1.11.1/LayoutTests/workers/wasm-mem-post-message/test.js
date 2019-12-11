"use strict";

const numPages = 10;

const builder = (new Builder())
    .Type().End()
    .Import()
        .Memory("a", "b", {initial: numPages})
    .End()
    .Function().End()
    .Export().Function("foo").End()
    .Code()
        .Function("foo", {params: [], ret: "i32"})
            .I32Const(0)
            .I32Load(0, 0)
            .Return()
        .End()
      .End();

const bin = builder.WebAssembly().get();
const module = new WebAssembly.Module(bin);
let mem = new WebAssembly.Memory({ initial: numPages });
const foo = new WebAssembly.Instance(module, {a: {b: mem}}).exports.foo;

let worker = new Worker("./wasm-mem-post-message/worker.js");

worker.onmessage = function(message) {
    debug("Should not be reached.");
}

let b = mem.buffer;

shouldThrow(() => worker.postMessage(b, [b]), "'TypeError: Cannot transfer a WebAssembly.Memory'");
