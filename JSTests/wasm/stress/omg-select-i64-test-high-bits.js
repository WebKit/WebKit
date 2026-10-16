import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

const wat = `
(module
    (import "m" "call" (func $call))
    (func (export "eqz") (param $x i64) (param $a i64) (param $b i64) (result i64)
        (call $call)
        (select (local.get $a) (local.get $b) (i64.eqz (local.get $x))))
    (func (export "and") (param $x i64) (param $y i64) (param $a i64) (param $b i64) (result i64)
        (call $call)
        (select (local.get $a) (local.get $b) (i64.eqz (i64.and (local.get $x) (local.get $y)))))
)
`;

const instance = await instantiate(wat, { m: { call() { } } });
const { eqz, and } = instance.exports;

for (let i = 0; i < wasmTestLoopCount; ++i) {
    assert.eq(eqz(0n, 1n, 2n), 1n);
    assert.eq(eqz(42n, 1n, 2n), 2n);
    assert.eq(eqz(0x100000000n, 1n, 2n), 2n);
    assert.eq(eqz(-0x100000000n, 1n, 2n), 2n);

    assert.eq(and(0n, -1n, 1n, 2n), 1n);
    assert.eq(and(42n, -1n, 1n, 2n), 2n);
    assert.eq(and(0x100000000n, -1n, 1n, 2n), 2n);
    assert.eq(and(-1n, 0x100000000n, 1n, 2n), 2n);
    assert.eq(and(0x100000000n, 0xffffffffn, 1n, 2n), 1n);
}
