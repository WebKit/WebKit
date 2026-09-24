//@ skip if $addressBits <= 32
//@ runDefaultWasm("-m", "--useConcurrentJIT=0", "--thresholdForBBQOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeSoon=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForOMGOptimizeSoon=0")
//@ runDefaultWasm("-m", "--useConcurrentJIT=1", "--thresholdForBBQOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeSoon=0", "--thresholdForOMGOptimizeAfterWarmUp=0", "--thresholdForOMGOptimizeSoon=0")
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (func $ifElse (param $flag i32) (param $x i32) (result i32)
        local.get $flag
        if (result i32)
            local.get $x
            i32.const 100
            i32.add
        else
            local.get $x
            i32.const 200
            i32.add
        end
    )

    (func $ifOnly (param $flag i32) (param $x i32) (result i32)
        local.get $flag
        if
            local.get $x
            i32.const 1000
            i32.add
            local.set $x
        end
        local.get $x
    )

    (func $brIf (param $flag i32) (param $x i32) (result i32)
        block (result i32)
            local.get $x
            i32.const 7
            i32.add
            local.get $flag
            br_if 0
            drop
            local.get $x
            i32.const 9
            i32.add
        end
    )

    (func $brTable (param $index i32) (result i32)
        block
            block
                block
                    local.get $index
                    br_table 0 1 2
                end
                i32.const 10
                return
            end
            i32.const 20
            return
        end
        i32.const 30
    )

    (func $select (param $flag i32) (param $x i32) (result i32)
        local.get $x
        i32.const 3
        i32.add
        local.get $x
        i32.const 5
        i32.add
        local.get $flag
        select
    )

    (func $overwritten (param $flag i32) (param $x i32) (result i32)
        local.get $x
        local.set $flag
        local.get $flag
        if (result i32)
            i32.const 1
        else
            i32.const 2
        end
    )

    (func $setLaterInLoop (param $flag i32) (param $n i32) (result i32)
        (local $acc i32)
        loop $loop
            local.get $flag
            if
                local.get $acc
                i32.const 1
                i32.add
                local.set $acc
            end
            i32.const 1
            local.set $flag
            local.get $n
            i32.const 1
            i32.sub
            local.tee $n
            br_if $loop
        end
        local.get $acc
    )

    (func $zeroLocalSetLaterInLoop (param $n i32) (result i32)
        (local $flag i32)
        (local $acc i32)
        loop $loop
            local.get $flag
            if
                local.get $acc
                i32.const 1
                i32.add
                local.set $acc
            end
            i32.const 1
            local.set $flag
            local.get $n
            i32.const 1
            i32.sub
            local.tee $n
            br_if $loop
        end
        local.get $acc
    )

    (func (export "test") (param $x i32) (result i32)
        (local $sum i32)
        i32.const 1
        local.get $x
        call $ifElse
        i32.const 0
        local.get $x
        call $ifElse
        i32.add

        i32.const 1
        local.get $x
        call $ifOnly
        i32.add
        i32.const 0
        local.get $x
        call $ifOnly
        i32.add

        i32.const 1
        local.get $x
        call $brIf
        i32.add
        i32.const 0
        local.get $x
        call $brIf
        i32.add

        i32.const 0
        call $brTable
        i32.add
        i32.const 1
        call $brTable
        i32.add
        i32.const 2
        call $brTable
        i32.add
        i32.const 42
        call $brTable
        i32.add

        i32.const 1
        local.get $x
        call $select
        i32.add
        i32.const 0
        local.get $x
        call $select
        i32.add

        i32.const 1
        local.get $x
        call $overwritten
        i32.add
        i32.const 0
        local.get $x
        call $overwritten
        i32.add

        i32.const 0
        i32.const 5
        call $setLaterInLoop
        i32.add

        i32.const 5
        call $zeroLocalSetLaterInLoop
        i32.add
    )
)
`;

function expected(x) {
    let flagOf = (value) => value !== 0;
    let result = 0;
    result += x + 100;
    result += x + 200;
    result += x + 1000;
    result += x;
    result += x + 7;
    result += x + 9;
    result += 10 + 20 + 30 + 30;
    result += x + 3;
    result += x + 5;
    result += flagOf(x) ? 1 : 2;
    result += flagOf(x) ? 1 : 2;
    result += 4;
    result += 4;
    return result | 0;
}

async function test() {
    const instance = await instantiate(wat, {});
    const { test } = instance.exports;
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        let x = i % 3;
        assert.eq(test(x), expected(x));
    }
}

await assert.asyncTest(test());
