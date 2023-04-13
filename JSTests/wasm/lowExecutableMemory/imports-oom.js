// FIXME: Consider making jump islands work with Options::jitMemoryReservationSize
// https://bugs.webkit.org/show_bug.cgi?id=209037
//@ skip

import * as assert from '../assert.js'
import Builder from '../Builder.js'

const verbose = false;
const numFunctions = 2;
const maxParams = 8;

// This test starts running with a few bytes of executable memory available. Try
// to create and instantiate modules which have way more imports than anything
// else. Hopefully they'll fail when trying to instantiate their entrypoints.

const type = () => {
    const types = ["i32", "f32", "f64"];
    return types[(Math.random() * types.length) | 0];
};

const params = () => {
    let p = [];
    let count = (Math.random() * maxParams) | 0;
    while (count--)
        p.push(type());
    return p;
};

const randomProgram = () => {
    let b = new Builder()
        .Type().End()
        .Import();
    const ps = params();
    for (let f = 0; f < numFunctions; ++f)
        b = b.Function("imp", `${f}`, { params: ps });
    b = b.End()
        .Function().End()
        .Export();
    for (let f = 0; f < numFunctions; ++f)
        b = b.Function(`call${f}`);
    b = b.End()
        .Code();
    for (let f = 0; f < numFunctions; ++f) {
        b = b.Function(`call${f}`, { params: ps });
        for (let p = 0; p < ps.length; ++p)
            b = b.GetLocal(p);
        b = b.Call(f).End();
    }
    b = b.End();
    return b.WebAssembly().get();
}

let failCount = 0;
let callCount = 0;
let instances = [];

let imports = [];
for (let f = 0; f < numFunctions; ++f)
    imports.push((...args) => {
        if (verbose)
            print(`Invoked ${f} with: ${args}`);
        ++callCount;
    });

const invoke = instance => {
    let result = 0;
    for (let f = 0; f < numFunctions; ++f) {
        const name = `call${f}`;
        if (verbose)
            print(`Invoking ${name}`);
        result += instance.exports[name]();
    }
    return result;
};

while (failCount === 0) {
    if (verbose)
        print(`Trying...`);

    const buf = randomProgram();
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
            instance = new WebAssembly.Instance(module, { imp: imports });
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
            invoke(instance);
        }
    }
}

if (callCount === 0)
    throw new Error(`Expected to be able to allocate a WebAssembly module, instantiate it, and call its exports at least once`);

// Make sure we can still call all the instances we create, even after going OOM.

if (verbose)
    print(`Invoking all previously created instances`);

for (let instance of instances)
    invoke(instance);
