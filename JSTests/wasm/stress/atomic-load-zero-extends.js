//@ skip if $architecture != "arm64" && $architecture != "x86_64"

import * as assert from "../assert.js";
import { instantiate } from "../wabt-wrapper.js";

const loads = [
    ["i32", "i32.atomic.load", -1],
    ["i32", "i32.atomic.load8_u", 0xff],
    ["i32", "i32.atomic.load16_u", 0xffff],
    ["i64", "i64.atomic.load", -1n],
    ["i64", "i64.atomic.load8_u", 0xffn],
    ["i64", "i64.atomic.load16_u", 0xffffn],
    ["i64", "i64.atomic.load32_u", 0xffffffffn],
];

const wat = `
    (module
        (memory (export "memory") 1 1 shared)
        ${loads.map(([type, load]) => `
            (func (export "${load}") (param i32) (result ${type})
                (${load} (local.get 0)))`).join("")}
    )`;

const instance = await instantiate(wat, {}, { threads: true });
new Uint8Array(instance.exports.memory.buffer).fill(0xff, 0, 16);

for (let i = 0; i < wasmTestLoopCount; ++i) {
    for (const [type, load, expected] of loads)
        assert.eq(instance.exports[load](0), expected);
}
