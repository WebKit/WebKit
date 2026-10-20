//@ skip if $architecture != "arm64" && $architecture != "x86_64"

import * as assert from "../assert.js";
import { instantiate } from "../wabt-wrapper.js";

const stores = [
    ["i32", "i32.atomic.store"],
    ["i32", "i32.atomic.store8"],
    ["i32", "i32.atomic.store16"],
    ["i64", "i64.atomic.store"],
    ["i64", "i64.atomic.store8"],
    ["i64", "i64.atomic.store16"],
    ["i64", "i64.atomic.store32"],
];

const wat = `
    (module
        (memory 1 1 shared)
        ${stores.map(([type, store]) => `
            (func (export "${store}") (param i32 ${type}) (result ${type})
                (${store} (local.get 0) (local.get 1))
                (local.get 1))`).join("")}
    )`;

const instance = await instantiate(wat, {}, { threads: true });

let next = 0;
for (let i = 0; i < wasmTestLoopCount; ++i) {
    for (const [type, store] of stores) {
        const value = type === "i64" ? BigInt(++next) : ++next;
        assert.eq(instance.exports[store](0, value), value);
    }
}
