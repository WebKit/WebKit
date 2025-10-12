//@ skip unless $isSIMDPlatform
//@ $skipModes << :lockdown
//@ requireOptions("--useWasmSIMD=1")
//@ requireOptions("--useExecutableAllocationFuzz=false")

// The purpose of this test is to compare SIMD and non-SIMD versions of this code.
// Sample run:
// $ jsc -m exceptions-simd.js --useConcurrentJIT=0 --useConcurrentGC=0 --forceAllFunctionsToUseSIMD=1
// Vector runtime with float as baseline:  1.0006938427253944 times as long
// $ jsc -m exceptions-simd.js --useConcurrentJIT=0 --useConcurrentGC=0 --forceAllFunctionsToUseSIMD=0
// Vector runtime with float as baseline:  1.016849616584845 times as long

// This measures how costly the vector version of the MASM trampoline is. When OMG is disabled, this
// also measures the expected regression from treating every float as a vector in BBQ.

// We also use this test case to manually inspect the quality of the register allocation. For example,
// this is bad for OMG:
/*
Generated OMG code for WebAssembly OMG function[2] () -> [F32] name <?>.wasm-function[2]
...
Air        MoveFloat %q0, -44(%fp), b@11
[JSC::Wasm::B3IRGenerator::createCallPatchpoint|JSC::Wasm::B3IRGenerator::addCall]
b3    Void b@16 = Patchpoint($500(b@13):Register(%x0), b@11:LateColdAny, generator = 0x1150e1680, earlyClobbered = [%x16, %x17], lateClobbered = [%x0, %x1, %x2, %x3, %x4, %x5, %x6, %x7, %x8, %x9, %x10, %x11, %x12, %x13, %x14, %x15, %x16, %x17, %q0, %q1, %q2, %q3, %q4, %q5, %q6, %q7, %q8↑, %q9↑, %q10↑, %q11↑, %q12↑, %q13↑, %q14↑, %q15↑, %q16, %q17, %q18, %q19, %q20, %q21, %q22, %q23, %q24, %q25, %q26, %q27, %q28, %q29, %q30, %q31], usedRegisters = [%fp], resultConstraints = WarmAny, ExitsSideways|ControlDependent|WritesPinned|ReadsPinned|Fence|Writes:Top|Reads:Top, Wasm: {opcode: Call, location: 0xd})
Air        Patch &Patchpoint0, %x0, -44(%fp), b@16
*/
// We should instead allocate b@11 in %q8 when it is a float or double.

function test() {
    const tag = new WebAssembly.Tag({ parameters: ["i32"] });
    const instanceSIMD = new WebAssembly.Instance(new WebAssembly.Module(read("exceptions-simd.wasm", "binary")), { m: { tag } })
    const test_throw_simd = instanceSIMD.exports.test_throw, test_catch_simd = instanceSIMD.exports.test_catch

    const instanceNoSIMD = new WebAssembly.Instance(new WebAssembly.Module(read("exceptions-simd-float.wasm", "binary")), { m: { tag } })
    const test_throw_no_simd = instanceNoSIMD.exports.test_throw, test_catch_no_simd = instanceNoSIMD.exports.test_catch

    for (let i = 0; i < 5000; ++i) {
        try {
            test_throw_simd(42)
            throw new Error("should be caught")
        } catch (e) {
            if (!e.is(tag) || e.getArg(tag, 0) != 42)
                throw new Error("Test failed")
        }
        if (test_catch_simd() != 1337)
            throw new Error("Test failed")
    }

    for (let i = 0; i < 5000; ++i) {
        try {
            test_throw_no_simd(42)
            throw new Error("should be caught")
        } catch (e) {
            if (!e.is(tag) || e.getArg(tag, 0) != 42)
                throw new Error("Test failed")
        }
        if (test_catch_no_simd() != 1337)
            throw new Error("Test failed")
    }

    fullGC()

    let start = 0, simdTime = 0.0, floatTime = 0.0

    start = preciseTime()
    for (let i = 0; i < 1000000; ++i) {
        if (test_catch_simd() != 1337)
            throw new Error("Test failed")
    }
    simdTime += preciseTime() - start

    fullGC()

    start = preciseTime()
    for (let i = 0; i < 1000000; ++i) {
        if (test_catch_no_simd() != 1337)
            throw new Error("Test failed")
    }
    floatTime += preciseTime() - start

    fullGC()

    start = preciseTime()
    for (let i = 0; i < 1000000; ++i) {
        if (test_catch_simd() != 1337)
            throw new Error("Test failed")
    }
    simdTime += preciseTime() - start

    fullGC()

    start = preciseTime()
    for (let i = 0; i < 1000000; ++i) {
        if (test_catch_no_simd() != 1337)
            throw new Error("Test failed")
    }
    floatTime += preciseTime() - start

    // print("Vector runtime with float as baseline: ", simdTime / floatTime, "times as long: SIMD took ", Math.round(simdTime*1000)/1000, " vs ", Math.round(floatTime*1000)/1000)

}
if (typeof WebAssembly === "object")
    test()
