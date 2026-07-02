// FIXME: Consider making jump islands work with Options::jitMemoryReservationSize
// https://bugs.webkit.org/show_bug.cgi?id=209037
//@ skip if $architecture == "arm64" || $addressBits <= 32
// This test will never fail to compile a module with this option enabled.

import * as assert from '../assert.js'
import Builder from '../Builder.js'

const verbose = false;
const maxInstructionCount = 500;
const instancesTotal = 8;
const invocationsTotal = 8;
const tierUpCalls = 20000; // Call enough to trigger tier up and get it to compile.

// This test starts running with a few bytes of executable memory available. Try
// to create and instantiate a module which will fail to fit.

const randomProgram = instructionCount => {
    let b = new Builder()
        .Type().End()
        .Function().End()
        .Export()
            .Function("foo")
            .Function("bar")
        .End()
        .Code()
            .Function("foo", { params: [], ret: "f32" })
                .F32Const(2.0)
                .Return()
            .End()
            .Function("bar", { params: ["f32", "f32"], ret: "f32" })
              .GetLocal(0);

    // Insert a bunch of dependent instructions in a single basic block so that
    // our compiler won't be able to strength-reduce.
    const actions = [
        b => b.GetLocal(0).F32Sub(),
        b => b.GetLocal(1).F32Sub(),
        b => b.GetLocal(0).F32Add(),
        b => b.GetLocal(1).F32Add(),
        b => b.GetLocal(0).F32Mul(),
        b => b.GetLocal(1).F32Mul(),
    ];

    while (--instructionCount)
        b = actions[(Math.random() * actions.length) | 0](b);

    b = b.Return().End().End();

    return b.WebAssembly().get();
}

let failCount = 0;
let callCount = 0;
let instances = [];

const invoke = (instance, count) => {
    if (verbose)
        print(`Invoking`);
    for (let i = 0; i < count; ++i)
        assert.eq(instance.exports["foo"](), 2.0);
    for (let i = 0; i < count; ++i)
        instance.exports["bar"](2.0, 6.0);
    ++callCount;
};

while (failCount === 0 && callCount < 100) {
    const instructionCount = (Math.random() * maxInstructionCount + 1) | 0;

    if (verbose)
        print(`Trying module with ${instructionCount} instructions.`);

    const buf = randomProgram(instructionCount);
    let module;

    try {
        module = new WebAssembly.Module(buf);
    } catch (e) {
        if (e instanceof WebAssembly.CompileError) {
            if (verbose)
                print(`Caught: ${e}`);
            ++failCount;
        }
        else
            throw new Error(`Expected a WebAssembly.CompileError, got ${e}`);
    }

    if (module !== undefined) {
        if (verbose)
            print(`Creating instance`);

        let instance;
        try {
            instance = new WebAssembly.Instance(module);
        } catch (e) {
            if (e instanceof WebAssembly.LinkError) {
                if (verbose)
                    print(`Caught: ${e}`);
                ++failCount;
            }
            else
                throw new Error(`Expected a WebAssembly.LinkError, got ${e}`);
        }

        if (instance !== undefined) {
            instances.push(instance);
            invoke(instance, 1);
        }
    }
}

if (callCount === 0)
    throw new Error(`Expected to be able to allocate a WebAssembly module, instantiate it, and call its exports at least once`);

// Make sure we can still call all the instances we create, even after going
// OOM. This will try to force tier-up as well, which should fail.

if (verbose)
    print(`Invoking all previously created instances`);

for (let instance of instances)
    invoke(instance, tierUpCalls);

// Do it twice to revisit what should have gotten tiered up.
for (let instance of instances)
    invoke(instance, 1);
