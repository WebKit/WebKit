//@ requireOptions("--useWasmSIMD=1", "--useWasmTailCalls=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

// The frame offsets these layouts are built around assume ARM64's eight argument GPRs.

const overlappingVectors = {
    params: "i64 i64 i64 i64 i64 i64 i64 i64 v128 v128 v128 v128 v128 v128 v128 v128 i64 v128 v128 v128 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64",
    calleeBody: `
    local.get 16
    i64.const 100
    i64.mul
    local.get 17
    i64x2.extract_lane 0
    i64.add
    i64.const 100
    i64.mul
    local.get 18
    i64x2.extract_lane 0
    i64.add
    i64.const 100
    i64.mul
    local.get 19
    i64x2.extract_lane 0
    i64.add
    `,
    callerBody: `
    (local $v0 v128) (local $v1 v128) (local $v2 v128)
    i64.const 1 i64x2.splat local.set $v0
    i64.const 2 i64x2.splat local.set $v1
    i64.const 3 i64x2.splat local.set $v2
    block
    end
    i64.const 0 i64.const 0 i64.const 0 i64.const 0
    i64.const 0 i64.const 0 i64.const 0 i64.const 0
    local.get $v0 local.get $v0 local.get $v0 local.get $v0
    local.get $v0 local.get $v0 local.get $v0 local.get $v0
    i64.const 9
    local.get $v0
    local.get $v1
    local.get $v2
    i64.const 0 i64.const 0 i64.const 0 i64.const 0
    i64.const 0 i64.const 0 i64.const 0 i64.const 0
    i64.const 0 i64.const 0 i64.const 0
    `,
    expected: 9010203n,
};

const twoPendingSpills = {
    params: "i64 i64 i64 i64 i64 i64 i64 i64 v128 v128 v128 v128 v128 v128 v128 v128 i64 v128 v128 v128 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64",
    calleeBody: `
    local.get 16
    i64.const 100
    i64.mul
    local.get 17
    i64x2.extract_lane 0
    i64.add
    i64.const 100
    i64.mul
    local.get 18
    i64x2.extract_lane 0
    i64.add
    i64.const 100
    i64.mul
    local.get 19
    i64x2.extract_lane 0
    i64.add
    `,
    callerBody: `
    (local $v0 v128) (local $v1 v128) (local $v2 v128) (local $v3 v128)
    i64.const 1 i64x2.splat local.set $v0
    i64.const 2 i64x2.splat local.set $v1
    i64.const 3 i64x2.splat local.set $v2
    i64.const 4 i64x2.splat local.set $v3
    block
    end
    i64.const 0 i64.const 0 i64.const 0 i64.const 0
    i64.const 0 i64.const 0 i64.const 0 i64.const 0
    local.get $v0 local.get $v0 local.get $v0 local.get $v0
    local.get $v0 local.get $v0 local.get $v0 local.get $v0
    i64.const 9
    local.get $v0
    local.get $v3
    local.get $v2
    i64.const 0 i64.const 0 i64.const 0 i64.const 0
    i64.const 0 i64.const 0 i64.const 0 i64.const 0
    i64.const 0 i64.const 0 i64.const 0 i64.const 0
    i64.const 0 i64.const 0 i64.const 0
    `,
    expected: 9010403n,
};

const registerDestinationsInCycle = {
    params: "i64 i64 i64 i64 i64 i64 i64 i64 v128 v128 v128 v128 v128 v128 v128 v128 i64 v128 v128 v128 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64",
    calleeBody: `
    local.get 16
    i64.const 100
    i64.mul
    local.get 8
    i64x2.extract_lane 0
    i64.add
    i64.const 100
    i64.mul
    local.get 9
    i64x2.extract_lane 0
    i64.add
    i64.const 100
    i64.mul
    local.get 17
    i64x2.extract_lane 0
    i64.add
    i64.const 100
    i64.mul
    local.get 18
    i64x2.extract_lane 0
    i64.add
    `,
    callerBody: `
    (local $v0 v128) (local $v1 v128) (local $v2 v128) (local $v3 v128)
    i64.const 1 i64x2.splat local.set $v0
    i64.const 2 i64x2.splat local.set $v1
    i64.const 3 i64x2.splat local.set $v2
    i64.const 4 i64x2.splat local.set $v3
    block
    end
    i64.const 0 i64.const 0 i64.const 0 i64.const 0
    i64.const 0 i64.const 0 i64.const 0 i64.const 0
    local.get $v3 local.get $v2 local.get $v0 local.get $v0
    local.get $v0 local.get $v0 local.get $v0 local.get $v0
    i64.const 9
    i64.const 5 i64x2.splat
    i64.const 6 i64x2.splat
    local.get $v1
    i64.const 0 i64.const 0 i64.const 0 i64.const 0
    i64.const 0 i64.const 0 i64.const 0 i64.const 0
    i64.const 0 i64.const 0 i64.const 0 i64.const 0
    i64.const 0 i64.const 0 i64.const 0
    `,
    expected: 904030506n,
};

const repeatedRegisterSource = {
    params: registerDestinationsInCycle.params,
    calleeBody: registerDestinationsInCycle.calleeBody,
    callerBody: registerDestinationsInCycle.callerBody.replace("local.get $v3 local.get $v2", "local.get $v3 local.get $v3"),
    expected: 904040506n,
};

const repeatedStackSource = {
    params: "i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64",
    calleeBody: `
    local.get 9
    i64.const 1000
    i64.mul
    local.get 10
    i64.add
    i64.const 1000
    i64.mul
    local.get 11
    i64.add
    i64.const 1000
    i64.mul
    local.get 12
    i64.add
    `,
    callerBody: `
    (local $x0 i64) (local $x1 i64) (local $x2 i64) (local $x3 i64)
    i64.const 1 local.set $x0
    i64.const 2 local.set $x1
    i64.const 3 local.set $x2
    i64.const 4 local.set $x3
    block
    end
    i64.const 0 i64.const 0 i64.const 0 i64.const 0
    i64.const 0 i64.const 0 i64.const 0 i64.const 0
    i64.const 0
    local.get $x2
    local.get $x0
    local.get $x3
    local.get $x2
    i64.const 0 i64.const 0 i64.const 0 i64.const 0
    i64.const 0 i64.const 0 i64.const 0 i64.const 0
    i64.const 0 i64.const 0 i64.const 0 i64.const 0
    i64.const 0
    `,
    expected: 3001004003n,
};

const features = { simd: true, tail_call: true };

async function instantiateCallers(testCase, kinds) {
    const type = `(type $t (func (param ${testCase.params}) (result i64)))`;
    const callee = `(func $callee (export "callee") (type $t) ${testCase.calleeBody})`;
    const caller = tailCall => `(func (export "caller") (result i64) ${testCase.callerBody} ${tailCall})`;
    const indirectTailCall = "i32.const 0 return_call_indirect (type $t)";

    let other;
    const instances = [];
    for (const kind of kinds) {
        if (kind == "import" || kind == "cross-instance-indirect")
            other ??= await instantiate(`(module ${type} ${callee})`, {}, features);
        switch (kind) {
        case "direct":
            instances.push(await instantiate(`(module ${type} ${callee} ${caller("return_call $callee")})`, {}, features));
            break;
        case "indirect":
            instances.push(await instantiate(`(module ${type} (table funcref (elem $callee)) ${callee} ${caller(indirectTailCall)})`, {}, features));
            break;
        case "import":
            instances.push(await instantiate(`(module ${type} (import "m" "callee" (func $callee (type $t))) ${caller("return_call $callee")})`, { m: other.exports }, features));
            break;
        case "cross-instance-indirect": {
            const table = new WebAssembly.Table({ initial: 1, element: "funcref" });
            table.set(0, other.exports.callee);
            instances.push(await instantiate(`(module ${type} (import "m" "table" (table 1 funcref)) ${caller(indirectTailCall)})`, { m: { table } }, features));
            break;
        }
        default:
            throw new Error(`unknown kind ${kind}`);
        }
    }
    return instances;
}

async function test() {
    // The call kind matters on the path that copies a source to a frame slot: an indirect call adds a
    // source addressed from SP, and a cross-instance call shifts the frame for a restore frame.
    const plan = [
        [overlappingVectors, ["direct"]],
        [twoPendingSpills, ["direct"]],
        [registerDestinationsInCycle, ["direct", "indirect", "import", "cross-instance-indirect"]],
        [repeatedRegisterSource, ["direct", "indirect"]],
        [repeatedStackSource, ["direct"]],
    ];
    const cases = [];
    for (const [testCase, kinds] of plan) {
        for (const instance of await instantiateCallers(testCase, kinds))
            cases.push({ instance, expected: testCase.expected });
    }
    for (let i = 0; i < wasmTestLoopCount; ++i) {
        for (const { instance, expected } of cases)
            assert.eq(instance.exports.caller(), expected);
    }
}

await assert.asyncTest(test());
