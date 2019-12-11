import * as assert from '../assert.js';

const module = Uint8Array.of(0x0, 0x61, 0x73, 0x6d, 0x1, 0x00, 0x00, 0x00);

let promises = [];

const runNearStackLimit = f => {
    const t = () => {
        try {
            return t();
        } catch (e) {
            return f();
        }
    };
    return t();
};

const touchArgument = arg => promises.push(arg);

const compileMe = () => touchArgument(WebAssembly.compile(module));

async function testCompile() {
    await touchArgument(async function() {
        runNearStackLimit(compileMe);
    }());
}

const instantiateMe = () => touchArgument(WebAssembly.instantiate(module));

async function testInstantiate() {
    await touchArgument(async function() {
        runNearStackLimit(instantiateMe);
    }());
}

assert.asyncTest(testCompile());
assert.asyncTest(testInstantiate());
