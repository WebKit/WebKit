//@ skip unless $isSIMDPlatform
//@ requireOptions("--useWasmSIMD=1")
//@ runDefaultWasm("-m", "--useConcurrentJIT=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeAfterWarmUp=0")

// array.fill of v128 elements is inlined as overlapping vector stores for short lengths and a store
// loop for longer ones, and calls out for the longest. Sweep lengths across all of those and check
// that nothing outside the filled range changes.

import { instantiate } from "../gc/wast-wrapper.js"
import * as assert from "../assert.js"

const length = 40;
const sentinel = "(v128.const i8x16 0xc3 0xc3 0xc3 0xc3 0xc3 0xc3 0xc3 0xc3 0xc3 0xc3 0xc3 0xc3 0xc3 0xc3 0xc3 0xc3)";
const first = "(v128.const i64x2 0x0123456789abcdef 0x1122334455667788)";
const second = "(v128.const i8x16 0x5a 0x5a 0x5a 0x5a 0x5a 0x5a 0x5a 0x5a 0x5a 0x5a 0x5a 0x5a 0x5a 0x5a 0x5a 0x5a)";

async function testV128() {
    const { test } = (await instantiate(`
        (module
            (type $arr (array (mut v128)))
            (global $a (mut (ref null $arr)) (ref.null $arr))

            (func $value (param $which i32) (result v128)
                (if (result v128) (local.get $which) (then ${first}) (else ${second})))

            (func $reset
                (global.set $a (array.new $arr ${sentinel} (i32.const ${length}))))
            (func $fill (param $offset i32) (param $length i32) (param $which i32)
                (array.fill $arr (global.get $a) (local.get $offset) (call $value (local.get $which)) (local.get $length)))
            (func $fillFirst (param $offset i32) (param $length i32)
                (array.fill $arr (global.get $a) (local.get $offset) ${first} (local.get $length)))
            (func $fillSecond (param $offset i32) (param $length i32)
                (array.fill $arr (global.get $a) (local.get $offset) ${second} (local.get $length)))

            (func $check (param $offset i32) (param $length i32) (param $which i32) (result i32)
                (local $i i32)
                (local $end i32)
                (local $expected v128)
                (local.set $end (i32.add (local.get $offset) (i32.add (local.get $length) (i32.const 16))))
                (if (i32.gt_u (local.get $end) (i32.const ${length})) (then (local.set $end (i32.const ${length}))))
                (loop $loop
                    (local.set $expected ${sentinel})
                    (if (i32.and (i32.ge_u (local.get $i) (local.get $offset))
                                 (i32.lt_u (local.get $i) (i32.add (local.get $offset) (local.get $length))))
                        (then (local.set $expected (call $value (local.get $which)))))
                    (if (i32.eqz (i8x16.all_true (i8x16.eq (array.get $arr (global.get $a) (local.get $i)) (local.get $expected))))
                        (then (return (i32.add (local.get $i) (i32.const 1)))))
                    (local.set $i (i32.add (local.get $i) (i32.const 1)))
                    (br_if $loop (i32.lt_u (local.get $i) (local.get $end))))
                (i32.const 0))

            (func (export "test") (param $offset i32) (param $length i32) (param $mode i32) (result i32)
                (call $reset)
                (block $done
                    (block $second
                        (block $first
                            (block $variable
                                (br_table $variable $variable $first $second (local.get $mode)))
                            (call $fill (local.get $offset) (local.get $length) (local.get $mode))
                            (br $done))
                        (call $fillFirst (local.get $offset) (local.get $length))
                        (return (call $check (local.get $offset) (local.get $length) (i32.const 1))))
                    (call $fillSecond (local.get $offset) (local.get $length))
                    (return (call $check (local.get $offset) (local.get $length) (i32.const 0))))
                (call $check (local.get $offset) (local.get $length) (local.get $mode))))
    `)).exports;

    // Modes 0 and 1 fill with a runtime pattern, and modes 2 and 3 with a constant one.
    for (let offset of [0, 1]) {
        for (let count = 0; count <= length - offset; ++count) {
            for (let mode = 0; mode < 4; ++mode)
                assert.eq(test(offset, count, mode), 0, `offset ${offset} length ${count} mode ${mode}`);
        }
    }
}

await assert.asyncTest(testV128())
