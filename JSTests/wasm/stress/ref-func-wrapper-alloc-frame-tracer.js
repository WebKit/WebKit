//@ requireOptions("--alwaysUseShadowChicken=true", "--slowPathAllocsBetweenGCs=1", "--forceGCSlowPaths=true")

import { compile } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// ref.func materializes the funcref's JS wrapper via ensureFunctionWrapper, which
// allocates and so can GC. Wasm tiers update topCallFrame only just-in-time, so
// after a JS import returns it points at dead native state, and a GC there makes
// ShadowChicken read that as a JS CallFrame.
//
// Each ref.func targets a distinct function, preceded by an import call to leave
// topCallFrame stale. Fresh instances have empty wrapper caches, so later
// iterations rerun the path in whichever tier the function reached.

const numFuncs = 8;
const numInstances = 8;

let funcs = "";
let declares = "";
let body = "";
for (let i = 0; i < numFuncs; ++i) {
    funcs += `    (func $f${i})\n`;
    declares += ` $f${i}`;
    body += `        (call $import)\n        (drop (ref.func $f${i}))\n`;
}

let wat = `
(module
    (import "m" "f" (func $import))
    (elem declare func${declares})
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
