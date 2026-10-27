//@ skip unless $isSIMDPlatform
//@ requireOptions("--useWasmSIMD=1")
//@ runDefaultWasm("-m", "--useConcurrentJIT=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeAfterWarmUp=0")

// array.new of v128 elements fills them with overlapping vector stores for short lengths, a store
// loop for longer ones, and a call for the longest. Sweep lengths across all of those, with both a
// runtime and a constant length.

import { instantiate } from "../gc/wast-wrapper.js"
import * as assert from "../assert.js"

const maxLength = 40;
const constantLengths = [0, 1, 2, 3, 7, 33];
const first = "(v128.const i64x2 0x0123456789abcdef 0x1122334455667788)";
const second = "(v128.const i8x16 0x5a 0x5a 0x5a 0x5a 0x5a 0x5a 0x5a 0x5a 0x5a 0x5a 0x5a 0x5a 0x5a 0x5a 0x5a 0x5a)";

async function testV128() {
    const constantTests = constantLengths.map(length => `
            (func (export "constant${length}") (result i32)
                (call $check (array.new $arr ${first} (i32.const ${length})) (i32.const ${length}) (i32.const 1)))`).join("");
    const exports = (await instantiate(`
        (module
            (type $arr (array (mut v128)))

            (func $value (param $which i32) (result v128)
                (if (result v128) (local.get $which) (then ${first}) (else ${second})))

            (func $check (param $array (ref $arr)) (param $length i32) (param $which i32) (result i32)
                (local $i i32)
                (if (i32.ne (array.len (local.get $array)) (local.get $length))
                    (then (return (i32.const -1))))
                (block $done
                    (br_if $done (i32.eqz (local.get $length)))
                    (loop $loop
                        (if (i32.eqz (i8x16.all_true (i8x16.eq (array.get $arr (local.get $array) (local.get $i)) (call $value (local.get $which)))))
                            (then (return (i32.add (local.get $i) (i32.const 1)))))
                        (local.set $i (i32.add (local.get $i) (i32.const 1)))
                        (br_if $loop (i32.lt_u (local.get $i) (local.get $length)))))
                (i32.const 0))

            (func (export "variable") (param $length i32) (param $which i32) (result i32)
                (call $check (array.new $arr (call $value (local.get $which)) (local.get $length)) (local.get $length) (local.get $which)))
            (func (export "variableFirst") (param $length i32) (result i32)
                (call $check (array.new $arr ${first} (local.get $length)) (local.get $length) (i32.const 1)))
            ${constantTests})
    `)).exports;

    for (let length = 0; length <= maxLength; ++length) {
        for (let which of [0, 1])
            assert.eq(exports.variable(length, which), 0, `variable(${length}, ${which})`);
        assert.eq(exports.variableFirst(length), 0, `variableFirst(${length})`);
    }
    for (let length of constantLengths)
        assert.eq(exports[`constant${length}`](), 0, `constant${length}`);
}

await assert.asyncTest(testV128())
