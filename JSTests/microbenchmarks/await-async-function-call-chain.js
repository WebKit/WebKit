// Awaiting a chain of distinct async functions makes each microtask resume a
// different function, stressing the microtask call cache in the drain loop.

async function d5() { return 42; }
async function d4() { return await d5(); }
async function d3() { return await d4(); }
async function d2() { return await d3(); }
async function d1() { return await d2(); }

async function run() {
    let acc = 0;
    for (let i = 0; i < 1e5; i++)
        acc += await d1();
    return acc;
}

run();
drainMicrotasks();
