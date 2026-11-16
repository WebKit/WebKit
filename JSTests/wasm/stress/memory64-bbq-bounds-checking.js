//@ skip if $addressBits <= 32
//@ requireOptions("--useWasmMemory64=1")
//@ runDefaultWasm("-m", "--useWasmMemory64=1", "--useOMGJIT=0", "--thresholdForBBQOptimizeAfterWarmUp=0")
// https://bugs.webkit.org/show_bug.cgi?id=308683
// Memory64 cannot use signaling memory; BBQ must emit explicit bounds checks.
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

const { exports } = await instantiate(`
(module
  (memory i64 1)
  (func (export "load") (param i64) (result i32)
    (i32.load (local.get 0)))
  (func (export "store") (param i64)
    (i32.store (local.get 0) (i32.const 7)))
  (func (export "constLoad") (result i32)
    (i32.load (i64.const -1)))
  (func (export "largeOffsetLoad") (result i32)
    (i32.load offset=0xffffffff (i64.const 0)))
)
`, {}, { memory64: true });

function test() {
    assert.throws(() => exports.load(0xffffffffffffffffn), WebAssembly.RuntimeError, "Out of bounds");
    assert.throws(() => exports.load(0xfffffffffffffffen), WebAssembly.RuntimeError, "Out of bounds");
    assert.throws(() => exports.store(0xffffffffffffffffn), WebAssembly.RuntimeError, "Out of bounds");
    assert.throws(() => exports.constLoad(), WebAssembly.RuntimeError, "Out of bounds");
    assert.throws(() => exports.largeOffsetLoad(), WebAssembly.RuntimeError, "Out of bounds");
    // One page = 65536 bytes; i32 load needs 4 bytes.
    assert.throws(() => exports.load(65536n), WebAssembly.RuntimeError, "Out of bounds");
    assert.throws(() => exports.load(65535n), WebAssembly.RuntimeError, "Out of bounds");
    assert.throws(() => exports.load(65533n), WebAssembly.RuntimeError, "Out of bounds");
    assert.eq(exports.load(65532n), 0);
    exports.store(0n);
    assert.eq(exports.load(0n), 7);
}

for (let i = 0; i < wasmTestLoopCount; ++i)
    test();
