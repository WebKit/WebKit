//@ requireOptions("--alwaysUseShadowChicken=true", "--slowPathAllocsBetweenGCs=1", "--forceGCSlowPaths=true")

import { compile } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// Companion to ref-func-wrapper-alloc-frame-tracer.js, for table.get.
//
// An active elem segment of non-imported funcrefs installs only Wasm-side metadata
// (FuncRefTable::setLazy); FuncRefTable::get materializes the JS wrapper on first
// read. Each index is read once per instance, so every table.get allocates.

const numFuncs = 8;
const numInstances = 8;

let funcs = "";
let elems = "";
let body = "";
for (let i = 0; i < numFuncs; ++i) {
    funcs += `    (func $f${i})\n`;
    elems += ` $f${i}`;
    body += `        (call $import)\n        (drop (table.get $t (i32.const ${i})))\n`;
}

let wat = `
(module
    (import "m" "f" (func $import))
    (table $t ${numFuncs} funcref)
    (elem (i32.const 0)${elems})
${funcs}    (func (export "test")
${body}    )
)
`;

let calls = 0;
const imports = { m: { f: () => { ++calls; } } };
const module = await compile(wat);

for (let i = 0; i < numInstances; ++i)
    new WebAssembly.Instance(module, imports).exports.test();

assert.eq(calls, numFuncs * numInstances);
