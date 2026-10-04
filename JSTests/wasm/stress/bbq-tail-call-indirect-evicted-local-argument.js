//@ requireOptions("--useWasmTailCalls=true")
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// A cross-instance return_call_indirect whose arguments are locals held in registers. Every
// allocatable GPR holds a local, so making the restore frame needs a scratch that evicts one of
// them. Locals are set in register allocation order (ARM64: x0-x7, x9-x15). The high-index locals
// land in the registers freed before the frame is copied, so only high-index slots bound the copy;
// local 0 lands in the first register the scratch can take, and its slot is far above that bound.
const argumentCount = 15;
const setOrder = [14, 13, 11, 10, 9, 8, 7, 6, 12, 0, 1, 2, 3, 4, 5];
const params = Array(argumentCount).fill("i32").join(" ");

function weightedSum(values)
{
    let sum = 0;
    for (let i = 0; i < values.length; ++i)
        sum = (sum + Math.imul(values[i], i + 1)) | 0;
    return sum;
}

const calleeBody = [];
for (let i = 0; i < argumentCount; ++i)
    calleeBody.push(`(local.get ${i}) (i32.const ${i + 1}) (i32.mul)`, i ? "(i32.add)" : "");

const watCallee = `
(module
    (type $sig (func (param ${params}) (result i32)))
    (func (export "weightedSum") (type $sig)
        ${calleeBody.join("\n        ")}
    )
)`;

const locals = [];
const sets = [];
const gets = [];
for (let i = 0; i < argumentCount; ++i) {
    locals.push(`(local $l${i} i32)`);
    gets.push(`(local.get $l${i})`);
}
for (let i of setOrder)
    sets.push(`(local.set $l${i} (i32.add (global.get $base) (i32.const ${(i + 1) * 7})))`);

const watCaller = `
(module
    (type $sig (func (param ${params}) (result i32)))
    (import "m" "weightedSum" (func $weightedSum (type $sig)))
    (import "m" "base" (global $base i32))
    (table 1 funcref)
    (elem (i32.const 0) $weightedSum)
    (func (export "test") (result i32)
        ${locals.join(" ")}
        ${sets.join("\n        ")}
        ${gets.join(" ")}
        (i32.const 0)
        (return_call_indirect (type $sig))
    )
)`;

async function test()
{
    const base = 1000;
    const callee = await instantiate(watCallee, { }, { tail_call: true });
    const caller = await instantiate(watCaller, {
        m: { weightedSum: callee.exports.weightedSum, base: new WebAssembly.Global({ value: "i32" }, base) },
    }, { tail_call: true });

    const expected = weightedSum(Array.from({ length: argumentCount }, (_, i) => base + (i + 1) * 7));
    for (let i = 0; i < wasmTestLoopCount; ++i)
        assert.eq(caller.exports.test(), expected);
}

await assert.asyncTest(test());
