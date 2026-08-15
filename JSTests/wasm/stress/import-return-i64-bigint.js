// https://bugs.webkit.org/show_bug.cgi?id=220053
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

const values = [
    0n,
    1n,
    -1n,
    20n,
    0x7fffffffffffffffn,
    -0x8000000000000000n,
    0xffffffffffffffffn,
    0x10000000000000000n,
    -0x10000000000000000n,
    42n,
];

async function test() {
    for (const value of values) {
        const expected = BigInt.asIntN(64, value);
        const inst = await instantiate(`
(module
  (func $imp (import "imp" "func") (result i64))
  (func (export "run") (result i64)
    (call $imp))
)`, { imp: { func: () => value } });
        for (let i = 0; i < 100; i++)
            assert.eq(inst.exports.run(), expected);
    }

    const inst = await instantiate(`
(module
  (func $imp (import "imp" "func") (result i64))
  (func (export "run") (result i64)
    (call $imp))
)`, { imp: { func: () => 20 } });
    assert.throws(() => inst.exports.run(), TypeError, "Invalid argument type in ToBigInt operation");
}

await assert.asyncTest(test());
