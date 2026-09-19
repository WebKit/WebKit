//@ requireOptions("--useWasmMultiMemory=1")
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
  (memory 1)
  (memory 2)
  (func (export "sizeA") (result i32)
    (memory.size 0))
  (func (export "sizeB") (result i32)
    (memory.size 1))
  (func (export "growB") (param i32) (result i32)
    (memory.grow 1 (local.get 0)))
)
`;

async function test() {
    const instance = await instantiate(wat, {}, { multi_memory: true });
    const { sizeA, sizeB, growB } = instance.exports;

    assert.eq(sizeA(), 1);
    assert.eq(sizeB(), 2);

    for (let i = 0; i < wasmTestLoopCount; i++)
        assert.eq(sizeB(), 2);

    assert.eq(growB(1), 2);
    assert.eq(sizeA(), 1);
    assert.eq(sizeB(), 3);

    for (let i = 0; i < wasmTestLoopCount; i++) {
        assert.eq(sizeA(), 1);
        assert.eq(sizeB(), 3);
    }
}

await assert.asyncTest(test());
