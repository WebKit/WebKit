//@ requireOptions("--coalesceSpillSlots=0")

import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

globalThis.wasmTestLoopCount ??= 10000;

const verbose = false;
// Create a large enough frame so that stack slots are not directly addressable from %sp or %fp.
const num_copies = 2048;

const type = 'f64';

let wat = `(module\n`;

for (let i = 0; i < num_copies; i++)
    wat += `  (global $g${i} (mut ${type}) (${type}.const ${i+1000}))\n`;
for (let i = 0; i < num_copies; i++)
    wat += `  (global $h${i} (mut ${type}) (${type}.const ${i}))\n`;

wat += `  (func (export "test") (param $arg i32)(result ${type})\n`;
for (let i = 0; i < num_copies; i++) {
    wat += `    (local $a${i} ${type})\n`;
    wat += `    (local $b${i} ${type})\n`;
}

for (let i = 0; i < num_copies; i++)
    wat += `    (local.set $a${i} (global.get $g${i}))\n`;
for (let i = 0; i < num_copies; i++)
    wat += `    (local.set $b${i} (global.get $h${i}))\n`;

// Induce 'Move (spillA), (spillB), scratchReg' stack coalescable Air instructions using local.set/local.get inside an if block.
wat += `    (if (i32.ne (local.get $arg)(i32.const 0))\n`;
wat += `      (then\n`;

for (let i = 0; i < num_copies; i++)
    wat += `        (local.set $a${i} (local.get $b${i}))\n`

wat += `      )\n`
wat += `    )\n`

for (let i = 0; i < num_copies; i++)
    wat += `    (${type}.add (local.get $a${i})(local.get $b${i}))\n`;

for (let i = 0; i < num_copies - 1; i++)
    wat += `    (${type}.add)\n`;

wat += `  )\n)\n`;

if (verbose)
    print(wat);

async function run() {
    const instance = await instantiate(wat);
    const {test} = instance.exports;
    for (let i = 0; i < wasmTestLoopCount; ++i)
        assert.eq(test(1), num_copies * (num_copies - 1));
}

assert.asyncTest(run());
